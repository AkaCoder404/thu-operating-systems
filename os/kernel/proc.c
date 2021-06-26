#include "defs.h"
#include "proc.h"
#include "trap.h"
#include "riscv.h"
#include "file.h"

#include "memory_layout.h"


struct proc pool[NPROC];
__attribute__ ((aligned (16))) char kstack[NPROC][KSTACK_SIZE];
__attribute__ ((aligned (4096))) char ustack[NPROC][PAGE_SIZE];
char trapframe[NPROC][PAGE_SIZE];
extern char trampoline[];


extern char boot_stack_top[];
struct proc* current_proc = 0;
struct proc idle;
int curr_pid = 0;

struct proc* get_proc(int pid) {
    struct proc *p;
    for(p = pool; p < &pool[NPROC]; p++) {
        if(p->pid == pid)
            return p;
    }
    return 0;
}

struct proc* curr_proc() {
    if(current_proc == 0)
        return &idle;
    return current_proc;
}

void
procinit(void)
{
    struct proc *p;
    for(p = pool; p < &pool[NPROC]; p++) {
        p->state = UNUSED;
        p->kstack = (uint64)kstack[p - pool];
        p->ustack = (uint64)ustack[p - pool];
        p->trapframe = kalloc();
        p->pid=0;
    }
    idle.kstack = (uint64)boot_stack_top;
    idle.pid = 0;
    idle.killed = 0;
}

int allocpid() {
    static int PID = 1;
    return PID++;
}
pagetable_t
proc_pagetable(struct proc *p)
{
    pagetable_t pagetable;

    // An empty page table.
    pagetable = uvmcreate();
    if(pagetable == 0)
        panic("");

    if(mappages(pagetable, TRAMPOLINE, PGSIZE,
                (uint64)trampoline, PTE_R | PTE_X) < 0){
        uvmfree(pagetable, 0);
        return 0;
    }

    memset(p->trapframe, 0, sizeof(struct trapframe));

    // map the trapframe just below TRAMPOLINE, for trampoline.S.
    if(mappages(pagetable, TRAPFRAME, PGSIZE,
                (uint64)(p->trapframe), PTE_R | PTE_W) < 0){;
        panic("");
    }

    return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0);
    uvmfree(pagetable, sz);
}

static void
freeproc(struct proc *p)
{
    // if(p->trapframe)
    //     kfree((void*)p->trapframe);
    // p->trapframe = 0;
    if(p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;
    p->state = UNUSED;
    for(int i = 0; i < FD_MAX; ++i) {
        if(p->files[i] != 0) {
            fileclose(p->files[i]);
            p->files[i] = 0;
        }
    }
}

struct proc* allocproc(void)
{
    struct proc *p;
    for(p = pool; p < &pool[NPROC]; p++) {
        if(p->state == UNUSED) {
            goto found;
        }
    }
    return 0;

    found:
    p->pid = allocpid();
    p->state = USED;
    p->sz = 0;
    p->exit_code = -1;
    p->parent = 0;
    p->ustack = 0;
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
        panic("");
    }
    memset(&p->context, 0, sizeof(p->context));
    memset(p->trapframe, 0, PAGE_SIZE);
    memset((void*)p->kstack, 0, KSTACK_SIZE);
    p->context.ra = (uint64)usertrapret;
    p->context.sp = p->kstack + KSTACK_SIZE;
    p->prio = 16; //default prio
    p->pass=INT_MAX/16;
    p->stride = 0;
    return p;
}

void
scheduler(void)
{
    // for(;;){
    //     struct proc *p;
    //     struct proc *chosen=0;
    //     // printf("here");
    //     for(p = pool; p < &pool[NPROC]; p++) {
    //         // printf("here2");
    //         if(p->state == RUNNABLE && 
    //         (!chosen||p->stride < chosen->stride)) {
    //             chosen = p;
    //             // printf("chosen");
    //         }
    //     }
    //     chosen->state = RUNNING;
    //     chosen->stride+=chosen->pass; //将对应的 stride 加上其对应的步长 pass
    //     current_proc = chosen;
    //     if(current_proc->stride >= 500*(chosen->pass)){
    //         exit(-1);
    //         continue;
    //     }
    //     swtch(&idle.context, &chosen->context);
    // }
    struct proc *p;
    for(;;){
        int all_done = 1;
        for(p = pool; p < &pool[NPROC]; p++) {
            if(p->state == RUNNABLE) {
                all_done = 0;
                p->state = RUNNING;
                current_proc = p;
                curr_pid = p->pid;
                trace("switch to next proc %d\n", p->pid);
                swtch(&idle.context, &p->context);
            }
        }
        if(all_done)
            panic("all apps over\n");
    }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
    struct proc *p = curr_proc();
    if(p->state == RUNNING)
        panic("sched running");
    current_proc = &idle;
    swtch(&p->context, &idle.context);
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    struct proc *p = curr_proc();
    p->state = RUNNABLE;
    sched();
}

