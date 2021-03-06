/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

/* isspace(int c) 函数用来检查c是否为空格字符
 * 空格字符: ' ', '\t'(水平制表), '\n'(换行符), '\v'(垂直制表符), '\f'(换页符), '\r'(回车符)
 */
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);

void bad_request(int);

void cat(int, FILE *);

void cannot_execute(int);

void error_die(const char *);

void execute_cgi(int, const char *, const char *, const char *);

int get_line(int, char *, int);

void headers(int, const char *);

void not_found(int);

void serve_file(int, const char *);

int startup(u_short *);

void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
// 客户端请求被服务器接收后,开辟线程,该线程所执行的函数
void accept_request(int client) {
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
    char *query_string = NULL;

    // 从client这个socket中读取sizeof(buf)个数据(其实就是http请求的第一行数据),写入buf中
    numchars = get_line(client, buf, sizeof(buf));
    i = 0;
    j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
        // 读取buf的数据,获得method,比如GET,POST,HEAD等
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) { // strcasecmp比较两个字符串,相同返回0
        // 方法不是GET或者POST的时候,执行下面的函数
        unimplemented(client);
        return;
    }

    if (strcasecmp(method, "POST") == 0)
        cgi = 1;    // 这个POST请求需要调用cgi

    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))    // 空字符
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        // 获得客户端请求的url,并写入变量url中
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        // 在请求url中找出请求参数
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?') {
            cgi = 1;    // 这个GET请求需要调用cgi
            *query_string = '\0';
            query_string++;     // 指针指向?后的第一个字符

            /*
             * ex: http://www.baidu.com/search?id=1
             * 执行上面的代码后: http://www.baidu.com/search\0id=1
             * 把?替换成\0,读取url的时候,读到\0的时候就会停止,达到分割url与请求参数的目的
             */
        }
    }

    // sprintf(char *str, char *format, [..]) 将格式化的数据写入字符串
    sprintf(path, "htdocs%s", url);     // path 存储 请求的url(去掉参数), htdocs是存放网页的文件夹,与Apache的WWW类似
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    /* stat(const char *file_name, struct stat *buf)
     * stat函数用来把file_name所指的文件状态复制到buf中
     * 下面的作用是在系统上查询该文件是否存在
     */
    if (stat(path, &st) == -1) {
        // 文件不存在,把本次http请求的后续内容读取完,忽略,然后返回404
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else {
        /*
         * 文件存在
         * 文件&S_IFMT 这个操作可以获得文件的类型
         */
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            // 文件类型为目录
            strcat(path, "/index.html");
        /*
         * S_IXUSR 文件所有者具有可执行权限
         * S_IXGRP 用户组有可执行权限
         * S_IXOTH 其他用户有可执行权限
         */
        if ((st.st_mode & S_IXUSR) ||
            (st.st_mode & S_IXGRP) ||
            (st.st_mode & S_IXOTH))
            cgi = 1;    // 请求需要调用cgi
        if (!cgi)
            serve_file(client, path);   // 不需要调用cgi
        else
            execute_cgi(client, path, method, query_string);    // 执行cgi程序
    }

    close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource) {
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource)) {   // 循环读取
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
// 打印错误
void error_die(const char *sc) {
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string) {
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A';   // 确保能进入下面的循环
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        // GET请求,读取http请求剩下的部分,然后忽略
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else    /* POST */
    {
        numchars = get_line(client, buf, sizeof(buf));
        // 下面的循环主要是为了提取content-length,即body的大小,剩余部分忽略
        while ((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));  // 获得Content-Length, 即body的大小
            numchars = get_line(client, buf, sizeof(buf));  // 读取http请求剩余的部分,然后忽略
        }
        if (content_length == -1) {
            // header没有content-length参数,报错
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    // 创建2个管道,用来让2个进程通信
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    // 创建子进程
    if ((pid = fork()) < 0) {
        cannot_execute(client);
        return;
    }
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        // dup2(int oldfd, int newfd)用来复制参数oldfd所指的文件描述词, 并将它拷贝至参数newfd后一块返回
        dup2(cgi_output[1], 1); // 将管道cgi_output的写端定向到子进程的写端(1是STDOUT写入端)
        dup2(cgi_input[0], 0);  // 将管道cgi_input的读端定向到子进程的读端(0是STDIN读取端)
        close(cgi_output[0]);   // 关闭cgi_output管道的读端
        close(cgi_input[1]);    // 关闭cgi_input管道的写端

        sprintf(meth_env, "REQUEST_METHOD=%s", method); // 构造meth_env变量
        /**
         * putenv(const char *string) 改变或增加环境变量的内容(字符串格式:"name=value")
         * 如果原先name变量存在,则值更新为value,如果不存在,则创建
         */
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);    // 构造query_env变量,用来存放请求参数
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);   // 构造length_env变量,存放body的长度
            putenv(length_env);
        }

        /**
         * int extcl(const char *path, const char *arg, ...)
         * 该函数用来执行path代表的文件路径, 后面的变量表示执行需要的参数
         */
        execl(path, path, NULL);    // 执行cgi脚本, 相当于目前的服务器程序(通常用PHP,JAVA,Python,ruby等语言完成)
        exit(0);
    } else {    /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        /*
         * 如果请求是POST, 继续读取body的内容, 并把读取到的内容写入管道,交给子进程
         */
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1); // 写入管道,交给子进程
            }
        while (read(cgi_output[0], &c, 1) > 0)  // 从管道中读出子进程的输出,发送到客户端
            send(client, &c, 1, 0);

        // 关闭管道
        close(cgi_output[0]);
        close(cgi_input[1]);

        // 等待子进程的退出
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
// 从socket中读取size个字符,存入buf中, 返回已读取字符的个数
int get_line(int sock, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        /*
         * recv(int sock, char *buf, int len, int flags) 用来接受TCP连接的另一端发送过来的数据
         * sock是socket描述符,buf是用来存放接收过来的数据的内存的地址,
         * len指明buf的长度, flag通常设置为0
         * 该函数返回值是实际读取的字节数,如果网络中断则返回0, 失败返回-1
         */
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0) {
            if (c == '\r') {    // 读取到CRLF,换行符
                // flag=MSG_PEEK的时候,recv函数会从套接字的缓冲区中预读取size个字符(并不把读取的字符从缓冲区删除)
                n = recv(sock, &c, 1, MSG_PEEK);    // 预读取下一个字符
                /* DEBUG printf("%02X\n", c); */
                /* 如果预读取的是换行符,则把换行符写入;否则也写入换行符.
                 * 下面几行的函数其实就是当遇到CRLF的时候,添加一个换行符进行强制换行
                 */
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
    }
    buf[i] = '\0';

    return (i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename) {
    char buf[1024];
    (void) filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
// 把文件传输给客户端
void serve_file(int client, const char *filename) {
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A';
    buf[1] = '\0';
    // 循环读取http请求后面的所有内容,然后忽略掉
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");    // 打开文件
    if (resource == NULL)
        not_found(client);
    else {
        headers(client, filename);  // 将文件的基本信息封装成HTTP Response请求,并发送
        cat(client, resource);  // 读出这个文件的全部内容,并作为response body发送给客户端
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port) {
    /*
     * 下面的main函数中,会先执行startup函数,然后再执行accept函数
     * 所以这个startup函数需要做的就是,先执行bind(绑定端口),然后再执行listen(监听端口)
     */
    int httpd = 0;
    struct sockaddr_in name;

    // 建立socket时,指定协议,使用PF_xxx, 设置地址时使用AF_xxx(二者差别很小,混用也ok)
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);   // htons()用来把端口号转成16位网络字节序(大尾顺序)
    // htonl()用来把本机字节序转化为32位网络字节序(统一转换成大尾顺序)
    name.sin_addr.s_addr = htonl(INADDR_ANY);   // IP地址, INADDR_ANY=inet_addr("0.0.0.0"),表示监听所有地址

    // 执行bind函数,把socket描述字和sockaddr_in绑定
    if (bind(httpd, (struct sockaddr *) &name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);

        // 如果端口号port=0, 手动调用getsockname函数获取端口号
        // 调用getsockname()获取系统给httpd这个socket随机分配的端口号
        if (getsockname(httpd, (struct sockaddr *) &name, &namelen) == -1)
            error_die("getsockname");
        // name.sin_port 随机端口，使用ntohs将16位网络字符顺序(大尾顺序)转换成主机字符顺序(大尾\小尾,不同机型不一样)
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return (httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
// 当请求方法不是GET 和 POST的时候,调用这个函数, 告诉客户端,请求的方法未定义
void unimplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void) {
    int server_sock = -1;
    u_short port = 0;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    // 在对应端口建立httpd服务, 如果port = 0，则端口随机
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    // 无限循环，一个请求创建一个线程,在线程中会执行accept_request函数
    while (1) {
        // accept函数是阻塞的,也就是说,只有当客户端发送请求的时候,才能继续执行下去
        client_sock = accept(server_sock, (struct sockaddr *) &client_name,
                             &client_name_len);
        if (client_sock == -1)  // 失败
            error_die("accept");
        /* accept_request(client_sock); */
        /**
         * 创建线程，accept_request是线程处理函数，client_sock是参数，注意做下参数类型转换
         * 函数pthread_create用来创建线程, 返回0表示线程创建成功,其他值表示失败, 它有四个参数:
         * 1. 指向线程标识符的指针(pthread_t*)
         * 2. 线程属性
         * 3. 线程运行函数起始地址,其实就是告诉线程,它该执行哪个函数
         * 4. 需要运行的函数的参数
         */
        if (pthread_create(&newthread, NULL, accept_request, (void *) &client_sock) != 0)
            perror("pthread_create");   // 创建失败
    }

    close(server_sock);

    return (0);
}
