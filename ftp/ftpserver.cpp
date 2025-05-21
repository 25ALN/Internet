#include "ser.hpp"



//�źŴ��������ڴ���LIST����ʱ����waitpidΪ����������
void handle(int sig){
    int status;
    pid_t pid;
    while((pid=waitpid(-1,&status,WNOHANG))>0);
    //-1����ȴ�������ӽ��̣�WNOHANG���������ģʽ
}


std::unordered_map<int,std::shared_ptr <client_data> > client_message; //���ù�ϣ�����ӽ���Ϣһһ��Ӧ����

int main(){
    struct sigaction act;
    act.sa_handler=handle;
    sigemptyset(&act.sa_mask); 
    act.sa_flags=SA_RESTART|SA_NOCLDWAIT;
    if(sigaction(SIGCHLD,&act,NULL)==-1){
        perror("sigaction");
        return 1;
    }
    int readyf=0; //��¼׼�����¼��ı���
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
            if(events[i].data.fd==server_fd){ //����
                deal_new_connect(server_fd,epoll_fd);
            }else if(events[i].events&EPOLLIN){ //������Ϣ
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
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&contain,sizeof(int)); //���ö˿ڸ���
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

        // ��ȡ����˱���IP��ַ
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
    if(!command.empty()){ //����β�Ŀո�ȥ��
        command.erase(0,command.find_first_not_of(" "));
        command.erase(command.find_last_not_of(" ")+1);
    }
    auto client=client_message[data_fd];
    if(command.find("PASV")!=std::string::npos){
        deal_pasv_data(data_fd);
    }else if(command.find("LIST")!=std::string::npos){
        deal_list_data(data_fd);
    }else if(command.find("STOR")!=std::string::npos){
        deal_STOR_data(client,command.substr(5)); //�ӵ������ֽڿ�ʼ��ȡ�ļ�����
    }else if(command.find("RETR")!=std::string::npos){
        deal_RETR_data(client,command.substr(5));
    }
}

void deal_pasv_data(int client_fd){
    
    auto client = client_message[client_fd];
    std::string server_ip = client->server_ip; // ʹ�ñ���ķ����IP

    // ���������׽���
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return;
    }
    
    // ���ö˿ڸ���
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(mes_travel_port); // ʹ��Ԥ��˿�
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
    //���������λ
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
        char dir[]="/home/aln/����/Internet/ftp/";
        std::vector<std::string> order{"ls",dir};
        std::vector<char *> zx_order;
        for(auto x:order){
            zx_order.push_back(const_cast<char *>(x.c_str()));
            //x.c_ctr()����ֵ��const char*�ģ������Ҫʹ��const_cast��constȥ��
        }
        zx_order.push_back(nullptr);
        execvp(zx_order[0],zx_order.data());
    }
    //֮���ɿ��ֵ��źŴ�����������ֱ�ӷ���
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
    while(size_t n=fread(buf,sizeof(buf),1,fp)){ //ÿ�ζ�ȡsizeof(buf)��Ԫ�أ���СΪ1�ֽڣ����ض�ȡԪ�صĸ�����ֱ����ȡ��Ԫ�ظ���Ϊ1Ϊֹ
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
            shutdown(client->listen_fd,SHUT_RDWR);  //�ȵ���������Է�ֹ��Դй©������Ϊ��δ�������close�ᵼ������һֱ������ȥ
            close(client->listen_fd);
            client->listen_fd=-1;
        }
        if(client->data_fd!=-1){
            shutdown(client->data_fd,SHUT_RDWR);
            close(client->data_fd);
            client->data_fd=-1;
        }
        client_message.erase(fd); //�ӹ�ϣ�����Ƴ�
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
            //������ȫ���������
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
            //���ݽ����쳣
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return reallen; // ������ģʽ������δ������ֱ�ӷ����ѽ��ճ���
            }
            error_report("recv",fd);
        }else if(temp==0){
            //������ȫ���������
            break;
        }
        reallen+=temp;
    }
    return reallen;
}