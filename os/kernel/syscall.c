#include "defs.h"
#include "fcntl.h"
#include "proc.h"
#include "syscall_ids.h"
#include "trap.h"
#include "riscv.h"
#include "fs.h"

#define min(a, b) a < b ? a : b;

uint64 console_write(uint64 va, uint64 len) {
    struct proc *p = curr_proc();
    char str[200];
    int size = copyinstr(p->pagetable, str, va, MIN(len, 200));
    for(int i = 0; i < size; ++i) {
        console_putchar(str[i]);
    }
    return size;
}

uint64 console_read(uint64 va, uint64 len) {
    struct proc *p = curr_proc();
    char str[200];
    for(int i = 0; i < MIN(len, 200); ++i) {
        int c = console_getchar();
        str[i] = c;
    }
    copyout(p->pagetable, va, str, len);
    return len;
}

uint64 sys_write(int fd, uint64 va, uint64 len) {
    if(fd > 15) return -1;
    if(fd <= 2) return console_write(va, len);



    struct proc *p = curr_proc();
    struct file *f = p->files[fd];
    if (f->type == FD_PIPE) {
        info("write to pipe at %p\n", f->pipe);
        return pipewrite(f->pipe, va, len);
    } else if (f->type == FD_INODE) {
        return filewrite(f, va, len);
    }    
    error("unknown file type %d\n", f->type);
    panic("syswrite: unknown file type\n");
    return -1;
}

uint64 sys_read(int fd, uint64 va, uint64 len) {
    if (fd == 0) { 
        return console_read(va, len);
    }
    struct proc *p = curr_proc();
    struct file *f = p->files[fd];
    if (f->type == FD_PIPE) {
        info("read to pipe at %p\n", f->pipe);
        return piperead(f->pipe, va, len);
    } else if (f->type == FD_INODE) {
        return fileread(f, va, len);
    }
    error("unknown file type %d\n", f->type);
    panic("sysread: unknown file type\n");
    return -1;
}

uint64 sys_mailread(void *buf, int len) {
    return mail_read(buf, len);
}

uint64 sys_mailwrite(int pid, void *buf, int len) {
    struct proc *p = get_proc(pid);
    if (p == 0) return -1;
    return mail_write(p, buf, len);
}

uint64 sys_pipe(uint64 fdarray) {
     info("init pipe\n");
    struct proc *p = curr_proc();
    uint64 fd0, fd1;
    struct file *f0, *f1;
    if (f0 < 0 || f1 < 0) {
        return -1;
    }
    f0 = filealloc();
    f1 = filealloc();
    if (pipealloc(f0, f1) < 0)
        return -1;
    fd0 = fdalloc(f0);
    fd1 = fdalloc(f1);
    if (copyout(p->pagetable, fdarray, (char *) &fd0, sizeof(fd0)) < 0 ||
        copyout(p->pagetable, fdarray + sizeof(uint64), (char *) &fd1, sizeof(fd1)) < 0) {
        fileclose(f0);
        fileclose(f1);
        p->files[fd0] = 0;
        p->files[fd1] = 0;
        return -1;
    }
    return 0;
}

uint64 sys_exit(int code) {
    exit(code);
    return 0;
}

uint64 sys_sched_yield() {
    yield();
    return 0;
}

uint64 sys_getpid() {
    return curr_proc()->pid;
}

uint64 sys_clone() {
    // info("fork!\n");
    return fork();
}

uint64 sys_exec(uint64 va) {
    struct proc* p = curr_proc();
    char name[200];
    copyinstr(p->pagetable, name, va, 200);
    info("sys_exec %s\n", name);
    return exec(name);
}

uint64 sys_wait(int pid, uint64 va) {
    struct proc *p = curr_proc();
    int *code = (int *) useraddr(p->pagetable, va);
    return wait(pid, code);
}

uint64 sys_times() {
    uint64 time = get_time_ms();
    return time;
}

uint64 sys_close(int fd) {
    if (fd == 0)
        return 0;
    struct proc *p = curr_proc();
    struct file *f = p->files[fd];
    fileclose(f);
    p->files[fd] = 0;
    return 0;
}

