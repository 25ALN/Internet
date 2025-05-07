#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <memory>
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
int Connect(int fd,const sockaddr* clt,socklen_t clt_len,int mark){
    int error_fd=0,flags,connect_fd;
    socklen_t len;
    flags=fcntl(fd,F_GETFL,0); //��һ����ȡ��ǰ�ļ���������״̬�����ظ���flags
    fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    //���õ�ǰ�ļ���������Ϊ�������ģ�ʹ��|������֮ǰ�ļ���������״̬������ᱻ�µı�־������
    if(connect_fd=connect(fd,clt,len)<0){
        if(error_fd!=EINPROGRESS){
            //�����쳣�������˳�
            exit(1);
        }
    }
    //���׽���������������Ϊ��������������epoll��������

    int revel=getsockopt(fd,SOL_SOCKET,SO_ERROR,&error_fd,&len);
    if(revel<0){
        //�쳣�˳�
        exit(1);
    }else{
        if(error_fd!=0){
            std::cout<<"connect fail"<<std::endl;
            exit(1);
        }else{
            std::cout<<"connect success"<<std::endl;
        }
    }
}

int main(){
    int sockfd=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in clt;
    clt.sin_family=AF_INET;
    clt.sin_addr.s_addr=htonl(INADDR_ANY);
    clt.sin_port=htons(1234);
    if(Connect(sockfd,(struct sockaddr*)&clt,sizeof(clt),0)<0){
        std::cout<<"error"<<std::endl;
        exit(1);
    }
    return 0;
}