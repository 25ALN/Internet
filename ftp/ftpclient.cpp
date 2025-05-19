#include "cli.hpp"


int main(){
    client_fd=connect_init();
    if(client_fd<0){
        error_report("connect ",client_fd);
    }
    int epoll_fd=epoll_create(0);
    if(epoll_fd<0){
        std::cout<<"epoll create fail"<<std::endl;
        clean_connect(client_fd);
        return 0;
    }
    struct epoll_event ev,events[maxt];
    ev.data.fd=client_fd;
    ev.events=EPOLLET|EPOLLIN;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&ev)<0){
        std::cout<<"epoll_ctl fail"<<std::endl;
        clean_connect(client_fd);
        return 0;
    }

    while(true){
        int ready=epoll_wait(epoll_fd,events,maxt,-1);
        if(ready<0&&errno==EINTR){ 
            if(errno==EINTR){     //信号被中断了，连接重试
            continue;
            }
            break;
        }    
        for(int i=0;i<ready;i++){
            if(events[i].events&&(EPOLLERR||EPOLLHUP)){
                clean_connect(events[i].data.fd);
            }else if(events[i].events&&EPOLLIN){
                if(events[i].data.fd==client_fd){
                    deal_new_connect(events[i].data.fd,epoll_fd);
                }else{
                    deal_new_command(events[i].data.fd);
                }
            }else if(events[i].events&&EPOLLOUT){ //处理剩余可写的数据
                if(ser_msave.find(client_fd)!=ser_msave.end()){ //检查是否还存在可写数据存在
                    ser_mes &remain=ser_msave[client_fd];
                    int sent=Send(epoll_fd,client_fd,remain.buf.data()+remain.real_len,remain.buf.size()-remain.real_len,0);
                    if(sent>0){
                        remain.real_len+=sent;
                        if(remain.real_len>=remain.buf.size()){ //数据已确定发送完毕，可以将添加的EPOLLOUT
                            struct epoll_event ev;
                            ev.data.fd=client_fd;
                            ev.events=EPOLLET|EPOLLIN;
                            epoll_ctl(epoll_fd,EPOLL_CTL_MOD,client_fd,&ev);
                            ser_msave.erase(client_fd);
                        }
                    }else if(sent<0&&errno!=EAGAIN){ //传输异常
                        clean_connect(client_fd);
                    }
                }
            }
        }
    }
    
    clean_connect(client_fd);
    return 0;
}

void error_report(const std::string &x,int fd){
    std::cout<<x<<" start fail"<<std::endl;
    close(fd);
    exit(1);
}

int connect_init(){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){
        error_report("socket",fd);
    }
    struct sockaddr_in client_mess;
    client_mess.sin_family=AF_INET;
    client_mess.sin_port=htons(first_port);
    client_mess.sin_addr.s_addr=inet_addr(INADDR_ANY);
    int flags=fcntl(fd,F_GETFL,0);
    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){
        error_report("fcntl",fd);
    }
    int connect_fd=connect(fd,(struct sockaddr*)&client_mess,sizeof(client_mess));
    if(connect_fd<0){
        error_report("connect",fd);
    }
    return fd;
}

void deal_new_connect(int fd,int epfd){
    int error=0;
    socklen_t len=sizeof(error);
    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&len)<0||error!=0){ //检查连接是否成功建立
        error_report("getsockopt",fd);
    }
    std::cout<<"success connect server!"<<std::endl;
    std::string first_message="hello server!";
    Send(epfd,fd,first_message.c_str(),first_message.size(),0);
}

