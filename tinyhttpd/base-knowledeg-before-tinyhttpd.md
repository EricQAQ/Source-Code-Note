#base-knowledeg-before-tinyhttpd
tinyhttpd是一个微型的服务器程序，分析它的代码可以详细了解服务器工作的实质.

## 1. TCP链接
TCP的三次握手，这里就不详细讲了。这里主要讲的是C中客户端如何与服务器建立连接  

### 服务器端
1. 创建套接字
2. 给结构体sockaddr\_in变量开辟空间，设置基本配置(AF_INET, 端口号, ip地址)
3. 绑定套接字与第二步的变量，通常使用`bind()`函数
4. 监听这个套接字，通常使用`listen()`函数
5. 循环调用接受请求函数，该函数是阻塞的，接受请求函数通常是`accepte()`

### 客户端
1. 创建套接字
2. 给结构体sockaddr\_in变量开辟空间，设置基本配置(AF_INET, 端口号, ip地址)，配置为需要连接的服务器端的参数
3. 向服务器端发送连接请求，通常使用`connect()`函数
4. 通过`write()\read()`函数，向socket描述符对应的缓冲区写\读数据，这些数据会通过`send\recv`函数通过Tcp连接的方式传输

## 2. send(), recv()函数
### 1. send()函数

```c
int send(int s, const void *buf, size_t len, int flag )
```
1. **s**: 目标地址的socket描述符
2. **buf**: 存放要发送的数据的缓冲区(地址)
3. **len**: 发送的数据的长度
4. **flag**: 通常设置为0

这个函数的作用就是把缓冲区中的数据发送到目标地址(socket描述符).  
**不论是客户端还是服务器端应用程序都用send函数来向TCP连接的另一端发送数据.**  
send函数有三种返回值：

	返回值=0：发送成功
	返回值<0：发送失败，错误原因存于全局变量errno中
	返回值>0：表示发送的字节数（实际上是拷贝到发送缓冲中的字节数）


### 2. recv()函数

```c
int recv(int s, const void *buf, size_t len, int flag)
```
1. **s**: 发送端的socket描述符
2. **buf**: 接收端的缓冲区，用来存放接受的数据
3. **len**: 数据的长度
4. **flag**: 通常设置为0

这个函数的作用就是把发送端(socket描述符)发送的数据存放在缓冲区中.  
**不论是客户端还是服务器端应用程序都用recv函数从TCP连接的另一端接收数据.** 
返回值：

	成功执行时，返回接收到的字节数。
	若另一端已关闭连接则返回0，这种关闭是对方主动且正常的关闭
	失败返回-1

在tinyhttpd中，使用到了recv的第四个变量：flag

```c
n = recv(sock, &c, 1, 0);
/* DEBUG printf("%02X\n", c); */
if (n > 0) {
    if (c == '\r') {    // 读取到CRLF,换行符
        n = recv(sock, &c, 1, MSG_PEEK);    // 预读取下一个字符
        /* DEBUG printf("%02X\n", c); */
        if ((n > 0) && (c == '\n'))
            recv(sock, &c, 1, 0);
        else
            c = '\n';
    }
    buf[i] = c;
    i++;
}
else
    c = '\n';
```
`flag=MSG_PEEK`,recv函数会从套接字的缓冲区中**预读取**size个字符(并不把读取的字符从缓冲区删除)  
这段函数的作用其实就是当遇到回车符(\t)的时候进行强制换行

## 3. stat()函数
`stat()`函数用来获得文件信息、状态.

```c
int stat(const char *file_name, struct stat *buf)
```
**`stat()`用来将参数`file_name`所指的文件状态, 复制到参数`buf`所指的结构中。**
具体的详细说明见[stat函数以及结构体](http://c.biancheng.net/cpp/html/326.html)

在`tinyhttpd`中，有如下代码

```c
if((st.st_mode & S_IFMT) == S_IFDIR){
	strcat(path, "/index.html");
}
```
`st.st_mode&S_IFMT`可以获得文件类型. S_IFDIR表示文件是一个目录

```c
if ((st.st_mode & S_IXUSR) ||
    (st.st_mode & S_IXGRP) ||
    (st.st_mode & S_IXOTH))
    cgi = 1;
```
1. **S_IXUSR**: 文件所有者拥有文件可执行权限
2. **S_IXGRP**: 用户组拥有文件可执行权限
3. **S_IXOTH**: 其他用户拥有文件可执行权限