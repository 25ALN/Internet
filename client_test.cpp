#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

int main(){
    int mark=0;
    int sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    client_addr.sin_family=AF_INET;
    client_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    // int result=client_addr.sin_addr.s_addr=inet_pton(AF_INET,"127.0.0.1",&client_addr.sin_addr);
    // if(result<=0){
    //     perror("inet_pton failed");
    //     exit(1);
    // }
    client_addr.sin_port=htons(1234);
    if(connect(sock,(struct sockaddr*)&client_addr,sizeof(client_addr))<0){
        std::cout<<"connect fail";
        exit(1);
    }
    char message[100]="hello server\n";
    if(send(sock,message,strlen(message),0)<0){
        printf("send fail\n");
    }
    while(1){
        if(mark==0){
        printf("enter cycle\n");
        }
        mark=1;
        char buf[100]={};
        fgets(buf,sizeof(buf),stdin);
        buf[strcspn(buf, "\n")] = '\0';  // 去掉换行符
        if(strncmp(buf,"quit",4)==0) break;
        if(send(sock,buf,strlen(buf),0)<0){
            std::cout<<"send fail";
            exit(1);
        }
        // read(sock,message,sizeof(message)-1); //与write是一对的，相比于recv没有那么灵活
        int len=recv(sock,buf,sizeof(buf),0); //和send是一对的
        buf[len]='\0';
        if(len<=0){
        printf("chat end!\n");
        break;
        }else{
        printf("server: %s\n",buf);
        fflush(stdout);
        }
    }
    if(close(sock)<0){
        std::cout<<"close fail"<<std::endl;
    }
    return 0;
}