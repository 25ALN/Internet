#include "cli.hpp"

int main()
{
    client_fd = connect_init();
    if (client_fd < 0)
    {
        error_report("socket", client_fd);
    }
    start_PASV_mode(client_fd, "PASV\r\n");
    char repose[1024];
    memset(repose, '\0', sizeof(repose));
    std::cout << "ready recv" << std::endl;
    int n = Recv(client_fd, repose, sizeof(repose), 0);
    if (n <= 0){
        clean_connect(client_fd);
        return 0;
    }
    std::cout << "server PASV reponse:" << repose;
    std::string cmd(repose, n);
    std::cout << "cmd=" << cmd << std::endl;
    if (cmd.find("227 Entering Passive Mode") != std::string::npos){
        get_ip_port(cmd);
    }
    else{
        error_report("PASV mode", client_fd);
    }
    while (true){
        char message[1024];
        memset(message, '\0', sizeof(message));
        std::cin.getline(message, sizeof(message));
        deal_willsend_message(client_fd, message);
    }
    clean_connect(client_fd);
    return 0;
}
void start_PASV_mode(int fd, std::string first_m){
    if (!first_m.empty()){
        int n = Send(fd, first_m.c_str(), first_m.size(), 0);
    }
}
void deal_willsend_message(int fd, char m[1024])
{
    std::string mes = (std::string)m;
    if (mes.find("PASV") != std::string::npos){
        std::thread x(deal_send_message, fd, mes);
        x.detach();
    }
    else if (mes.find("LIST") != std::string::npos){
        std::thread x([fd]()
        {
            deal_list_data(fd); // 新增函数，用于接收列表数据
        });
        x.detach();
    }
    else if (mes.find("STOR") != std::string::npos){
        std::string stor_command = "STOR " + mes.substr(5) + "\r\n";
        Send(fd, stor_command.c_str(), stor_command.size(), 0);
        // 启动线程处理文件传输
        std::thread x([fd, mes]()
        { deal_up_file(mes.substr(5), fd); });
        x.detach();
    }
    else if (mes.find("RETR") != std::string::npos){
        std::string retr_command = "RETR " + mes.substr(5) + "\r\n";
        Send(fd, retr_command.c_str(), retr_command.size(), 0);
        std::thread x([fd, mes]()
        { deal_get_file(mes.substr(5), fd); });
        x.detach();
    }
    else if (mes.find("quit") != std::string::npos){
        std::cout << "client quit connect" << std::endl;
        std::thread x(deal_send_message, fd, mes);
        x.detach();
        sleep(1);
        clean_connect(fd);
        exit(0);
    }
    else{
        std::cout << "not find this command" << std::endl;
        return;
    }
}
void deal_list_data(int control_fd){
    const char *listm="LIST\r\n";
    Send(control_fd,listm,strlen(listm),0);
    char listmes[4096];
    memset(listmes,'\0',sizeof(listmes));
    Recv(control_fd,listmes,sizeof(listmes),0);
    std::cout<<listmes<<std::endl;
}
void deal_send_message(int fd, std::string m)
{
    if (!m.empty())
    {
        Send(fd, m.c_str(), m.size(), 0);
    }
}
void error_report(const std::string &x, int fd)
{
    std::cout << x << " start fail" << std::endl;
    close(fd);
    exit(1);
}

int connect_init()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        error_report("socket", fd);
    }
    struct sockaddr_in client_mess;
    client_mess.sin_family = AF_INET;
    client_mess.sin_port = htons(first_port);
    client_mess.sin_addr.s_addr = htonl(INADDR_ANY);
    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        error_report("fcntl", fd);
    }
    int connect_fd = connect(fd, (struct sockaddr *)&client_mess, sizeof(client_mess));
    if (connect_fd < 0 && errno != EINPROGRESS)
    {
        error_report("connect", fd);
    }

    return fd;
}

void get_ip_port(std::string ser_mesage)
{
    std::string ip;
    int cnt = 0;
    int start = ser_mesage.find("(");
    int end = ser_mesage.find(")");
    std::string mes = ser_mesage.substr(start + 1, end - start - 1);
    int douhao = mes.find(",");
    std::string p1, p2;
    while (cnt != 4)
    {
        for (int i = 0; i < douhao; i++)
        {
            ip += mes[i];
        }
        if (cnt != 3)
        {
            ip += '.';
            mes.erase(0, douhao + 1);
            douhao = mes.find(",");
        }
        cnt++;
    }
    mes.erase(0, douhao + 1);
    for (auto j : mes)
    {
        if (j != ',' && cnt != 4)
        {
            p1 += j;
        }
        else if (j != ')' && j != ',')
        {
            p2 += j;
        }
        else if (j == ',')
        {
            cnt++;
        }
    }
    IP = ip;
    data_port = std::stoi(p2) * 256 + std::stoi(p1);
}

