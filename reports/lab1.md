# Lab 1

## 实验目的

1. 实现分支： ch1
2. 完成饰演指导书的内容，实现 hello world 输出
3. 实现彩色输出宏
4. 输出os内存空间布局，输出等级为INFO

## 实验过程

This first lab was relatively easy. The goal of the lab was to realize a logger system that would make it easier to debug. We had to implement 5 levels of output: error, warn, info, and debug, each of which having their own color, tag, and pid. 

I used C's enum functionality to handle the ASCII color values, as well as handle the different levels of the logger. I created five functions, each of them represent one of the different levels of output. For example, the ```info``` function:

```
void info(char str[]) { logger(INFO, str)}
```

where ```str``` represents the message, INFO, the level of output, and. The logger function controls which levels of outputs that are shown.

```
void logger(int level, char message[]) {
    if (debug_level <= level)
        printf("\x1b[%dm[%s][%d]  %s\x1b[0m\n", COLOR[level], LEVEL[level], curr_pid, message);
}
```

If the ```debug_level``` is smaller than the level of the output type, ```info``` for example, then all ```info``` outputs will be printed. The result of this simple system is as shown.

![image-20210307223707919](/home/akashili17/.config/Typora/typora-user-images/image-20210307223707919.png)

## 问答作业

1. 为了方便 os 处理，Ｍ态软件会将 S 态异常/中断委托给 S 态软件，请指出有哪些寄存器记录了委托信息，rustsbi 委托了哪些异常/中断？（也可以直接给出寄存器的值）

   - ecall: supervisor environment call handler function, should be called during the runtime exception handler, returns a0 and a1, which store the error and value o the legacy function. The part of the code that handles this is as shown below.

     ```
     let params = [trap_frame.a0, trap_frame.a1, trap_frame.a2, trap_frame.a3, trap_frame.a4];
                 // Call RustSBI procedure
                 let ans = rustsbi::ecall(trap_frame.a7, trap_frame.a6, params);
                 // Return the return value to TrapFrame
                 trap_frame.a0 = ans.error;
                 trap_frame.a1 = ans.value;
     ```
   - mideleg 和 medeleg 寄存器记录了委托信息； mideleg=0x222 ， medeleg=0xb1ab 。
   
2. 请学习 gdb 调试工具的使用(这对后续调试很重要)，并通过 gdb 简单跟踪从机器加电到跳转到 0x80200000 的简单过程。只需要描述重要的跳转即可，只需要描述在 qemu 上的情况。Please learn the use of the gdb debugging tool (this is very important for  subsequent debugging), and use gdb to simply trace the simple process  from power-on to the jump to 0x80200000. Only need to describe the important jump, only need to describe the situation on qemu.

   gdb is a powerful debugging program, its functionality includee: set breakpoints, monitor the value of program variables, display/modify the value of the variable, display/modify register, vew the stack of the program, remote debugging, debug thread. In order to use gdb, we need to compile the source program with -g or -ggdb. gdb commands include: aliases, breakpoints, data, files, internals, running, stack, status, and tracepoints

   | 地址       | 跳转                                      |
   | ---------- | ----------------------------------------- |
   | 0x1000     | start                                     |
   | 0x1010     | jump to RustSBI_start (0x80000000)        |
   | 0x80000036 | jump to RustSBI main (0x800002572)        |
   | 0x80002cce | jump to enter_priveliged (0x80001504)     |
   | 0x8000150e | jump to RustSBI s_mode_start (0x800023da) |
   | 0x800023e2 | jump to 0x80200000                        |
   
   




















## References
1. https://en.wikipedia.org/wiki/ANSI_escape_code
2. https://github.com/DeathWish5/ucore-Tutorial/tree/ch1
3. https://github.com/DeathWish5/ucore-Tutorial-Book/blob/main/lab1/exercise.md