void deal_new_command(int fd){
    char buf[4096];
    memset(buf,'\0',sizeof(buf));
    int n=Recv(fd,buf,sizeof(buf),0);
    if(n<=0){
        clean_connect(fd);
        return;
    }
    std::string ser_mesage(buf,n);
    std::cout<<"server reponse: "<<ser_mesage<<std::endl;
    if(ser_mesage.find("227 entering passive mode")!=std::string::npos){ //解析ip地址与端口号
        std::string ip;
        int cnt=0;
        ser_mesage.erase(0,27);
        for(auto i:ser_mesage){
            if(i!=','){
                ip+=i;
                if(cnt==3){
                    break;
                }
            }else{
                ip+='.';
                cnt++;
            }
        }
        std::string p1,p2;
        ser_mesage.erase(0,ip.size()+1);
        for(auto j:ser_mesage){
            if(j!=','&&cnt!=4){
                p1+=j;
            }else if(j!=')'&&j!=','){
                p2+=j;
            }else if(j==','){
                cnt++;
            }
        }
        IP=(char *)(ip.c_str());
        data_port=std::stoi(p1)*256+std::stoi(p2);
    }
}

void deal_get_file(std::string filename,int fd){
    int data_fd=socket(AF_INET,SOCK_STREAM,0);
    if(data_fd<0){
        error_report("socket",data_fd);
    }
    struct sockaddr_in filedata;
    filedata.sin_family=AF_INET;
    filedata.sin_port=htons(data_port);
    filedata.sin_addr.s_addr=inet_addr(IP);
    int m=connect(fd,(struct sockaddr*)&filedata,sizeof(filedata));
    if(m<0){
        std::cout<<"connect fail"<<std::endl;
        return;
    }
    FILE *fp=fopen(filename.c_str(),"wb");
    if(fp==nullptr){
        std::cout<<"file open fail "<<std::endl;
        shutdown(fd,SHUT_RDWR);
        close(fd);
        fclose(fp);
        return;
    }
    char buf[4096];
    memset(buf,'\0',sizeof(buf));
    while(size_t n=recv(fd,buf,sizeof(buf),0)){
        fwrite(buf,1,n,fp);
    }
    fclose(fp);
}

void deal_up_file(std::string filename,int fd){
    int connnect_fd=connect(fd,nullptr,0);
    if(connnect_fd<0){
        std::cout<<"connect fail"<<std::endl;
        return;
    }
    FILE *fp=fopen(filename.c_str(),"rb");
    if(fp==nullptr){
        std::cout<<"file open fail "<<std::endl;
        fclose(fp);
        return;
    }
    char buf[4096];
    memset(buf,'\0',sizeof(buf));
    while(size_t n=fread(buf,sizeof(buf),1,fp)){
        send(fd,buf,sizeof(buf),0);
    }
    shutdown(fd,SHUT_RDWR);
    close(fd);
    fclose(fp);
}

int Recv(int fd,char *buf,int len,int flags){
    int reallen=0;
    while(reallen<len){
        int temp=recv(fd,buf+reallen,len-reallen,flags);
        if(temp<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                break; //此时说明当前无连接
            }
            error_report("recv",fd);
        }else if(temp==0){
            break;
        }
        reallen+=temp;
    }
    return reallen;
}

int Send(int epoll_fd,int fd,const char *buf,int len,int flags){
    int reallen=0;
    while(reallen<len){
        int temp=send(fd,buf+reallen,len-reallen,flags);
        if(temp<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK){ //此时连接中断了，消息传了一半或没有传，需重新使用epoll监听事件
                ser_mes message;
                message.fd=fd;
                message.buf.assign(buf+temp,buf+len); //保存好未发送的数据
                message.real_len=0;
                ser_msave[fd]=message; //用定义好的哈希表保存对应的套接字所对应的信息

                //开始利用epoll重新进行监听
                struct epoll_event ev;
                ev.data.fd=message.fd;
                ev.events=EPOLLET||EPOLLIN||EPOLLOUT;
                if(epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev)<0){
                    error_report("epoll_ctl ",fd);
                }
                return reallen;
            }
            error_report("send",fd);
        }else if(temp==0){
            //数据已全部发送完毕
            return reallen;
        }
        reallen+=temp;
    }
    return reallen;
}

void clean_connect(int fd){
    if(fd!=-1){
        shutdown(fd,SHUT_RDWR);
        close(fd);
        fd=-1;
    }
}