int
fork(void)
{
    int pid;
    struct proc *np;
    struct proc *p = curr_proc();
    // Allocate process.
    if((np = allocproc()) == 0){
        np = curr_proc();
        // panic("allocproc\n");
    }
    // Copy user memory from parent to child.
    if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
        return np->pid;
        // panic("uvmcopy\n");
    }
    np->sz = p->sz;

    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);
    
    for(int i = 0; i < FD_MAX; ++i)
        if(p->files[i] != 0 && p->files[i]->type != FD_NONE) {
            p->files[i]->ref++;
            np->files[i] = p->files[i];
        }

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;
    pid = np->pid;
    np->parent = p;
    np->state = RUNNABLE;
    // printf("in fork np pid is: %d \n",pid);
    // printf("in fork parent pid is: %d\n",np->parent->pid);

    return pid;
}

int exec(char* name) {
    int id = get_id_by_name(name);
    if(id < 0)
        return -1;
    struct proc *p = curr_proc();
    proc_freepagetable(p->pagetable, p->sz);
    p->sz = 0;
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
        panic("");
    }
    loader(id, p);
    return 0;
}

int
wait(int pid, int* code)
{
    struct proc *np;
    int havekids;
    struct proc *p = curr_proc();
    // printf("current proc is %d\n",p->pid);
    for(;;){
        // Scan through table looking for exited children.
        havekids = 0;
        // printf("in wait pid is: %d\n",pid);
        // printf("current proc is %d\n",p);
        for(np = pool; np < &pool[NPROC]; np++){
            // printf("%d",np->state != UNUSED);
            // printf("%d\n",np->parent == p);
            // printf("%d",(pid <= 0 || np->pid == pid));
            // printf("\n");
            // if(np->parent!=0)
            //     printf("parent pid is %d\n", np->parent->pid);
            // if(np->pid!=0){
            //     printf("np pid %d\n", np->pid);
            //     printf("\n");
            // }
            // printf("%d %d %d\n",np->state != UNUSED,np->parent == p,(pid <= 0 || np->pid == pid));
            if(np->state != UNUSED && np->parent == p && (pid <= 0 || np->pid == pid)){
                havekids = 1;
                // printf("there is a kid\n");
                if(np->state == ZOMBIE){
                    // Found one.
                    // printf("FOUND A ZOMBIE! %d\n", np->pid);
                    np->state = UNUSED;
                    pid = np->pid;
                    *code = np->exit_code;
                    return pid;
                }
            }
        }
        if(!havekids){
            return -1;
        }
        p->state = RUNNABLE;
        sched();
    }
}

uint64 spawn(char* name){
    uint64 pid;
    struct proc *np;
    struct proc *p = curr_proc();
    // Allocate process.
    if((np = allocproc()) == 0){
        panic("allocproc\n");
    }
    // Copy user memory from parent to child.

    // Cause fork to return 0 in the child.
    pid = np->pid;
    // printf("in spawn np pid is: %d \n",pid);
    np->parent = p;
    // printf("in spawn parent pid is: %d\n",np->parent->pid);
    np->state = RUNNABLE;   
    //exec
    info("sys_exec %s\n", name);
    int id = get_id_by_name(name);
    if(id < 0)
        return -1;
    loader(id, np);
    
    return pid;

}

void exit(int code) {
    struct proc *p = curr_proc();
    p->exit_code = code;
    info("proc %d exit with %d\n", p->pid, code);
    freeproc(p);
    if(p->parent != 0) {
        warn("in exit -> set it to ZOMBIE %d\n", p->pid);
        warn("wait for parent to clean\n");
        p->state = ZOMBIE;
    } 
    sched();
}

int fdalloc(struct file* f) {
    struct proc* p = curr_proc();
    for(int i = 3; i < FD_MAX; ++i) {
        if(p->files[i] == 0) {
            p->files[i] = f;
            return i;
        }
    }
    return -1;
}

int cpuid() {
    return 0;
}

uint64 set_priority(int code) {
    struct proc *p = curr_proc();
    // printf("code is %d",code);
    if(code>=2&&code<=2147483647){
        p->prio=code;
        p->pass = INT_MAX/p->prio;
        return code;
    }
    else{
        return -1;
    }
}

uint64 get_time(TimeVal* ts, int tz) {
    ts->sec = get_cycle()/12500000;
    ts->usec = (get_cycle()%12500000)*10/125;
    return 0;
}

struct proc *get_proc_by_id(int pid) {
    struct proc *p;
    for(p = pool; p < &pool[NPROC]; p++) {
        if (p->state != UNUSED&& p->pid == pid)
            return p;
    }
    return 0;
}

int mail_read(void *buf, int len) {
    struct proc *p = curr_proc();
    struct Mailbox *mbox = &p->mailbox;
    if (mbox->tail == mbox->head) return -1;
    if (len == 0) return 0;

    if (len > mbox->len[mbox->head]) len = mbox->len[mbox->head];
    int ret = copyout(p->pagetable, (uint64)buf, mbox->mails[mbox->head], len);
    if (ret == -1) return -1;
    mbox->head = (1 + mbox->head) % 17;
    return len;
}

int mail_write(struct proc *p, void *buf, int len) {
    struct Mailbox *mbox = &p->mailbox;
    if ((1 + mbox->tail) % 17 == mbox->head) return -1;
    if (len == 0) return 0;

    if (len > 256) len = 256;
    mbox->len[mbox->tail] = len;
    int ret = copyin(curr_proc()->pagetable, mbox->mails[mbox->tail], (uint64)buf, len);
    if (ret == -1) return -1;
    mbox->tail = (1 + mbox->tail) % 17;
    return len;

}