#include "ser.hpp"

std::unordered_map<int,std::shared_ptr <client_data> > client_message; //利用哈希表来从将信息一一对应起来

int main(){
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
        if(readyf<0){
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
                clean_connect(events[i].data.fd);
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
        int flags=fcntl(client_fd,F_GETFL,O_NONBLOCK);
        fcntl(client_fd,F_SETFL,flags|O_NONBLOCK);
        struct epoll_event cev;
        
        cev.data.fd=client_fd,
        cev.events=EPOLLET|EPOLLIN;
        
        epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&cev);
    }
}

void deal_client_data(int data_fd){
    char ensure[1024];
    memset(ensure,'\0',sizeof(ensure));
    int n=Recv(data_fd,ensure,sizeof(ensure),0);
    std::cout<<"client command:"<<ensure<<std::endl;
    if(n<0){
        std::cout<<"recv error"<<std::endl;
        close(data_fd);
        return;
    }
    std::string command(ensure, n);
    size_t crlf_pos = command.find("\r\n");
    if (crlf_pos != std::string::npos) {
        command = command.substr(0, crlf_pos); // 去除尾部 \r\n
    }
    if(!command.empty()){ //将首尾的空格去除
        command.erase(0,command.find_first_not_of(" "));
        command.erase(command.find_last_not_of(" ")+1);
    }
    auto client=client_message[data_fd];
    if(command.find("PASV")!=std::string::npos){
        deal_pasv_data(data_fd);
    }else if(command.find("LIST")!=std::string::npos){
        std::cout<<"begin LIST"<<std::endl;
        deal_list_data(data_fd);
    }else if(command.find("STOR")!=std::string::npos){
        std::cout<<"begin STOR"<<std::endl;
        deal_STOR_data(client,command.substr(5)); //从第六个字节开始读取文件名称
    }else if(command.find("RETR")!=std::string::npos){
        std::cout<<"begin RETR"<<std::endl;
        deal_RETR_data(client,command.substr(5));
    }else if(command.find("quit")!=std::string::npos){
        std::cout<<"server quit connect"<<std::endl;
        clean_connect(data_fd);
        exit(0);
    }
    else{
        std::cout<<"没有找到相关命令"<<std::endl;
        return;
    }
}

void deal_pasv_data(int client_fd){
    
    auto client = client_message[client_fd];
    std::string server_ip = client->server_ip; // 使用保存的服务端IP
    //创建监听套接字
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return;
    }
    int flags=fcntl(listen_fd,F_GETFL,0);
    fcntl(listen_fd,F_SETFL,flags|O_NONBLOCK); 
    // 设置端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0); // 随机分配一个端口
    inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return;
    }
    socklen_t addrlen=sizeof(addr);
    getsockname(listen_fd,(struct sockaddr*)&addr,&addrlen);
    mes_travel_port=ntohs(addr.sin_port);
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
    if(x<=0){
        error_report("send",client_fd);
        return;
    }
}

void deal_list_data(int data_fd){
    std::string listmes;
    std::ostringstream oss;
    struct store{
    char name[1024];
    long time;
    };
    char c[1024];
    getcwd(c,sizeof(c));
    char *dir_path=new char[1024];
    strncpy(dir_path,c,1023);
    const char *a="/";
    strncat(dir_path,a,1);
    dir_path[1023]='\0';
    DIR *dirr = opendir(dir_path);
    if (dirr == NULL){
        perror("fail open dir");
        return;
    }
    char path[10240];
    char rwx[11];
    int j = 0, filenum = 0;
    struct dirent *r;
    struct stat filestat[1024];
    struct store s[1024];
    while ((r = readdir(dirr)) != NULL){
        if (r->d_name[0] == '.')
        {
            continue;
        }
        strcpy(path, dir_path);
        strncat(path, r->d_name, strlen(r->d_name));
        if (stat(path, &filestat[filenum]) == -1)
        {
            perror("files fail");
            continue;
        }
        strcpy(s[filenum].name, r->d_name);
        s[filenum].time = filestat[filenum].st_mtime;
        filenum++;
    }
    for(int i=0;i<filenum;i++){
        mode_t file = filestat[i].st_mode;
        if (S_ISDIR(file))
            rwx[0] = 'd';
        else if (S_ISLNK(file))
            rwx[0] = 'l';
        else
            rwx[0] = '-';
        rwx[1] = (file & S_IRUSR) ? 'r' : '-';
        rwx[2] = (file & S_IWUSR) ? 'w' : '-';
        rwx[3] = (file & S_IXUSR) ? 'x' : '-';
        rwx[4] = (file & S_IRGRP) ? 'r' : '-';
        rwx[5] = (file & S_IWGRP) ? 'w' : '-';
        rwx[6] = (file & S_IXGRP) ? 'x' : '-';
        rwx[7] = (file & S_IROTH) ? 'r' : '-';
        rwx[8] = (file & S_IWOTH) ? 'w' : '-';
        rwx[9] = (file & S_IXOTH) ? 'x' : '-';
        rwx[10] = '\0';
        struct passwd *user = getpwuid(filestat[i].st_uid);
        struct group *gro = getgrgid(filestat[i].st_gid);
        time_t mod_time = filestat[i].st_mtime;
        struct tm *tt = localtime(&mod_time);
        char ctime[100];
        strftime(ctime, sizeof(ctime), "%m月 %d %H:%M", tt);
        oss << std::left << std::setw(11) << rwx << " "
            << std::setw(2) << filestat[i].st_nlink << " "
            << std::left << std::setw(8) << (user ? user->pw_name : "unknown") << " "
            << std::left << std::setw(8) << (gro ? gro->gr_name : "unknown") << " "
            << std::setw(5) << filestat[i].st_size << " "
            << ctime << " "
            << s[i].name << "\n";
    }
    listmes=oss.str();
    Send(data_fd,const_cast<char *>(listmes.c_str()),listmes.size(),0);
    delete []dir_path;
    closedir(dirr);
}

