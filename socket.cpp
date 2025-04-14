#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

//1041
//server端
int main(){
    int socket_server=socket(AF_INET,SOCK_STREAM,0); //创建了server的socket
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
    addr_server.sin_addr.s_addr=inet_addr("127.0.0.1"); //IP地址
    addr_server.sin_port=htons(1234); //绑定了端口1234
    bind(socket_server,(struct sockaddr*)&addr_server,sizeof(addr_server));
    //bind将IP和端口绑定起来交给socket来处理
    listen(socket_server,5);
    //开始监听，后面的数字5是等待队列的长度
    struct sockaddr_in addr_client;
    //准备开始处理客户端的请求
    socklen_t client_len=sizeof(addr_client);
    int socket_client=accept(socket_server,(struct sockaddr*)&addr_client,&client_len);
    //调用了accept来接收了客户端的请求
    //用addr_client来存储了客户端的IP和端口等信息
    //最后用client_len来得到了客户端的实际大小

    //以下开始向客户端发送信息
    char message[]="hello client,I'm server";
    write(socket_client,message,sizeof(message));
    //关闭所使用的套接字
    close(socket_server);
    close(socket_client);
    return 0;
}