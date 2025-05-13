#include "ser.hpp"

class client_data{
    public:
    int client_fd;  //控制连接所用的描述符
    int listen_fd;   //listen描述符
    int data_fd;    //数据连接所用的描述符
    bool pasv_flag; //pasv模式是否打开的标志
    std::string client_ip;

    client_data(int fd,const std::string &ip):client_fd(fd),listen_fd(-1),data_fd(-1),pasv_flag(false)
    ,client_ip(ip){}

};

//信号处理函数，在处理LIST命令时设置waitpid为非在阻塞的
void handle(int sig){
    int status;
    pid_t pid;
    while((pid=waitpid(-1,&status,WNOHANG))>0);
    //-1代表等待任意的子进程，WNOHANG代表非阻塞模式
}


std::unordered_map<int,std::shared_ptr <client_data> > client_message; //利用哈希表来从将信息一一对应起来

int main(){
    struct sigaction act;
    act.sa_handler=handle;
    sigemptyset(&act.sa_mask); 
    act.sa_flags=SA_RESTART;
    if(sigaction(SIGCHLD,&act,NULL)==-1){
        perror("sigaction");
        return 1;
    }
    int readyf=0; //记录准备好事件的变量
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
        if(errno<0){
            perror("epoll_wait");
            break;
        }
        for(int i=0;i<readyf;i++){
            struct sockaddr_in client_mes;
            if(events[i].data.fd==server_fd){ //处理
                deal_new_connect(server_fd,epoll_fd);
            }else if(events[i].events&&EPOLLIN){ //接受消息
                deal_client_data(events[i].data.fd);
            }else{

            }
        }
    }

    for(auto&[fd,client]:client_message){
        clean_connect(fd);
    }

    shutdown(server_fd,SHUT_RDWR);
    close(server_fd);
    close(epoll_fd);
    return 0;
}


void error_report(const std::string &x,int fd){
    std::cout<<x<<" start fail"<<std::endl;
    close(fd);
}

int connect_init(){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in ser;
    ser.sin_family=AF_INET;
    ser.sin_port=htons(first_port);
    ser.sin_addr.s_addr=INADDR_ANY;

    int contain;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&contain,sizeof(int)); //设置端口复用

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
    if(command.find("PASV")!=std::string::npos){
        deal_pasv_data(data_fd);
    }else if(command.find("LIST")!=std::string::npos){
        deal_list_data(data_fd);
    }else if(command.find("STOR")){
        deal_STOR_data(data_fd);
    }
}

void deal_pasv_data(int clinet_fd){
    int data_fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in data;
    data.sin_family=AF_INET;
    data.sin_port=htons(0); //0代表随机端口
    data.sin_addr.s_addr=htonl(INADDR_ANY);

    int bind_fd=bind(data_fd,(struct sockaddr*)&data,sizeof(data));
    if(bind_fd<0){
        error_report("bind",data_fd);
    }
    
    int listen_fd=listen(data_fd,5);
    if(listen_fd<0){
        error_report("listen",data_fd);
    }

    struct sockaddr_in local_ip;
    socklen_t ip_len=sizeof(local_ip);
    getsockname(data_fd,(struct sockaddr*)&local_ip,&ip_len);
    uint16_t real_port=ntohs(local_ip.sin_port);
    char ip[128];
    inet_ntop(AF_INET,(struct sockaddr*)&local_ip,ip,sizeof(ip));

    std::vector<std::string> call_mes;
    call_mes.push_back("227 entering passive mode ");
    char *temp=strtok(ip,".");
    int point=0;
    while(temp!=NULL){
        call_mes.push_back(temp);
        temp=strtok(NULL,".");
        point++;
    }
    //补充后面两位
    call_mes.push_back(",");
    call_mes.push_back((std::to_string)(real_port/256));
    call_mes.push_back(",");
    call_mes.push_back((std::to_string)(real_port%256));
    call_mes.push_back("\r\n");
    char mesg[258];
    memset(mesg,'\0',sizeof(mesg));
    const char *p=",";
    for(auto x:call_mes){
        strncat(mesg,x.data(),x.size());
        if(--point!=0){
        strncat(mesg,p,1);
        }
    }
    int x=Send(clinet_fd,mesg,strlen(mesg),0);

}

void deal_list_data(int data_fd){
    pid_t pid=fork();
    if(pid==-1){
        perror("fork");
        return;
    }else if(pid==0){
        char dir[128];
        memset(dir,'\0',sizeof(dir));
        getcwd(dir,sizeof(dir));
        std::vector<std::string> order{"ls",dir};
        std::vector<char *> zx_order;
        for(auto x:order){
            zx_order.push_back(const_cast<char *>(x.c_str()));
            //x.c_ctr()返回值是const char*的，因此需要使用const_cast将const去除
        }
        zx_order.push_back(nullptr);
        execvp(zx_order[0],zx_order.data());
    }
    //之后由开局的信号处理函数父进程直接返回
}

void deal_STOR_data(std::shared_ptr<client_data> client,const std::string filename){
    int data_fd=accept(client->listen_fd,nullptr,nullptr);
    FILE *fp=fopen(filename.c_str(),"wb");
}

void clean_connect(int fd){
    if(client_message.count(fd)){
        auto &client=client_message[fd];
        if(client->listen_fd!=-1){
            shutdown(client->listen_fd,SHUT_RDWR);  //先调用这个可以防止资源泄漏，数据为还未传输完就close会导致数据一直阻塞下去
            close(client->listen_fd);
        }
        if(client->data_fd!=-1){
            shutdown(client->data_fd,SHUT_RDWR);
            close(client->data_fd);
        }
        client_message.erase(fd);
    }
    shutdown(fd,SHUT_RDWR);
    close(fd);
}

int Send(int fd,char *buf,int len,int flag){
    int reallen=0;
    while(reallen<len){
        int temp=send(fd,buf+reallen,len-reallen,flag);
        if(temp<0){
            error_report("send",fd);
        }else if(temp==0){
            //数据已全部发送完毕
            break;
        }
        reallen+=temp;
    }
    return reallen;
}

int Recv(int fd,char *buf,int len,int flag){
    int reallen=0;
    while(reallen<len){
        int temp=recv(fd,buf+reallen,len-reallen,flag);
        if(temp<0){
            //数据接收异常
            error_report("recv",fd);
        }else if(temp==0){
            //数据已全部接受完毕
            break;
        }
        reallen+=temp;
    }
    return reallen;
}