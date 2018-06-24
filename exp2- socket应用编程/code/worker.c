/* server application */
 
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct send_package{
    char locate[200];
    int start;
    int end;
}send_package;


void low(char* message){
    for(int i=0; i<strlen(message) ; ++i){
        message[i] = (message[i]>='A' && message[i]<='Z')? 'a'+message[i]-'A' : message[i];
    }
    return;
}

void stat_char(char* message,int stat[]){
    int key;
    low(message);
    for(int i=0; i<strlen(message) ;++i){
        key = (message[i]>='a' && message[i]<='z')? message[i]-'a' : -1;
        if(key != -1)
            ++stat[key];
    }
    return;
}

int main(int argc, const char *argv[])
{
    int s, cs;
    struct sockaddr_in server, client;

    // Create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket\n");
		return -1;
    }
    printf("Socket created\n");
     

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);
     

    // Bind
    if (bind(s,(struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error\n");
        return -1;
    }
    printf("bind done\n");
     

    // Listen
    listen(s, 3);
     

    // Accept and incoming connection
    printf("Waiting for incoming connections...\n");
     

    // accept connection from an incoming client
    int c = sizeof(struct sockaddr_in);
    if ((cs = accept(s, (struct sockaddr *)&client, (socklen_t *)&c)) < 0) {
        perror("accept failed\n");
        return 1;
    }
    printf("Connection accepted\n");


    // Receive a message from client
	int msg_len = 0;
    send_package sp;

    while ((msg_len = recv(cs, (char*)&sp, sizeof(sp), 0)) > 0) {
    	char* locate = sp.locate;
        FILE* f=fopen(locate,"r");
        int stat[26]={0};
        char buf[2000]={0};
    	int i;
        
        for(i=0 ; i<sp.start ; ++i){
            fgets(buf,sizeof(buf),f);
        }

        for(int i=sp.start ; i<sp.end ; ++i){
            fgets(buf,sizeof(buf),f);
            stat_char(buf,stat);
        }
        
        write(cs, (char*)stat, sizeof(stat));
        memset(stat,0,sizeof(stat));
    }

     
    if (msg_len == 0) {
        printf("Client disconnected\n");
    }
    else { // msg_len < 0
        perror("recv failed\n");
		return -1;
    }
     
    return 0;
}
