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
#include <algorithm>
#include <sstream>

const int first_port=2100;
int data_port=-1;
std::string IP;
const int maxt=1000;
int client_fd=-1;

void error_report(const std::string &x,int fd);
int connect_init();
void start_PASV_mode(int fd,std::string first_m);
void get_ip_port(std::string ser_mesage);
void deal_willsend_message(int fd,char m[1024]);
void deal_send_message(int fd,std::string m);
void deal_get_file(std::string filename,int fd);
void deal_up_file(std::string filename,int fd);
void send_command(int fd,const std::string &cmd);
void clean_connect(int fd);
int Recv(int fd,char *buf,int len,int flags);
int Send(int fd,const char *buf,int len,int flags);