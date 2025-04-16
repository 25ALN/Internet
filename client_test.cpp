#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //sockaddr_in, htons() 等
#include <arpa/inet.h> //inet_pton(), inet_ntoa() 等

int main(){
    int sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    client_addr.sin_family=AF_INET;
    client_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    client_addr.sin_port=htons(1234);
    connect(sock,(struct sockaddr*)&client_addr,sizeof(client_addr));
    char message[100];
    // read(sock,message,sizeof(message)-1); //与write是一对的，相比于recv没有那么灵活
    recv(sock,message,sizeof(message),0); //和send是一对的
    printf("message have receved!\n");
    printf("%s",message);
    close(sock);
    return 0;
}