void deal_get_file(std::string filename, int fd)
{
    int data_fd = socket(AF_INET, SOCK_STREAM, 0);
    int flag = fcntl(data_fd, F_GETFL, 0);
    fcntl(data_fd, F_SETFL, flag | O_NONBLOCK);
    if (data_fd < 0)
    {
        error_report("socket", data_fd);
    }
    struct sockaddr_in filedata;
    filedata.sin_family = AF_INET;
    filedata.sin_port = htons(data_port);
    if (IP.empty())
    {
        return;
    }
    if (inet_pton(AF_INET, IP.c_str(), &filedata.sin_addr) < 0)
    {
        std::cout << "inet_pton fail" << std::endl;
        return;
    }
    int m = connect(data_fd, (struct sockaddr *)&filedata, sizeof(filedata));
    if (m < 0 && errno != EINPROGRESS)
    {
        std::cout << "connect fail" << std::endl;
        return;
    }

    // 使用select等待连接完成
    fd_set set;
    FD_ZERO(&set);
    FD_SET(data_fd, &set);
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (select(data_fd + 1, NULL, &set, NULL, &timeout) <= 0)
    {
        std::cout << "Connection timeout" << std::endl;
        close(data_fd);
        return;
    }

    if (filename.empty())
    {
        return;
    }
    FILE *fp = fopen(filename.c_str(), "wb");
    if (fp == nullptr)
    {
        std::cout << "file open fail " << std::endl;
        shutdown(fd, SHUT_RDWR);
        close(data_fd);
        fclose(fp);
        return;
    }
    std::cout << "开始接收文件" << std::endl;
    while (true) {
        char buf[4096];
        ssize_t n = recv(data_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            fwrite(buf, 1, n, fp);
        } else if (n == 0) {
            break; // 连接关闭
        } else if (errno == EAGAIN) {
            continue; // 非阻塞模式下重试
        } else {
            perror("recv error");
            break;
        }
    }
    std::cout << "文件接收完毕"<<std::endl;
    shutdown(data_fd, SHUT_RDWR);
    close(data_fd);
    fclose(fp);
}

void deal_up_file(std::string filename, int control_fd)
{
    // 1. 创建数据套接字并连接到服务端数据端口
    int data_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (data_fd < 0){
        error_report("socket", data_fd);
        return;
    }
    // 设置为非阻塞模式
    int flags = fcntl(data_fd, F_GETFL, 0);
    fcntl(data_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(data_port);
    inet_pton(AF_INET, IP.c_str(), &data_addr.sin_addr);

    // 发起非阻塞连接
    int connect_ret = connect(data_fd, (struct sockaddr *)&data_addr, sizeof(data_addr));
    if (connect_ret < 0 && errno != EINPROGRESS)
    {
        error_report("connect", data_fd);
        close(data_fd);
        return;
    }

    // 使用select等待连接完成
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(data_fd, &writefds);
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int sel_ret = select(data_fd + 1, NULL, &writefds, NULL, &timeout);
    if (sel_ret <= 0)
    {
        std::cout << "Data connection timeout" << std::endl;
        close(data_fd);
        return;
    }

    // 2. 发送文件数据
    FILE *fp = fopen(filename.c_str(), "rb");
    if (!fp)
    {
        perror("fopen failed");
        close(data_fd);
        return;
    }
    char buf[4096];
    size_t bytes_read;
    std::cout<<"开始上传文件"<<std::endl;
    while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0){
        size_t alreay_send=0;
        while(alreay_send<bytes_read){
            ssize_t sent = send(data_fd, buf, bytes_read, 0);
            if(sent>0){
                alreay_send+=sent;
            }else if(sent < 0){
                if(errno==EAGAIN||errno==EWOULDBLOCK){
                    continue;
                }else{
                    perror("send error");
                    break;
                }
            }
        }
        if(alreay_send<bytes_read){
            break;
        }
    }
    std::cout<<"文件上传完毕"<<std::endl;
    // 3. 关闭数据连接
    shutdown(data_fd, SHUT_WR);
    close(data_fd);
    fclose(fp);
}

int Recv(int fd, char *buf, int len, int flags)
{
    int reallen = 0;
    fd_set set;
    struct timeval timeout;
    while (reallen < len)
    {
        int temp = recv(fd, buf + reallen, len - reallen, flags);
        if (temp < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 数据未就绪，等待可读事件
                FD_ZERO(&set);
                FD_SET(fd, &set);
                timeout.tv_sec=2;
                timeout.tv_usec=0;
                if (select(fd + 1, &set, NULL, NULL, &timeout) <= 0)
                {
                    std::cout << "Recv timeout" << std::endl;
                    return reallen;
                }
                continue; // 重新尝试 recv
            }
            error_report("recv", fd);
            return -1;
        }
        else if (temp == 0)
        {
            break;
        }
        reallen += temp;
    }
    return reallen;
}

int Send(int fd, const char *buf, int len, int flags)
{
    int reallen = 0;
    while (reallen < len)
    {
        int temp = send(fd, buf + reallen, len - reallen, flags);
        if (temp < 0)
        {
            error_report("send", fd);
        }
        else if (temp == 0)
        {
            // 数据已全部发送完毕
            return reallen;
        }
        reallen += temp;
    }
    return reallen;
}

void clean_connect(int fd)
{
    if (fd != -1)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        fd = -1;
    }
}