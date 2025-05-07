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

