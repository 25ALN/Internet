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
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

const int first_port=2100;
const int mes_travel_port=5100;
const int maxevents=100;
const int filebuf_size=4096;

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