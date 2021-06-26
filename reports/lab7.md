# 操作系统 lab7

计83 李天勤 201808106

## 实验目的

1. realize branch ch7
2. 实现硬链接和相关系统调用```sys_linkat, sys_inlikat, sys_stat```
3. 通过ch7测试

## 实验实现

实现```sys_linkat, sys_inlikat, sys_stat```， 修改```syscall.c```中的```sycall```调用

## 实验结果

跑```ch7_file0, ch7_file1, ch7_file2```

![image-20210507002352446](lab7.assets/image-20210507002352446.png)

## 实验问答

1. 目前的文件系统只有单级目录，假设想要支持多级文件目录，请描述你设想的实现方式，描述合理即可。

   To achieve multi-level directories, you can add a directory entry that points to the inode of the next level directory on the basis of the current situation, where name is the name of the directory, and inum is the number of the corresponding inode, which can be searched recursively.

2. 在有了多级目录之后，我们就也可以为一个目录增加硬链接了。在这种情况下，文件树中是否可能出现环路？你认为应该如何解决？请在你喜欢的系统上实现一个环路(软硬链接都可以，鼓励多尝试)，描述你的实现方式以及系统提示、实际测试结果。

   If currently in directory ```dirA\dirB\dirC``` and its linked to ```dirA```, then there will be a loop. In order to avoid this,  prevent the addition of hard loops to directory.

   Linux example

   ```
   akashili17@akashili17:~/Documents$ mkdir dirA && cd dirA
   akashili17@akashili17:~/Documents/dirA$ mkdir dirB && cd dirB
   akashili17@akashili17:~/Documents/dirA/dirB$ ln ../../dirA/ dirC
   ln: ../../dirA/: hard link not allowed for directory
   akashili17@akashili17:~/Documents/dirA/dirB$ ln -s ../../dirA/ dirC
   akashili17@akashili17:~/Documents/dirA/dirB$ cd dirC
   akashili17@akashili17:~/Documents/dirA/dirB/dirC$ cd dirB
   akashili17@akashili17:~/Documents/dirA/dirB/dirC/dirB$ cd dirC
   akashili17@akashili17:~/Documents/dirA/dirB/dirC/dirB/dirC$ cd dirB
   akashili17@akashili17:~/Documents/dirA/dirB/dirC/dirB/dirC/dirB$ cd dirC
   ```

   ![image-20210506214135908](lab7.assets/image-20210506214135908.png)

