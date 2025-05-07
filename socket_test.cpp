#include <stdio.h>
#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

void print_ip_port(int sockfd);
//server端
int main(){
    int socket_server=socket(AF_INET,SOCK_STREAM,0); //创建了server的socket

    int opt = 1;
    setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //AF_INET表示使用了IPv4的地址
    //SOCK_STREAM表示使用TCP这个协议，如果使用UDP则是SOCK_DGRAM
    //最后一个参数为0可自动根据前面两个参数来自动选择合适的协议
    struct sockaddr_in addr_server;
    //上面那个结构体将套接字与IP和端口进行绑定，这是专门为IPv4所设计的结构体，其他地址的不能使用
    //他的成员如下所视
    // struct sockaddr_in{
    //     sa_family_t     sin_family;   //地址族（Address Family），也就是地址类型
    //     uint16_t        sin_port;     //16位的端口号
    //     struct in_addr  sin_addr;     //32位IP地址
    //     char            sin_zero[8];  //不使用，一般用0填充
    // };
    addr_server.sin_family=AF_INET; //使用IPv4地址
    addr_server.sin_addr.s_addr=inet_addr("127.0.0.1"); //IP地址，并且这个地址比较特殊被称为回环地址(本地地址)，这个总是用来测试本地程序
    addr_server.sin_port=htons(1234); //绑定了端口1234，如果有别的程序使用这个端口，bind就会失败，可以换一个
    //电脑一般都是小端序，而网络通信使用的是大端序所以需要使用htons来转换一下
    bind(socket_server,(struct sockaddr*)&addr_server,sizeof(addr_server));
    printf("bind success\n");
    //bind将IP和端口绑定起来交给socket来处理
    if(listen(socket_server,5)<0){
        std::cout<<"listen fail"<<std::endl;
    }
    //开始监听，后面的数字5是等待队列的长度
    struct sockaddr_in addr_client;
    //准备开始处理客户端的请求
    socklen_t client_len=sizeof(addr_client);
    int socket_client=accept(socket_server,(struct sockaddr*)&addr_client,&client_len);
    sleep(1);
    printf("always waiting\n");
    //调用了accept来接收了客户端的请求(如果未收到请求程序会一直阻塞下去)
    //用addr_client来存储了客户端的IP和端口等信息
    //最后用client_len来得到了客户端的实际大小

    //以下开始向客户端发送信息
    char message[]="hello client,I'm server";
    for(;;){
        printf("enter cycle\n");
        char buf[100]={};
        printf("waiting client message\n");
        int len=recv(socket_client,buf,sizeof(buf),0);
        printf("recv success\n");
        if(len<=0) {
        printf("error\n");
           break;
        }
        if(strncmp(buf,"quit",4)==0) break;
        buf[len]='\0';
        printf("client: %s\n",buf);
        fflush(stdout);
        fgets(buf,sizeof(buf),stdin);
        buf[strcspn(buf, "\n")] = '\0';  // 去掉换行符
        //write(socket_client,message,sizeof(message)); //这个相比于send少了最后一个flag参数，没有那么灵活
        if(send(socket_client,buf,strlen(buf),0)<0){
            std::cout<<"send fail"<<std::endl;
        }
        //关闭所使用的套接字
    }
    print_ip_port(socket_server);
    if(close(socket_server)<0){
        std::cout<<"close socket_server fail"<<std::endl;
    }
    if(close(socket_client)<0){
        std::cout<<"close socket_client fail"<<std::endl;
    }
    return 0;
}

void print_ip_port(int sockfd){
    //这个函数能成功运行的前提是已经建立了连接
    
    struct sockaddr_in local_addr,client_addr;
    socklen_t socklen=sizeof(sockaddr_in);
    getsockname(sockfd,(struct sockaddr*)&local_addr,&socklen);
    //通过上面那个函数可获取本地socket的端口号和ip号，得到的结果存在了第二个参数的那个结构体中了
    getpeername(sockfd,(struct sockaddr*)&client_addr,&socklen); //这个是获取远端的ip和端口的
    std::cout<<"local  ip is:"<<inet_ntoa(local_addr.sin_addr)<<std::endl;
    //inet_ntoa将ip转换为人看的懂的字符串，可以认为它与htons是互相逆向转换的
    std::cout<<"local  port is:"<<ntohs(local_addr.sin_port)<<std::endl;

    std::cout<<"client ip is:"<<inet_ntoa(client_addr.sin_addr)<<std::endl;
    std::cout<<"client port is:"<<ntohs(client_addr.sin_port)<<std::endl;
}
//79-86

// void errreport(std::string &x);
// int main(){
//     int server_fd;
//     server_fd=socket(AF_INET,SOCK_DGRAM,0);   
//     if(server_fd<0){
//     errreport("server_fd");
//     }
//     struct sockaddr_in seraddr;
//     seraddr.sin_port=htons(2537);
//     seraddr.sin_family=AF_INET;
//     seraddr.sin_addr.s_addr=INADDR_ANY; //默认是本机的ip
//     int bind_fd=bind(server_fd,(struct sockaddr*)&seraddr,sizeof(seraddr));
//     if(bind_fd<0){
//         errreport("bind_fd");
//     }
//     int listen_fd=listen(server_fd,5);
//     if(listen_fd<0){
//         errreport("listen_fd");
//     }
    
//     int epoll_fd=epoll_create1(0);
//     if(epoll_fd<0){
//         errreport("epoll_fd");
//     }
//     struct epoll_event event;
//     event.events=EPOLLIN;
//     event.data.fd=server_fd;

//     return 0;
// }
// void errreport(std::string x){
//     std::cout<<x<<" start fail"<<std::endl;
//     exit(1);
// }