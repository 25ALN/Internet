#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/epoll.h>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

const int first_port=21;
const int mes_travel_port=5100;
const int maxevents=100;
const int filebuf_size=4096;

int connect_init();
void error_report(const std::string &x,int fd); //错误处理
void ser_work(int fd,std::string ip);
void deal_new_connect(int ser_fd,int epoll_fd);
void deal_client_data(int data_fd);
void deal_pasv_data(int clinet_fd);

int Recv(int fd,char *buf,int len,int flag);
 
int main(){
    int readyf=0;
    int server_fd=connect_init();
    int epoll_fd=epoll_create(1);

    int flags=fcntl(server_fd,F_GETFL,0);
    fcntl(server_fd,F_SETFL,flags|O_NONBLOCK);

    struct epoll_event ev,events[maxevents];
    ev.data.fd=server_fd;
    ev.events=EPOLLIN|EPOLLET;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_fd,&ev);

    while(true){
        std::cout<<"FTP>: "<<std::endl;
        readyf=epoll_wait(epoll_fd,events,maxevents,-1);

        for(int i=0;i<readyf;i++){
            struct sockaddr_in client_mes;
            if(events[i].data.fd==server_fd){ //处理
                deal_new_connect(server_fd,epoll_fd);
            }else if(events[i].events&&EPOLLIN){ //接受消息
                deal_client_data(events[i].data.fd);
            }
        }
    }
    return 0;
}


void error_report(const std::string &x,int fd){
    std::cout<<x<<" start fail"<<std::endl;
    close(fd);
    exit(1);
}

int connect_init(){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in ser;
    ser.sin_family=AF_INET;
    ser.sin_port=htons(first_port);
    ser.sin_addr.s_addr=INADDR_ANY;

    int bind_fd=bind(fd,(struct sockaddr*)&ser,sizeof(ser));
    if(bind_fd<0){
        error_report("bind",fd);
    }

    int listen_fd=listen(fd,5);
    if(listen_fd<0){
        error_report("listen",fd);
    }
    return fd;
}


void deal_new_connect(int ser_fd,int epoll_fd){
    struct sockaddr_in client_mes;
    socklen_t mes_len=sizeof(client_mes);
    while(true){
        int client_fd=accept(ser_fd,(struct sockaddr*)&client_mes,&mes_len);
        if(client_fd<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                break;
            }else{
                error_report("accept",client_fd);
            }
        }

        int flags=fcntl(client_fd,F_SETFL,O_NONBLOCK);
        fcntl(client_fd,F_SETFL,flags|O_NONBLOCK);
        struct epoll_event cev;
        
        cev.data.fd=client_fd,
        cev.events=EPOLLET|EPOLLIN;
        
        epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&cev);
    }
}

void deal_client_data(int data_fd){
    char ensure[128];
    memset(ensure,'\0',sizeof(ensure));
    int n=Recv(data_fd,ensure,sizeof(ensure),0);
    if(n<0){
        std::cout<<"recv error"<<std::endl;
        close(data_fd);
        return;
    }
    std::string command(ensure,n);
    if(!command.empty()){ //将首尾的空格去除
        command.erase(0,command.find_first_not_of(" "));
        command.erase(command.find_last_not_of(" ")+1);
    }
    if(command.find("PASV")){
        deal_pasv_data(data_fd);
    }
}

void deal_pasv_data(int clinet_fd){
    int data_fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in data;
    data.sin_family=AF_INET;
    data.sin_port=htons(0); //0代表随机端口
    data.sin_addr.s_addr=htonl(INADDR_ANY);

    int bind_fd=bind(clinet_fd,(struct sockaddr*)&data,sizeof(data));
    if(bind_fd<0){
        error_report("bind",clinet_fd);
    }
    
    struct sockaddr_in local_ip;
    socklen_t ip_len=sizeof(local_ip);
    getsockname(clinet_fd,(struct sockaddr*)&local_ip.sin_addr,&ip_len);
    char ip[128];
    inet_ntop(AF_INET,(struct sockaddr*)&local_ip,ip,sizeof(ip));

    std::vector<std::string> call_mes;
    char *temp=strtok(ip,".");
    while(temp!=NULL){
        call_mes.push_back(temp);
        temp=strtok(NULL,".");
    }
    //补充后面两位
    call_mes.push_back(".");
    call_mes.push_back((std::to_string)(local_ip.sin_port/256));
    call_mes.push_back(".");
    call_mes.push_back((std::to_string)(local_ip.sin_port%256));
}

int Recv(int fd,char *buf,int len,int flag){
    int reallen=0;
    while(reallen<len){
        int temp=recv(fd,buf+reallen,len-reallen,flag);
        if(temp<0){
            //数据接收异常
            error_report("recv",fd);
            exit(1);
        }else if(temp==0){
            //数据已全部接受完毕
            break;
        }
        reallen+=temp;
    }
    return reallen;
}

void ser_work(int fd,std::string ip){
    std::cout<<"227 entering passive mode "<<std::endl;
    
}