#include <stdio.h>
#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() ��
#include <arpa/inet.h> //inet_pton(), inet_ntoa() ��

void print_ip_port(int sockfd);
//server��
int main(){
    int socket_server=socket(AF_INET,SOCK_STREAM,0); //������server��socket

    int opt = 1;
    setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //AF_INET��ʾʹ����IPv4�ĵ�ַ
    //SOCK_STREAM��ʾʹ��TCP���Э�飬���ʹ��UDP����SOCK_DGRAM
    //���һ������Ϊ0���Զ�����ǰ�������������Զ�ѡ����ʵ�Э��
    struct sockaddr_in addr_server;
    //�����Ǹ��ṹ�彫�׽�����IP�Ͷ˿ڽ��а󶨣�����ר��ΪIPv4����ƵĽṹ�壬������ַ�Ĳ���ʹ��
    //���ĳ�Ա��������
    // struct sockaddr_in{
    //     sa_family_t     sin_family;   //��ַ�壨Address Family����Ҳ���ǵ�ַ����
    //     uint16_t        sin_port;     //16λ�Ķ˿ں�
    //     struct in_addr  sin_addr;     //32λIP��ַ
    //     char            sin_zero[8];  //��ʹ�ã�һ����0���
    // };
    addr_server.sin_family=AF_INET; //ʹ��IPv4��ַ
    addr_server.sin_addr.s_addr=inet_addr("127.0.0.1"); //IP��ַ�����������ַ�Ƚ����ⱻ��Ϊ�ػ���ַ(���ص�ַ)����������������Ա��س���
    addr_server.sin_port=htons(1234); //���˶˿�1234������б�ĳ���ʹ������˿ڣ�bind�ͻ�ʧ�ܣ����Ի�һ��
    //����һ�㶼��С���򣬶�����ͨ��ʹ�õ��Ǵ����������Ҫʹ��htons��ת��һ��
    bind(socket_server,(struct sockaddr*)&addr_server,sizeof(addr_server));
    printf("bind success\n");
    //bind��IP�Ͷ˿ڰ���������socket������
    if(listen(socket_server,5)<0){
        std::cout<<"listen fail"<<std::endl;
    }
    //��ʼ���������������5�ǵȴ����еĳ���
    struct sockaddr_in addr_client;
    //׼����ʼ����ͻ��˵�����
    socklen_t client_len=sizeof(addr_client);
    int socket_client=accept(socket_server,(struct sockaddr*)&addr_client,&client_len);
    sleep(1);
    printf("always waiting\n");
    //������accept�������˿ͻ��˵�����(���δ�յ���������һֱ������ȥ)
    //��addr_client���洢�˿ͻ��˵�IP�Ͷ˿ڵ���Ϣ
    //�����client_len���õ��˿ͻ��˵�ʵ�ʴ�С

    //���¿�ʼ��ͻ��˷�����Ϣ
    char message[]="hello client,I'm server";
    for(;;){
        printf("enter cycle\n");
        char buf[100]={};
        printf("waiting client message\n");
        int len=recv(socket_client,buf,sizeof(buf),0);
        printf("recv success\n");
        if(len<=0) {
        printf("error\n");
           break;
        }
        if(strncmp(buf,"quit",4)==0) break;
        buf[len]='\0';
        printf("client: %s\n",buf);
        fflush(stdout);
        fgets(buf,sizeof(buf),stdin);
        buf[strcspn(buf, "\n")] = '\0';  // ȥ�����з�
        //write(socket_client,message,sizeof(message)); //��������send�������һ��flag������û����ô���
        if(send(socket_client,buf,strlen(buf),0)<0){
            std::cout<<"send fail"<<std::endl;
        }
        //�ر���ʹ�õ��׽���
    }
    print_ip_port(socket_server);
    if(close(socket_server)<0){
        std::cout<<"close socket_server fail"<<std::endl;
    }
    if(close(socket_client)<0){
        std::cout<<"close socket_client fail"<<std::endl;
    }
    return 0;
}

void print_ip_port(int sockfd){
    //��������ܳɹ����е�ǰ�����Ѿ�����������
    
    struct sockaddr_in local_addr,client_addr;
    socklen_t socklen=sizeof(sockaddr_in);
    getsockname(sockfd,(struct sockaddr*)&local_addr,&socklen);
    //ͨ�������Ǹ������ɻ�ȡ����socket�Ķ˿ںź�ip�ţ��õ��Ľ�������˵ڶ����������Ǹ��ṹ������
    getpeername(sockfd,(struct sockaddr*)&client_addr,&socklen); //����ǻ�ȡԶ�˵�ip�Ͷ˿ڵ�
    std::cout<<"local  ip is:"<<inet_ntoa(local_addr.sin_addr)<<std::endl;
    //inet_ntoa��ipת��Ϊ�˿��Ķ����ַ�����������Ϊ����htons�ǻ�������ת����
    std::cout<<"local  port is:"<<ntohs(local_addr.sin_port)<<std::endl;

    std::cout<<"client ip is:"<<inet_ntoa(client_addr.sin_addr)<<std::endl;
    std::cout<<"client port is:"<<ntohs(client_addr.sin_port)<<std::endl;
}
//79-86

// void errreport(std::string &x);
// int main(){
//     int server_fd;
//     server_fd=socket(AF_INET,SOCK_DGRAM,0);   
//     if(server_fd<0){
//     errreport("server_fd");
//     }
//     struct sockaddr_in seraddr;
//     seraddr.sin_port=htons(2537);
//     seraddr.sin_family=AF_INET;
//     seraddr.sin_addr.s_addr=INADDR_ANY; //Ĭ���Ǳ�����ip
//     int bind_fd=bind(server_fd,(struct sockaddr*)&seraddr,sizeof(seraddr));
//     if(bind_fd<0){
//         errreport("bind_fd");
//     }
//     int listen_fd=listen(server_fd,5);
//     if(listen_fd<0){
//         errreport("listen_fd");
//     }
    
//     int epoll_fd=epoll_create1(0);
//     if(epoll_fd<0){
//         errreport("epoll_fd");
//     }
//     struct epoll_event event;
//     event.events=EPOLLIN;
//     event.data.fd=server_fd;

//     return 0;
// }
// void errreport(std::string x){
//     std::cout<<x<<" start fail"<<std::endl;
//     exit(1);
// }