uint64 sys_openat(uint64 va, uint64 omode, uint64 _flags) {
    struct proc *p = curr_proc();
    char path[200];
    copyinstr(p->pagetable, path, va, 200);
    return fileopen(path, omode);
}

uint64 sys_setpriority(int code) {
    uint64 rtn = set_priority(code);
    return rtn;
}

uint64 sys_get_time(uint64 ts, uint64 tz){
    // uint64 rtn = get_time(ts,0);
    struct proc *p = curr_proc();
    uint64 physical_addr = useraddr(p->pagetable,ts);
    uint64 rtn = get_time((TimeVal *)physical_addr,0);
    return rtn;
}

uint64 sys_mmap(uint64 start, uint64 len, uint64 port){
    // if(((port&0x8)!=0) || ((port&0x7)==0)){
    //     return -1;
    // }
    // port between 1 and 7
    if(port < 1 || port > 7){
        return -1;
    }
    // make len multiple of 4096    
    if(len%4096!=0){
        while(len%4096!=0){
            len++;
        }    
    }    

    // get current process page
    struct proc *p = curr_proc();

    // first check if useraddr can retrieve it
    if (start%4096 != 0) return -1;
    // if(useraddr(p->pagetable, start) != 0) return -1;
 
    port = port << 1;
    port = port | PTE_U;
    port = port | PTE_V;       


    int mmap =0;
    for(int i=0;i<len/PGSIZE;i++) {
        uint64 physical_addr = (uint64) kalloc();
        mmap = mappages(p->pagetable, start+i*PGSIZE, PGSIZE, physical_addr, port);
    }

    if(mmap == 0) return len;
    else return -1;
}

uint64 sys_munmap(uint64 start, uint64 len) {
    if(start%4096!=0)
        return -1;

    if(len%4096!=0){
        while(len%4096!=0){
            len++;
        }    
    }
    pte_t *pte;
    uint64 a;
    struct proc *p = curr_proc();
    for (a = start; a < start + (len/PGSIZE) * PGSIZE; a += PGSIZE) {
        pte = walk(p->pagetable, a, 0);
        if ((*pte & PTE_V) == 0){
            return -1; //not mapped, return -1
        }
    }

    uvmunmap(p->pagetable,start,len/PGSIZE,1);
    return len;
}

uint64 sys_spawn(uint64 va){
    struct proc* p = curr_proc();
    char name[200];
    copyinstr(p->pagetable, name, va, 200);
    uint64 sp = spawn(name);
    return sp;
}

uint64 sys_linkat(uint64 olddirfd, char* oldpath_, uint64 newdirfd, char* newpath_, uint64 flags){
    // info("into function linkat\n");

    char oldpath[DIRSIZ], newpath[DIRSIZ];
    pagetable_t pagetable = curr_proc()->pagetable;
    copyin(pagetable, oldpath, (uint64)oldpath_, DIRSIZ);
    copyin(pagetable, newpath, (uint64)newpath_, DIRSIZ);

    struct inode *ip, *dp;
    dp = root_dir();
    if(strncmp(oldpath, newpath, DIRSIZ) == 0){
        warn("linkat: oldpath == newpath\n");
        return -1;
    }
    if ((ip = dirlookup(dp, oldpath, 0)) == 0){
        warn("linkat : dirlookup\n");
        return -1;
    }
    if(dirlink(dp, newpath, ip->inum) < 0){
        warn("linkat : dirlink\n");
        return -1;
    }
    return 0;
}

uint64 sys_unlinkat(uint64 dirfd, char* path_, uint64 flags) {
    info("into function unlinkat\n");
    char path[DIRSIZ];
    pagetable_t pagetable = curr_proc()->pagetable;
    copyin(pagetable, path, (uint64)path_, DIRSIZ);

    struct inode *ip, *dp;
    dp = root_dir();
    if ((ip = dirlookup(dp, path, 0)) == 0){
        warn("unlinkat : dirlookup\n");
        return -1;
    }
    if(dirunlink(dp, path) < 0){
        warn("unlinkat : dirunslink\n");
        return -1;
    }
    return 0;
}

