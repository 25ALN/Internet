# getsockopt
是一个用于检索与特定套接字关联的选项的函数。它可以用来获取套接字的各种状态信息，包括错误代码(一种掌控网络状态变化的工具)

1. 函数原型
```c
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
```
- level 指定了选项的类型
- optname 指定了要检索的选项
- optval 是指向保存选项值的缓冲区的指针
- optlen 是指向缓冲区长度的指针

# epoll
- 分为两种模式
1. 水平触发(LT)  
默认工作模式，表示当epoll_wait检测到某描述符事件就绪并通知应用程序时，应用程序可以不立即处理该事件；下次调用epoll_wait时，会再次通知此事件。
2. 边缘触发(ET)  
 当epoll_wait检测到事件就绪并通知应用程序时，应用程序必须立即处理该事件。如果不处理，下次调用epoll_wait时，不会再次通知此事件。边缘触发只在状态由未就绪变为就绪时只通知一次。(此模式搭配I/O复用)

- 三个函数接口  
1. **int epoll_create(int size)**;
```
会相应的的创建一个文件句柄/epoll实例（这也是这个函数的返回值），size为监听的数目大小，若创建失败会返回-1，在使用完这个文件句柄时还要记得需要关闭（调用close），不然累积过多会占用资源  
现在，size这个参数传入的值为1即可，内核会自动进行扩容
```  
2. **int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)**;  
此函数用来管理监控的事件,成功返回0,失败返回-1；
- epfd: epoll_create()返回的文件描述符
- op: 操作类型，以下是相应的选项 
1. *EPOLL_CTL_ADD*  
注册新事件到fd中  
2. *EOPLL_CTL_MOD*  
修改已注册事件  
3. *EOPLL_CTL_DEL*  
删除一个事件  
- fd  
需要监听的文件描述符  
- event  
指向 epoll_event 结构体的指针，定义监控的事件类型及用户数据  
**epoll_event结构体**
```c
struct epoll_event {
    uint32_t     events;  // 事件掩码（如EPOLLIN、EPOLLOUT）也是相应的事件类型
    epoll_data_t data;    // 用户数据（联合体，可存储指针、fd等）
};

typedef union epoll_data {
    void    *ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;  
```  
常用事件类型  
```
EPOLLIN：数据可读。
EPOLLOUT：数据可写。
EPOLLERR：发生错误（自动监控，无需显式设置）。
EPOLLHUP：连接挂断（自动监控）。
EPOLLET：边缘触发模式（默认是水平触发）。  
EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
```  
3. **int epoll_wait(int epfd, struct epoll_event *events,int maxevents, int timeout)**;   
阻塞等待epoll实例中的文件描述符产生事件，返回就绪的事件列表，成功返回就绪事件的数量，失败返回-1；  
- epfd  
epoll_create()返回的文件描述符  
- events  
存放就绪事件的数组  
- maxevents  
数组的最大容量（至少得大于0）  
- timeout  
等待的时间  
-1 : 无限阻塞  
 0 : 立即返回(也就是非阻塞)  
大于0 : 等待所指定的毫秒数  

# accept/recv/send
返回相应的错误码含义
- EAGAIN 或 EWOULDBLOCK

原因：套接字被设置为非阻塞（O_NONBLOCK），且当前没有待接受的连接。  

处理：在非阻塞模式下需等待或重试；若使用 epoll/select，需监听可读事件。

- EBADF

原因：传入的套接字描述符无效（如已被关闭）。

处理：检查套接字是否正确初始化并处于监听状态。

- ECONNABORTED

原因：客户端在完成三次握手后中止了连接（如异常断开）。

处理：可安全忽略此错误，继续调用 accept 处理其他连接。

- EINTR

原因：系统调用被信号中断（如进程收到 SIGINT）。

处理：需重新调用 accept，通常需在循环中处理此类中断。

- EINVAL

原因：套接字未处于监听状态（未调用 listen），或 addrlen 参数无效（如为负数）。

处理：确保套接字已正确调用 listen，并检查参数合法性。

- EMFILE/ENFILE

EMFILE：进程打开的文件描述符数量超过限制。

ENFILE：系统级文件描述符资源耗尽。

处理：关闭不必要的描述符，或调整系统限制（如 ulimit）。

- ENOTSOCK

原因：传入的描述符不是套接字（如普通文件描述符）。

处理：检查参数是否为有效的套接字描述符。

- EOPNOTSUPP

原因：套接字类型不支持 accept（如 SOCK_DGRAM 数据报套接字）。

处理：仅对 SOCK_STREAM（如 TCP）套接字使用 accept。

- ENOBUFS/ENOMEM

原因：系统内存不足，无法分配资源。

处理：优化资源使用，或等待系统恢复。

 EFAULT
原因：参数 addr 或 addrlen 指向无效内存地址。
处理：确保地址参数有效（如使用 sockaddr_storage 结构）。

EPROTO
原因：协议栈发生错误（如网络协议不兼容）。
处理：检查网络配置或协议实现。

# 网络的一些命令
- 查看端口是否被占用(有时候程序第二次无法运行时就需要检查)
lsof -i:端口号
之后若检查出来相关的占用利用kill来终止进程
- kill
kill -数字 端口号
一般设置 -9 这样可强制关闭某个端口