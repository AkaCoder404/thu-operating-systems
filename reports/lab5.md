# 操作系统 lab5

## 实验要求

1. 实现分支：ch5。

2. 实现进程控制，可以运行 usershell。

3. 实现自定义系统调用 spawn，并通过 [C测例](https://github.com/DeathWish5/riscvos-c-tests)中chapter5对应的所有测例。

## 实验结果

In this experiment, we had to implement a more flexible process control system. In order to realize such a system, we had to implement 4 new system call functions, including 

- sys_read: read bytes from the input (where we input the app name)
- sys_fork: create process that is the same as the current process
- sys_exec:  modify the current process to execute the specified program from scratch.
- sys_wait: wait for one or any of the subprocesses to end and get its exit_ code

After implementing these function located in ```syscall.c``` , and ```make all CHAPTER=5``` in /user, and ```make run``` in /kernel. 

![image-20210419160318692](lab5.assets/image-20210419160318692.png)

If we run one of the chapter 5 tests, such as ```ch5_usertest.bin``` , we get

![image-20210419160028343](lab5.assets/image-20210419160028343.png)



## 问答作业

1. fork + exec 的一个比较大的问题是 fork 之后的内存页/文件等资源完全没有使用就废弃了，针对这一点，有什么改进策略？

   【解答】

   On one hand, we can utilize ```spawn``` instead of ```fork``` + ```exec```

   On the other hand we can adopt ```copy on write (COW)```. Meaning, only necessary data can be copied when ```fork``` occurs, and resources such as memory pages/files can be copied when the behavior ofof changing the corresponding segment occurs in the parent and child processes.

2. 其实使用了题1的策略之后，fork + exec 所带来的无效资源的问题已经基本被解决了，但是近年来 fork 还是在被不断的批判，那么到底是什么正在"杀死"fork？可以参考[论文](https://www.microsoft.com/en-us/research/uploads/prod/2019/04/fork-hotos19.pdf)，**注意**：回答无明显错误就给满分，出这题只是想引发大家的思考，完全不要求看论文，球球了，别卷了。

   【解答】

   - if ```fork``` uses ```COW``` technology, extracurricular resources are needed to monitor and process the COW process
   - ```fork``` may use security risks

3. fork 当年被设计并称道肯定是有其好处的。请使用**带初始参数**的 spawn 重写如下 fork 程序，然后描述 fork 有那些好处。注意:使用"伪代码"传达意思即可，spawn接口可以自定义。可以写多个文件。

   ```
   int main() {
       int a = get_a();
       if(fork() == 0) {
           int b = get_b();
           printf("a + b = %d", a + b);
           exit(0);
       }
       printf("a = %d", a);
       return 0;
   }
   // child_program
   int main() {
   	let b = get_b();
   	printf("a + b = %d", a + b);
   	return 0;
   }
   ```

4. 描述进程执行的几种状态，以及 fork/exec/wait/exit 对与状态的影响。

   ```
   fork : child process is set as READY, parent is set as RUNNING
   exec : set as RUNNING
   wait : set as READY
   exit : set as ZOMBIE
   ```

   