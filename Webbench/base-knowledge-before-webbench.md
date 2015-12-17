# base-knowledge-before-webbench
在读webbench源码之前，我们需要了解一下这些知识点(需要有C的基础)

## 1. 字符串的基本操作

| 函数 | 描述 |
| :--:| :--: |
| char *strrchr(const char *, int); | 从第一个字符串中查询第二个字符**_最后一次出现的位置_**，如果有，则返回这个字符的地址 | 
| strcat(dest, src) | 把后面的字符串链接在dest后面 |
| char *strstr(const char *, const char *); | 从第一个参数字符串中找到第一个出现子串(第二个参数)的位置，返回地址 |
| int strncasecmp(const char *, const char *, size_t); | 比较前n(第三个参数)个字符, 结果为0表示相同 |
| char	*strchr(const char *, int); | 从第一个字符串中查询第二个字符**_第一次出现的位置_**，如果有，则返回这个字符的地址 |

以上函数基本上都能在`string.h`中找到

## 2. 命令参数解析
比如当我们使用命令行： `xxxx -p -b`, 其中，p和b就是命令参数.  
在c中，我们通常使用头文件`getopt.h`中的函数来解析命令。主要用到的函数：

### 1. int	 getopt(int, char * const [], const char *)
该函数的前两个参数通常就是main函数的argc和argv，这两者直接传入即可。  
第三个参数表示所有命令组成的字符串，比如：

```c
	char *str = "abcd:"
```
其中，a,b,c,d都是命令参数，冒号表示：参数d是可以指定值的，比如 `-d 100`  
当有多个命令可以选择的时候，通常我们需要循环调用这个参数来获得命令

注意，在头文件`getopt.h`中，还声明了以下的全局变量：

```c
extern char *optarg;
extern int optind, opterr, optopt;
```

1. **optarg**: 用来提取命令参数的指定的值，比如上例中参数d的100就可以通过这个参数获得
2. optind: 表示的是下一个将被处理到的参数在argv中的下标值
3. opterr: 通常等于0

### 2. int	getopt_long(int, char * const *, const char *, _const struct option *, int *_);
这个函数是上一个函数的加强版，支持**_长参数_**. 这个函数只比上面的函数多了2个参数(后两个)  

1. **const struct option *longopts**: longopts指向的是一个由option结构体组成的数组，那个数组的每个元素，指明了一个“长参数”, 比如webbench中的`-force`. 结构体option如下：

	```c
 struct option {
   const char *name;		// 参数名称
   int has_arg;	// 指明是否带参数(0:不带,1:必带,2:可选)
   int *flag;	//  当指针为空，函数直接将val的数值从getopt_long的返回值返回，当它非空时，val的值会被赋到flag指向的整型数中，而函数返回值为0
   int val; //用于指定函数找到该选项时的返回值，或者当flag非空时指定flag指向的数据的值
};
	```
2. **int *longindex**: 如果longindex非空，它指向的变量将记录当前找到参数符合longopts里的第几个元素的描述，即是longopts的下标值

## 3. 管道Pipe
管道是**父进程与子进程**, 或者**有亲缘关系的进程(同属于一个父进程)**进行通信的方式。  
管道是**半双工**的, 数据只能向一个方向流动。也就是说, 如果两个进程双方需要通信(双方都需要读写), 那么则需要两个管道  
管道单独构成一种**独立的文件系统**：管道对于管道两端的进程而言，就是一个文件，但它不是普通的文件，它不属于某种文件系统，而是自立门户，单独构成一种文件系统，并且只存在与内存中。  
数据的读出和写入：一个进程向管道中写的内容被管道另一端的进程读出。写入的内容每次都添加在管道缓冲区的末尾，并且每次都是从缓冲区的头部读出数据。

### 管道的创建

```c
#include <unistd.h>
int pipe(int fd[2]) // 创建成功则返回0, 失败返回-1
```
fd[2]表示两个文件描述符  
一个进程在由pipe()创建管道后，一般再fork一个子进程，然后通过管道实现父子进程间的通信  
管道两端可分别用描述字fd[0]和fd[1]来描述。一端只能用于读，由fd[0]表示(管道读端); 另一端只能用于写，由fd[1]表示(管道写端)  
文件的I/O操作都可以使用在管道上，比如close, read, write, fscanf(读), fprintf(写)

## 4. 信号
webbench中，使用定时器来处理压测时间. 即当到达压测时间的时候，会触发一个handler函数，这个函数由webbench实现(其实就是把变量`timerexpired`由0设置为1, 0: 未到达指定时间, 1: 到达)  
在函数中，定义了一个sigaction结构体，**用来描述对信号的处理**，这个结构体在`signal.h`中声明：

```c
struct	sigaction {
	void    (*__sa_handler)(int); // 别名是sa_handler
	void    (*__sa_sigaction)(int, struct __siginfo *, void *);
	sigset_t sa_mask;		/* signal mask to apply */
	int	sa_flags;		/* see signal options below */
};
```

1. sa_handler: 是一个函数指针，指向信号处理函数
2. sa_mask: 用来屏蔽特定的信号

信号处理函数：

```c
 int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```

1. **signum**：要操作的信号  
2. **act**：要设置的对信号的新处理方式  
3. oldact：原来对信号的处理方式  
4. 返回值：0 表示成功，-1 表示有错误发生

### 流程：  
启动子线程 ——> 开始计时 ——> 压测开始 ——> 检查timerexpired==0 ——>  
1. 若时间没到,继续压测  
2. 若到达时间, 发送信号`SIGALRM`，触发`sigaction()`函数，执行`handler()`, 将`timerexpired=1`, 当再次检查`timerexpired`的时候，退出