#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <sys/epoll.h>
#include <condition_variable>
#include <thread>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等
#include <time.h> //获取时间

// int main(){
//     int connectfd;
//     time_t ticks;
//     char buf[100];
//     struct sockaddr_in savetime;
//     int sockfd=socket(AF_INET,SOCK_STREAM,0);
//     savetime.sin_family=AF_INET;
//     savetime.sin_port=htons(13);
//     savetime.sin_addr.s_addr=htonl(INADDR_ANY); //INADDR_ANY表示是任意本地IP地址

//     bind(sockfd,(struct sockaddr*)&savetime,sizeof(savetime));
//     listen(sockfd,5);

//     //一次只能处理一个客户端，且处理完后一直等待着下一个客户端的连接
//     while(1){
//         connectfd=accept(sockfd,(struct sockaddr*)NULL,NULL);
//         ticks=time(NULL);
//         snprintf(buf,strlen(buf),"%24s\n",ctime(&ticks));
//         write(connectfd,buf,sizeof(buf));
//         close(connectfd);
//     }
    
//     return 0;
// }

//输出当前系统中 TCP 和 UDP 套接字的默认发送（send buffer）和接收（receive buffer）缓冲区大小

// void print_buffer(int fd,const std::string& type);
// int main(){
//     int tcpsocket=socket(AF_INET,SOCK_STREAM,0);
//     if(tcpsocket<0){
//         perror("create tcp socket fail");
//         return 1;
//     }
//     int udpsocket=socket(AF_INET,SOCK_DGRAM,0);
//     if(udpsocket<0){
//         perror("create udp socket fail");
//         return 1;
//     }
//     print_buffer(tcpsocket,"TCP");
//     print_buffer(udpsocket,"UDP");
//     return 0;
// }
// void print_buffer(int fd,const std::string& type){
//     int rl=0,sl=0;
//     socklen_t len=sizeof(int);
    
//     std::cout<<type<<" receve buffer is:";
//     int rebuf=getsockopt(fd,SOL_SOCKET,SO_RCVBUF,&rl,&len); 
//     //getsockopt参数详细可见p151页，很详细
//     //本示例中用的参数就是获取发送和缓冲区大小的
//     std::cout<<rl<<" byte"<<std::endl;

//     std::cout<<type<<" send buffer is:";
//     int sebuf=getsockopt(fd,SOL_SOCKET,SO_SNDBUF,&sl,&len);
//     std::cout<<sl<<" byte"<<std::endl;
// }

// getsockopt test 非阻塞connect
// int Connect(int fd,const sockaddr* clt,socklen_t clt_len,int mark){
//     int error_fd=0,flags,connect_fd;
//     socklen_t len;
//     flags=fcntl(fd,F_GETFL,0); //这一步获取当前文件表述符的状态，返回给了flags
//     fcntl(fd,F_SETFL,flags|O_NONBLOCK);
//     //设置当前文件法描述符为非阻塞的，使用|保留了之前文件描述符的状态，否则会被新的标志给覆盖
//     if(connect_fd=connect(fd,clt,len)<0){
//         if(error_fd!=EINPROGRESS){
//             //连接异常，即刻退出
//             exit(1);
//         }
//     }
//     //将套接字描述符和设置为非阻塞，后续用epoll进行完善

//     int revel=getsockopt(fd,SOL_SOCKET,SO_ERROR,&error_fd,&len);
//     if(revel<0){
//         //异常退出
//         exit(1);
//     }else{
//         if(error_fd!=0){
//             std::cout<<"connect fail"<<std::endl;
//             exit(1);
//         }else{
//             std::cout<<"connect success"<<std::endl;
//         }
//     }
// }

// int main(){
//     int sockfd=socket(AF_INET,SOCK_DGRAM,0);
//     struct sockaddr_in clt;
//     clt.sin_family=AF_INET;
//     clt.sin_addr.s_addr=htonl(INADDR_ANY);
//     clt.sin_port=htons(1234);
//     if(Connect(sockfd,(struct sockaddr*)&clt,sizeof(clt),0)<0){
//         std::cout<<"error"<<std::endl;
//         exit(1);
//     }
//     return 0;
// }

//用锁保护accept，避免多个线程同时调用
//使用一个主线程来监听accept函数，受到消息后传入线程池中进行工作
// std::mutex accept_def;
// std::condition_variable hx;


// void accept_work(int fd,struct sockaddr *s,socklen_t s_len);
// int main(){
//     int server_fd=socket(AF_INET,SOCK_DGRAM,0);
//     struct sockaddr_in ser;
//     ser.sin_family=AF_INET;
//     ser.sin_addr.s_addr=htonl(INADDR_ANY);
//     ser.sin_port=htons(1234);
//     bind(server_fd,(struct sockaddr*)&ser,sizeof(ser));

//     listen(server_fd,5);
//     //这里将会启动工作和线程
//     std::thread accept_begin(accept_work,server_fd,(struct sockaddr*)&ser,sizeof(ser));
//     return 0;
// }

// void accept_work(int fd,struct sockaddr *s,socklen_t s_len){
//     while(true){
//         if(accept(fd,s,&s_len)<0){
//             continue;
//         }
//         {
//             std::lock_guard<std::mutex> lock(accept_def);
//             //这里将任务加入线程池开始执行任务
//         }
//         hx.notify_one(); //开始工作
//     }
// }

// epoll例子，省略了服务器中具体的操作如bind,listen
int main(){
    int fd=epoll_create(1); //创建了一个epoll的实例
    struct epoll_event ev,events[20];
    int server_fd,ready=0,connect_fd; 

    ev.data.fd=server_fd; //得到该事件的描述符
    ev.events=EPOLLIN|EPOLLET; //设置该事件为可读，且切换为边缘触发模式

    int reval=epoll_ctl(fd,EPOLL_CTL_ADD,server_fd,&ev); 
    if(reval<0){
        std::cout<<"epoll_ctl fail"<<std::endl;
        exit(1);
    }

    while(true){
        ready=epoll_wait(fd,events,20,0); //得到已准备好的事件数量
        for(int i=0;i<ready;i++){
            if(server_fd==events[i].data.fd){ //当检测到有新的连接后，开始注册事件
            //调用accept,并且将这个套接字设置为非阻塞模式
            // 并且将这个新的事件加入到epoll的监听队列中去

            ev.data.fd=connect_fd;
            ev.events=EPOLLIN|EPOLLET;
            epoll_ctl(fd,EPOLL_CTL_ADD,connect_fd,&ev);
            }
            // 下面就将处理客户端具体的消息内容
        }
    }
    return 0;
}