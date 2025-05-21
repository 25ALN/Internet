#include "ser.hpp"



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
    act.sa_flags=SA_RESTART|SA_NOCLDWAIT;
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
        readyf=epoll_wait(epoll_fd,events,maxevents,-1);
        if(errno<0){
            perror("epoll_wait");
            break;
        }
        for(int i=0;i<readyf;i++){
            struct sockaddr_in client_mes;
            if(events[i].data.fd==server_fd){ //处理
                deal_new_connect(server_fd,epoll_fd);
            }else if(events[i].events&EPOLLIN){ //接受消息
                deal_client_data(events[i].data.fd);
            }else if(events[i].events&(EPOLLERR|EPOLLHUP)){
                clean_connect(server_fd);
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
    int contain;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&contain,sizeof(int)); //设置端口复用
    struct sockaddr_in ser;
    ser.sin_family=AF_INET;
    ser.sin_port=htons(first_port);
    ser.sin_addr.s_addr=htonl(INADDR_ANY);

    int bind_fd=bind(fd,(struct sockaddr*)&ser,sizeof(ser));

    if(bind_fd<0){
        error_report("bind",fd);
    }

    int listen_fd=listen(fd,10);
    if(listen_fd<0){
        error_report("listen",fd);
    }
    
    return fd;
}

void deal_new_connect(int ser_fd,int epoll_fd){
    struct sockaddr_in client_mes;
    socklen_t mes_len = sizeof(client_mes);
    while (true) {
        int client_fd = accept(ser_fd, (struct sockaddr*)&client_mes, &mes_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else error_report("accept", client_fd);
        }

        // 获取服务端本地IP地址
        struct sockaddr_in server_side_addr;
        socklen_t addr_len = sizeof(server_side_addr);
        getsockname(client_fd, (struct sockaddr*)&server_side_addr, &addr_len);
        std::string server_ip = inet_ntoa(server_side_addr.sin_addr);

        std::string client_ip = inet_ntoa(client_mes.sin_addr);
        auto client = std::make_shared<client_data>(client_fd, client_ip);
        client->server_ip = server_ip; 
        client_message[client_fd] = client;
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
    auto client=client_message[data_fd];
    if(command.find("PASV")!=std::string::npos){
        deal_pasv_data(data_fd);
    }else if(command.find("LIST")!=std::string::npos){
        deal_list_data(data_fd);
    }else if(command.find("STOR")!=std::string::npos){
        deal_STOR_data(client,command.substr(5)); //从第六个字节开始读取文件名称
    }else if(command.find("RETR")!=std::string::npos){
        deal_RETR_data(client,command.substr(5));
    }
}

void deal_pasv_data(int client_fd){
    
    auto client = client_message[client_fd];
    std::string server_ip = client->server_ip; // 使用保存的服务端IP

    // 创建监听套接字
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return;
    }
    
    // 设置端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(mes_travel_port); // 使用预设端口
    inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr);
    
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return;
    }
    
    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        close(listen_fd);
        return;
    }
    client->listen_fd = listen_fd;

    std::vector<std::string> call_mes;
    std::istringstream iss(server_ip);
    std::string part;
    while (getline(iss, part, '.')) {
        call_mes.push_back(part);
    }
    //补充后面两位
    std::ostringstream bc;
    bc<<"227 Entering Passive Mode (";
    for(int i=0;i<call_mes.size();i++){
        bc << call_mes[i];
        if (i != call_mes.size() - 1) bc << ",";
    }
    bc<<","<<(mes_travel_port/256)<<","<<(mes_travel_port%256)<<")\r\n";
    std::string mesg=bc.str();
    std::cout<<mesg<<std::endl;
    int x=Send(client_fd,const_cast<char *>(mesg.c_str()),strlen(mesg.c_str()),0);
}

void deal_list_data(int data_fd){
    pid_t pid=fork();
    if(pid==-1){
        perror("fork");
        return;
    }else if(pid==0){
        char dir[]="/home/aln/桌面/Internet/ftp/";
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

void deal_RETR_data(std::shared_ptr<client_data> client,std::string filename){
    int data_fd=accept(client->listen_fd,nullptr,nullptr);
    if(data_fd<0){
        perror("accept data connection");
        return;
    }
    client->data_fd=data_fd;
    FILE *fp=fopen(filename.c_str(),"rb");
    if(fp==nullptr){
        std::cout<<"file open fail "<<std::endl;
        close(data_fd);
        client->data_fd=-1;
        return;
    }
    char buf[4096];
    memset(buf,'\0',sizeof(buf));
    while(size_t n=fread(buf,sizeof(buf),1,fp)){ //每次读取sizeof(buf)个元素，大小为1字节，返回读取元素的个数，直到读取的元素个数为1为止
        send(data_fd,buf,sizeof(buf),0);
    }
    fclose(fp);
    shutdown(client->data_fd,SHUT_RDWR);
    close(data_fd);
    client->data_fd=-1;
}

void deal_STOR_data(std::shared_ptr<client_data> client,std::string filename){
    int data_fd=accept(client->listen_fd,nullptr,nullptr);
    if(data_fd<0){
        perror("accept data connection");
        return;
    }
    client->data_fd=data_fd;
    FILE *fp=fopen(filename.c_str(),"wb");
    if(fp==nullptr){
        std::cout<<"file open fail "<<std::endl;
        close(data_fd);
        client->data_fd=-1;
        return;
    }
    char buf[4096];
    memset(buf,'\0',sizeof(buf));
    while(size_t n=recv(data_fd,buf,sizeof(buf),0)){
        fwrite(buf,1,n,fp);
    }
    fclose(fp);
    shutdown(client->data_fd,SHUT_RDWR);
    close(client->data_fd);
    client->data_fd=-1;
}

void clean_connect(int fd){
    if(client_message.count(fd)){
        auto &client=client_message[fd];
        if(client->listen_fd!=-1){
            shutdown(client->listen_fd,SHUT_RDWR);  //先调用这个可以防止资源泄漏，数据为还未传输完就close会导致数据一直阻塞下去
            close(client->listen_fd);
            client->listen_fd=-1;
        }
        if(client->data_fd!=-1){
            shutdown(client->data_fd,SHUT_RDWR);
            close(client->data_fd);
            client->data_fd=-1;
        }
        client_message.erase(fd); //从哈希表中移除
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
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return reallen; // 非阻塞模式下数据未就绪，直接返回已接收长度
            }
            error_report("recv",fd);
        }else if(temp==0){
            //数据已全部接受完毕
            break;
        }
        reallen+=temp;
    }
    return reallen;
}