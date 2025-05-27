#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <csignal>
#include <sstream>
#include <sys/epoll.h>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <iomanip> 
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

class client_data{
    public:
    int client_fd;  //控制连接所用的描述符
    int listen_fd;   //listen描述符
    int data_fd;    //数据连接所用的描述符
    bool pasv_flag; //pasv模式是否打开的标志
    std::string server_ip;

    client_data(int fd,const std::string &ip):client_fd(fd),listen_fd(-1),data_fd(-1),pasv_flag(false)
    ,server_ip(ip){}
};

const int first_port=2100;
int mes_travel_port=5100;
const int maxevents=100;

int connect_init();
void error_report(const std::string &x,int fd); //错误处理
void ser_work(int fd,std::string ip);
void clean_connect(int fd);

void deal_new_connect(int ser_fd,int epoll_fd);
void deal_pasv_data(int clinet_fd);
void deal_list_data(int data_fd);
void deal_STOR_data(std::shared_ptr<client_data> client,std::string filename);
void deal_RETR_data(std::shared_ptr<client_data> client,std::string filename);
void deal_client_data(int data_fd);

int Recv(int fd,char *buf,int len,int flag);
int Send(int fd,char *buf,int len,int flag);