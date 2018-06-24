#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define WORKERS_NUM 2

typedef struct send_package{
    char locate[200];
    int start;
    int end;
}send_package;

int main(int argc, char *argv[]){
    if(argc<=1){
        printf("please input file\n");
        return 0;
    }


    //Create socket
    int sock[2];    

    for(int i=0; i<WORKERS_NUM ; ++i){
        sock[i] = socket(AF_INET,SOCK_STREAM,0);
        if(sock[i] == -1){
            printf("could not create socket %d\n",i);
        }
        printf("socket %d created %d\n",i,sock[i]);
    }
    
    
    //get workers ip addr
    char workers_ip[WORKERS_NUM][20];
    struct sockaddr_in workers[2];    
    FILE* conf = fopen("./workers.conf","r");
 
    for(int i=0;i<WORKERS_NUM;++i){
        fscanf(conf,"%s",workers_ip[i]);
        printf("%s",workers_ip[i]);

        workers[i].sin_addr.s_addr = inet_addr(workers_ip[i]);
        workers[i].sin_family = AF_INET;
        workers[i].sin_port = htons(8888);

        //connect to worker
        if(connect(sock[i],(struct sockaddr*)&workers[i],sizeof(workers[i]))<0){
            printf("connect failed. Error\n");
            return 1;
        }
        printf("Connected\n");
    }


    //count lines
    FILE* txt=fopen(argv[1],"r");
    int line=0;
    char buf[2000];
    for(line=0 ; fgets(buf,sizeof(buf),txt)!=NULL ; ++line,memset(buf,0,sizeof(buf)) )
        ;


    //send and receive
    int stat[26]={0};
    int worker_stat[2][26]={0};
    int offset = line/2;
    int to=0;
    int send_len=0,recv_len=0;
    send_package s_p;

    memset(s_p.locate,0,sizeof(s_p.locate));
    strcat(s_p.locate,"./");
    strcat(s_p.locate,argv[1]);

    for(to=0, s_p.start=0 ; to<WORKERS_NUM ; ++to, s_p.start+=offset){
        s_p.end = s_p.start + offset;
        //send
        if( (send_len=send(sock[to],(char*)&s_p,sizeof(s_p),0)) <0 ){
            printf("send failed\n");
            return -1;
        }
        //receive
        if( (recv_len=recv(sock[to],(char*)worker_stat[to],sizeof(worker_stat[to]),0)) <0 ){
            printf("recv failed\n");
            return -1;
        }
        //stat
        for(int i=0;i<26;++i){
            stat[i] += worker_stat[to][i];
        }
    }
    //stat result
    for(int i=0;i<26;++i){
        printf("%c:%d\n",'a'+i,stat[i]);
    }
    close(sock[0]);
    close(sock[1]);
    return 0;
}