void deal_RETR_data(std::shared_ptr<client_data> client,std::string filename){
    std::string response = "150 Opening data connection\r\n";
    Send(client->client_fd, const_cast<char*>(response.c_str()), response.size(), 0);
    struct sockaddr_in clmes;
    socklen_t len=sizeof(clmes);
    int data_fd=-1;
    while (true) {
        data_fd = accept(client->listen_fd, (struct sockaddr*)&clmes, &len);
        if (data_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下无连接，稍后重试
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                perror("accept error");
                return;
            }
        } else {
            break;
        }
    }
    if(data_fd<0){
        perror("accept data connection");
        return;
    }
    client->data_fd=data_fd;
    char c[1024];
    getcwd(c,sizeof(c));
    std::string x=(std::string)c+"/"+filename;
    std::cout<<x<<std::endl;
    FILE *fp=fopen(x.c_str(),"rb");
    if(fp==nullptr){
        std::cout<<"file open fail "<<std::endl;
        close(data_fd);
        client->data_fd=-1;
        return;
    }
    std::cout<<"开始上传文件"<<std::endl;
    while (true) {
        char buf[4096];
        ssize_t n = fread(buf, 1, sizeof(buf), fp);
        if (n <= 0) break;
        ssize_t sent = send(data_fd, buf, n, 0);
        if (sent < 0) break;
    }
    std::cout<<"文件上传完毕"<<std::endl;
    fclose(fp);
    shutdown(client->data_fd,SHUT_RDWR);
    close(data_fd);
    client->data_fd=-1;
}

void deal_STOR_data(std::shared_ptr<client_data> client, std::string filename) {
    // 1. 发送150响应
    std::string response = "150 Opening data connection\r\n";
    Send(client->client_fd, const_cast<char*>(response.c_str()), response.size(), 0);
    // 2. 接受数据连接（确保非阻塞模式正确处理）
    int data_fd = -1;
    struct sockaddr_in clmes;
    socklen_t len = sizeof(clmes);
    while (true) {
        data_fd = accept(client->listen_fd, (struct sockaddr*)&clmes, &len);
        if (data_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下无连接，稍后重试
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                perror("accept error");
                return;
            }
        } else {
            break;
        }
    }
    client->data_fd = data_fd;

    // 3. 接收文件数据
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        perror("fopen failed");
        close(data_fd);
        return;
    }
    ssize_t total = 0;
    while (true) {
        char buf[10000];
        ssize_t n = recv(data_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            fwrite(buf, 1, n, fp);
            total += n;
        } else if (n == 0) {
            break; // 连接关闭
        } else if (errno == EAGAIN) {
            continue; // 非阻塞模式下重试
        } else {
            perror("recv error");
            break;
        }
    }
    std::cout<<"文件接收完毕"<<std::endl;
    fclose(fp);
    close(data_fd);
    client->data_fd = -1;
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
        int temp = recv(fd, buf + reallen, len - reallen, flag);
        if (temp > 0) {
            reallen += temp;
        } else if (temp == 0) { // 连接关闭
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // 非阻塞模式下无更多数据，退出循环
            } else {
                error_report("recv", fd);
                return -1;
            }
        }
    }
    return reallen;
}