uint64 sys_fstat(uint64 fd, struct Stat* st){
    // info("into function fstat\n");
    // info("fd = %d\n", fd);
    if(fd < 0 || fd >= 16){
        warn("fd out of range\n");
        return -1;
    }
    struct inode *ip, *dp;
    dp = root_dir();
    struct proc* p = curr_proc();
    ip = p->files[fd]->ip;
    // Look for link number.
    int nlink = 0;
    int off;
    struct dirent de;
    for(off = 0; off < dp->size; off += sizeof(de)){
        if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)){
            warn("fstat readi\n");
            return -1;
        }
        if(de.inum == ip->inum) {
            nlink++;
        }
    }
    struct Stat sst;

    sst.dev = ip->dev;
    sst.ino = ip->inum;
    if(ip->type == T_DIR)
        sst.mode = DIR;
    else if(ip->type == T_FILE)
        sst.mode = FILE;
    else{
        warn("no such file mode exit\n");
        return -1;
    }
    sst.nlink = nlink;
    if(copyout(p->pagetable, (uint64)st, (char*)&sst, sizeof(sst)) < 0){
        return -1;
    }
    return 0;
}

void syscall() {
    struct proc *p = curr_proc();
    struct trapframe *trapframe = p->trapframe;
    int id = trapframe->a7, ret;
    uint64 args[7] = {trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6};
    trace("syscall %d args:%p %p %p %p %p %p %p\n", id, args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    switch (id) {
        case SYS_write:
            ret = sys_write(args[0], args[1], args[2]);
            break;
        case SYS_read:
            ret = sys_read(args[0], args[1], args[2]);
            break;
        case SYS_openat:
            ret = sys_openat(args[0], args[1], args[2]);
            break;
        case SYS_close:
            ret = sys_close(args[0]);
            break;
        case SYS_exit:
            ret = sys_exit(args[0]);
            break;
        case SYS_sched_yield:
            ret = sys_sched_yield();
            break;
        case SYS_getpid:
            ret = sys_getpid();
            break;
        case SYS_clone:// SYS_fork
            ret = sys_clone();
            break;
        case SYS_execve:
            ret = sys_exec(args[0]);
            break;
        case SYS_wait4:
            ret = sys_wait(args[0], args[1]);
            break;
        case SYS_times:
            ret = sys_times();
            break;
        case SYS_pipe2:
            ret = sys_pipe(args[0]);
            break;
        case SYS_setpriority:
            ret = sys_setpriority(args[0]);
            break;
        case SYS_gettimeofday:
            ret = sys_get_time(args[0],0);
            // ret=-1;
            break;
        case SYS_mmap:
            ret = sys_mmap(args[0],args[1],args[2]);
            break;
        case SYS_munmap:
            ret = sys_munmap(args[0],args[1]);
            break;
        case SYS_spawn:
            ret = sys_spawn(args[0]);
        case SYS_mailread:
            ret = sys_mailread((void*)args[0],args[1]);
            // printf("ret in mail_read is:%d\n",ret);
            break;
        case SYS_mailwrite:
            // printf("in here");
            // printf("%p\n",args[0]);
            // printf("%p\n",args[1]);
            // printf("%p\n",args[2]);

            ret = sys_mailwrite(args[0],(void*)args[1],args[2]);
            // printf("ret in mail_write is:%d\n",ret);
            break;
        case SYS_linkat:
            // printf("%p, %p, %p, %p\n", args[0], args[1], args[2], args[3]);
            ret = sys_linkat(args[0], (char*)args[1], args[2], (char*)args[3], args[4]);
            // ret = 0;
            break;

        case SYS_fstat: 
            // printf("%p, %p\n", args[0], args[1]);
            ret = sys_fstat(args[0],(struct Stat*)args[1]);
            // ret = 0;
            break;
        case SYS_unlinkat: 
            // printf("%p, %p, %p\n", args[0] , args[1], args[2]);
            ret = sys_unlinkat(args[0], (char*)args[1], args[2]);
            // ret = 0;
            break;
        default:
            ret = -1;
            warn("unknown syscall %d\n", id);
    }
    trapframe->a0 = ret;
    trace("syscall ret %d\n", ret);
}
