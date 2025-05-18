#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <csignal>
#include <sys/epoll.h>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h> //sockaddr_in, htons() ?
#include <arpa/inet.h> //inet_pton(), inet_ntoa() ?

struct ser_mes{
    int fd;
    std::vector<char> buf;
    size_t real_len;
};
std::unordered_map<int,ser_mes> ser_msave;

const int first_port=2100;
const int maxt=100;
int client_fd=-1;

void error_report(const std::string &x,int fd);
int connect_init();
void deal_new_connect(int fd,int epfd);
void deal_new_command(int fd);
void deal_get_file(std::string filename,int fd);
void deal_up_file(std::string filename,int fd);
void clean_connect(int fd);
int Recv(int fd,const char *buf,int len,int flags);
int Send(int epoll_fd,int fd,const char *buf,int len,int flags);