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
#include <netinet/in.h> //sockaddr_in, htons() ��
#include <arpa/inet.h> //inet_pton(), inet_ntoa() ��
#include <time.h> //��ȡʱ��

// int main(){
//     int connectfd;
//     time_t ticks;
//     char buf[100];
//     struct sockaddr_in savetime;
//     int sockfd=socket(AF_INET,SOCK_STREAM,0);
//     savetime.sin_family=AF_INET;
//     savetime.sin_port=htons(13);
//     savetime.sin_addr.s_addr=htonl(INADDR_ANY); //INADDR_ANY��ʾ�����Ȿ��IP��ַ

//     bind(sockfd,(struct sockaddr*)&savetime,sizeof(savetime));
//     listen(sockfd,5);

//     //һ��ֻ�ܴ���һ���ͻ��ˣ��Ҵ������һֱ�ȴ�����һ���ͻ��˵�����
//     while(1){
//         connectfd=accept(sockfd,(struct sockaddr*)NULL,NULL);
//         ticks=time(NULL);
//         snprintf(buf,strlen(buf),"%24s\n",ctime(&ticks));
//         write(connectfd,buf,sizeof(buf));
//         close(connectfd);
//     }
    
//     return 0;
// }

//�����ǰϵͳ�� TCP �� UDP �׽��ֵ�Ĭ�Ϸ��ͣ�send buffer���ͽ��գ�receive buffer����������С

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
//     //getsockopt������ϸ�ɼ�p151ҳ������ϸ
//     //��ʾ�����õĲ������ǻ�ȡ���ͺͻ�������С��
//     std::cout<<rl<<" byte"<<std::endl;

//     std::cout<<type<<" send buffer is:";
//     int sebuf=getsockopt(fd,SOL_SOCKET,SO_SNDBUF,&sl,&len);
//     std::cout<<sl<<" byte"<<std::endl;
// }

// getsockopt test ������connect
// int Connect(int fd,const sockaddr* clt,socklen_t clt_len,int mark){
//     int error_fd=0,flags,connect_fd;
//     socklen_t len;
//     flags=fcntl(fd,F_GETFL,0); //��һ����ȡ��ǰ�ļ���������״̬�����ظ���flags
//     fcntl(fd,F_SETFL,flags|O_NONBLOCK);
//     //���õ�ǰ�ļ���������Ϊ�������ģ�ʹ��|������֮ǰ�ļ���������״̬������ᱻ�µı�־������
//     if(connect_fd=connect(fd,clt,len)<0){
//         if(error_fd!=EINPROGRESS){
//             //�����쳣�������˳�
//             exit(1);
//         }
//     }
//     //���׽���������������Ϊ��������������epoll��������

//     int revel=getsockopt(fd,SOL_SOCKET,SO_ERROR,&error_fd,&len);
//     if(revel<0){
//         //�쳣�˳�
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

//��������accept���������߳�ͬʱ����
//ʹ��һ�����߳�������accept�������ܵ���Ϣ�����̳߳��н��й���
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
//     //���ｫ�������������߳�
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
//             //���ｫ��������̳߳ؿ�ʼִ������
//         }
//         hx.notify_one(); //��ʼ����
//     }
// }

// epoll���ӣ�ʡ���˷������о���Ĳ�����bind,listen
int main(){
    int fd=epoll_create(1); //������һ��epoll��ʵ��
    struct epoll_event ev,events[20];
    int server_fd,ready=0,connect_fd; 

    ev.data.fd=server_fd; //�õ����¼���������
    ev.events=EPOLLIN|EPOLLET; //���ø��¼�Ϊ�ɶ������л�Ϊ��Ե����ģʽ

    int reval=epoll_ctl(fd,EPOLL_CTL_ADD,server_fd,&ev); 
    if(reval<0){
        std::cout<<"epoll_ctl fail"<<std::endl;
        exit(1);
    }

    while(true){
        ready=epoll_wait(fd,events,20,0); //�õ���׼���õ��¼�����
        for(int i=0;i<ready;i++){
            if(server_fd==events[i].data.fd){ //����⵽���µ����Ӻ󣬿�ʼע���¼�
            //����accept,���ҽ�����׽�������Ϊ������ģʽ
            // ���ҽ�����µ��¼����뵽epoll�ļ���������ȥ

            ev.data.fd=connect_fd;
            ev.events=EPOLLIN|EPOLLET;
            epoll_ctl(fd,EPOLL_CTL_ADD,connect_fd,&ev);
            }
            // ����ͽ�����ͻ��˾������Ϣ����
        }
    }
    return 0;
}