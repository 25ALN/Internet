#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等
#include <time.h> //获取时间

int main(){
    int connectfd;
    time_t ticks;
    char buf[100];
    struct sockaddr_in savetime;
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    savetime.sin_family=AF_INET;
    savetime.sin_port=htons(13);
    savetime.sin_addr.s_addr=htonl(INADDR_ANY); //INADDR_ANY表示是任意本地IP地址

    bind(sockfd,(struct sockaddr*)&savetime,sizeof(savetime));
    listen(sockfd,5);

    while(1){
        connectfd=accept(sockfd,(struct sockaddr*)NULL,NULL);
        ticks=time(NULL);
        snprintf(buf,100,"%24s\n",ctime(&ticks));
        write(connectfd,buf,sizeof(buf));
        close(connectfd);
    }

    return 0;
}