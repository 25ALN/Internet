#include "cli.hpp"


int main(){
    client_fd=connect_init();
    if(client_fd<0){
        error_report("socket",client_fd);
    }
    start_PASV_mode(client_fd,"PASV\r\n");
    char repose[1024];
    memset(repose,'\0',sizeof(repose));
    int n=Recv(client_fd,repose,sizeof(repose),0);
    if(n<0){
        clean_connect(client_fd);
        return 0;
    }
    std::cout<<"server PASV reponse:"<<repose;
    std::string cmd(repose,n);
    std::cout<<"cmd="<<cmd<<std::endl;
    if(cmd.find("227 Entering Passive Mode")!=std::string::npos){
        get_ip_port(cmd);
    }else{
        error_report("PASV mode",client_fd);
    }
    while(true){
        char message[1024];
        memset(message,'\0',sizeof(message));
        std::cin.getline(message,sizeof(message));
        deal_willsend_message(client_fd,message);
    }
    
    clean_connect(client_fd);
    return 0;
}
void start_PASV_mode(int fd,std::string first_m){
    if(!first_m.empty())
    Send(fd,first_m.c_str(),first_m.size(),0);
}
void deal_willsend_message(int fd,char m[1024]){
    std::string mes=(std::string)m;
    if(mes.find("PASV")!=std::string::npos){
        std::thread x(deal_send_message,fd,mes);
        x.join();
    }else if(mes.find("STOR")!=std::string::npos){
        std::thread x(deal_up_file,mes.substr(5,mes.size()-5),fd);
        x.join();
    }else if(mes.find("RETR")!=std::string::npos){
        std::thread x(deal_get_file,mes.substr(5,mes.size()-5),fd);
        x.join();
    }
}
void deal_send_message(int fd,std::string m){
    if(!m.empty())
    Send(fd,m.c_str(),m.size(),0);
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
    client_mess.sin_addr.s_addr=htonl(INADDR_ANY);
    int flags=fcntl(fd,F_GETFL,0);
    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){
        error_report("fcntl",fd);
    }
    int connect_fd=connect(fd,(struct sockaddr*)&client_mess,sizeof(client_mess));
    if(connect_fd<0&&errno!=EINPROGRESS){
        error_report("connect",fd);
    }
    
    return fd;
}

void get_ip_port(std::string ser_mesage){
    std::string ip;
    int cnt=0;
    int start=ser_mesage.find("(");
    int end=ser_mesage.find(")");
    std::string mes=ser_mesage.substr(start+1,end-start-1);
    int douhao=mes.find(",");
    while(cnt!=4){
        for(int i=0;i<douhao;i++){
            ip+=mes[i];
        }
        if(cnt!=3){
        ip+='.';
        mes.erase(0,douhao+1);
        douhao=mes.find(",");
        cnt++;
        }
    std::string p1,p2;
    for(auto j:mes){
        if(j!=','&&cnt!=4){
            p1+=j;
            }else if(j!=')'&&j!=','){
            p2+=j;
        }else if(j==','){
            cnt++;
        }
    }
    IP=ip;
    data_port=std::stoi(p2)*256+std::stoi(p1);
    std::cout<<"IP="<<IP<<" port="<<data_port<<std::endl;
    }
}

void deal_get_file(std::string filename,int fd){
    int data_fd=socket(AF_INET,SOCK_STREAM,0);
    int flag=fcntl(data_fd,F_GETFL,0);
    fcntl(data_fd,F_SETFL,flag|O_NONBLOCK);
    if(data_fd<0){
        error_report("socket",data_fd);
    }
    struct sockaddr_in filedata;
    filedata.sin_family=AF_INET;
    filedata.sin_port=htons(data_port);
    if(IP.empty()){
        return;
    }
    if(inet_pton(AF_INET,IP.c_str(),&filedata.sin_addr)<0){
        std::cout<<"inet_pton fail"<<std::endl;
        return;
    }
    int m=connect(data_fd,(struct sockaddr*)&filedata,sizeof(filedata));
    if(m<0&&errno!=EINPROGRESS){
        std::cout<<"connect fail"<<std::endl;
        return;
    }

    // 使用select等待连接完成
    fd_set set;
    FD_ZERO(&set);
    FD_SET(data_fd, &set);
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if(select(data_fd+1, NULL, &set, NULL, &timeout) <= 0){
        std::cout<<"Connection timeout"<<std::endl;
        close(data_fd);
        return;
    }

    if(filename.empty()){
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
    int data_fd=socket(AF_INET,SOCK_STREAM,0);
    if(data_fd<0){
        error_report("socket",data_fd);
    }
    int flag=fcntl(data_fd,F_GETFL,0);
    fcntl(data_fd,F_SETFL,flag|O_NONBLOCK);
    struct sockaddr_in data_message;
    data_message.sin_family=AF_INET;
    data_message.sin_port=htons(data_port);
    if(IP.empty()){
        return;
    }
    if(inet_pton(AF_INET,IP.c_str(),&data_message.sin_addr)<0){
        std::cout<<"inet_pton fail"<<std::endl;
        return;
    }
    int connnect_fd=connect(fd,(struct sockaddr*)&data_message,sizeof(data_message));
    if(connnect_fd<0&&errno!=EINPROGRESS){
        std::cout<<"connect fail"<<std::endl;
        return;
    }
    if(filename.empty()){
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
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return reallen;
            }
            error_report("recv",fd);
            return -1;
        }else if(temp==0){
            break;
        }
        reallen+=temp;
    }
    return reallen;
}

int Send(int fd,const char *buf,int len,int flags){
    int reallen=0;
    while(reallen<len){
        int temp=send(fd,buf+reallen,len-reallen,flags);
        if(temp<0){
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
