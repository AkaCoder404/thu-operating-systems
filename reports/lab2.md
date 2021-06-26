# Lab 2

计83 李天勤 201808106

## 实验目的

1. 实现分支ch2
2. 能运行用户态程序并执行 sys_write 和 sys_exit 系统调用
3. 增加对sys_write的安全检查

## 实验结果

![image-20210315224136979](/home/akashili17/.config/Typora/typora-user-images/image-20210315224136979.png)

## 问答 

1. 正确进入 U 态后，程序的特征还应有：使用 S 态特权指令，访问 S 态寄存器后会报错。目前由于一些其他原因，这些问题不太好测试，请同学们可以自行测试这些内容（参考[前三个测例](https://github.com/DeathWish5/rCore_tutorial_tests/tree/master/user/src/bin>）))，描述程序出错行为，同时注意注明你使用的 sbi 及其版本。

   - rustsbi版本 -  RustSBI version 0.1.1

   - error using SRET``` bad_instruction```

   - error accessing sstatus using csrr ```bad_register```

   - writing 0S at 0x0 causes error ```bad_address```

2. 请结合用例理解 [trampoline.S](https://github.com/DeathWish5/ucore-Tutorial/blob/ch2/kernel/trampoline.S) 中两个函数 `userret` 和 `uservec` 的作用，并回答如下几个问题:

   1. L79: 刚进入 `userret`时，`a0`、`a1`分别代表了什么值。

      【解答】

      ```
      a0 代表trapframe的initial地址
      a1 代表user页面的initial地址
      ```

   2. L87-L88: `sfence` 指令有何作用？为什么要执行该指令，当前章节中，删掉该指令会导致错误吗？

      ```
      csrw satp, a1
      sfence.vma zero, zero
      ```

      【解答】

      sfence.wma instruction refreshes/flushes TLB entries, it is done so because userret needs to complete conversion from S state to U state. Because U state uses page tables and TLB., we need to refresh the cached content.  页表 has not been implemented, so deletion will have no effect.

   3. L96-L125: 为何注释中说要除去 `a0`？哪一个地址代表 `a0`？现在 `a0` 的值存在何处？

      ```
      # restore all but a0 from TRAPFRAME
      ld ra, 40(a0)
      ld sp, 48(a0)
      ld t5, 272(a0)
      ld t6, 280(a0)
      ```

      【解答】

      a0 is used to obtain the values of other registers saved in trapframe, thus mused be saved after the other registers are saved.

      the address 112(a0) represents a0. 

      a0 is eventually saved in the sscratch register

   4. `userret`：中发生状态切换在哪一条指令？为何执行之后会进入用户态？

      【解答】

      State switch occurs at the last sret instruction of userret.

      Since sstatus and sepc are set in usertrapret, and the sret instruction will cause the pc to jump to the next instruction of the interrupt instruction

      

   5. L29： 执行之后，a0 和 sscratch 中各是什么值，为什么？

      ```
      csrrw a0, sscratch, a0
      ```

      【解答】
      After execution, a0 is the starting address of trapframe, and sscratch is the value in the original U state a0 register. This is because before execution, sscratch is the starting address of the trapframe, and the value in a0 is set in the U state. The above statement is to exchange a0 and sscratch.

   6. L32-L61: 从 trapframe 第几项开始保存？为什么？是否从该项开始保存了所有的值，如果不是，为什么？

      ```
      sd ra, 40(a0)
      sd sp, 48(a0)
      ...
      sd t5, 272(a0)
      sd t6, 280(a0)
      ```

      【解答】

      Start saving from the 6th item, because the first five items are all values of S state, you don't need to save it when you enter the S state from the U state.

      Do not save from a0 to 112(a0), since at this time a0 stores the starting address of the trapframe, and the real U state a0 register value is stored in sscratch.

   7. 进入 S 态是哪一条指令发生的？

      【解答】

      enter the S state when the ecall instruction is executed

   8. L75-L76: `ld t0, 16(a0)` 执行之后，`t0`中的值是什么，解释该值的由来？

      ```
      ld t0, 16(a0)
      jr t0
      ```

      【解答】

      value of t0 is the start address of usertrap. 

      16(a0) refers to the third item in trapframe, kernel_trap, and this item is assigned to the start address of usertrap in usertrapret.

3. 描述程序陷入内核的两大原因是中断和异常，请问 riscv64 支持那些中断／异常？如何判断进入内核是由于中断还是异常？描述陷入内核时的几个重要寄存器及其值。

   | Interrupt | Exception Code | Description                    |
   | --------- | -------------- | ------------------------------ |
   | 1         | 0              | user software interrupt        |
   | 1         | 1              | supervisor software interrupt  |
   | 1         | 2              | hyper visor software interrupt |
   | 1         | 3              | machine software interrupt     |
   | 1         | 4              | user timer interrupt           |
   | 1         | 5              | supervisor timer interrupt     |
   | 1         | 6              | hpervisor timer interrupt      |
   | 1         | 7              | machine timer interrupt        |
   | 1         | 8              | user external interrupt        |
   | 1         | 9              | supervisor external interrupt  |
   | 1         | 10             | hypervisor external interrupt  |
   | 1         | 11             | machin external interupt       |
   | 1         | $\geq$ 12      | reserved                       |
   | 0         | 0              | Instruction address misaligned |
   | 0         | 1              | instruction access fault       |
   | 0         | 2              | illegal instruction            |
   | 0         | 3              | breakpoint                     |
   | 0         | 4              | load address misaligned        |
   | 0         | 5              | load access fault              |
   | 0         | 6              | store/AMO address misaligned   |
   | 0         | 7              | store/AMO access fault         |
   | 0         | 8              | environment call from U-mode   |
   | 0         | 9              | environment call from S-mode   |
   | 0         | 10             | environment call from H-mode   |
   | 0         | 11             | environment call from M-mode   |
   | 0         | $\geq$12       | reserved                       |

   Use the first bit of the scause register to determine whether the reason for entering the kernel is an interrupt or an exception

   There are 8 important registers

   - stvec: Supervisor trap handler base address
   - sepc: Supervisor exception program counter.
   - scause: Supervisor trap cause.
   - sie: Supervisor interrupt-enable register.
   - sip: Supervisor interrupt pending.
   - stval: additional information: the address of the error in the address exception, the instruction of the illegal instruction exception, etc.;
   - sscratch: Scratch register for supervisor trap handlers.
   - sstatus: Supervisor status register. 

4. 对于任何中断， `__alltraps` 中都需要保存所有寄存器吗？你有没有想到一些加速 `__alltraps` 的方法？简单描述你的想法。

   Not all registers have to be saved in __alltraps, I think it is possible to save registers according to the interrupt type. If an interrupt does not change the value of some specific registers, then there is no need to  save the value of these registers, instead we can just discard it, therefore speeding up the process.

