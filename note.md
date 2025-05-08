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
