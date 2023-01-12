#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#define BUFF_SIZE 1000000 /* buffer max size is 1MB */
/* some of the code is from recreation 10 */

uint32_t pcc_total[95] = {0}; /* 126-32+1 = 95 printable characters*/
int flag;

void signal_handler(){
    int i;
    if(flag == 0){
        for(i = 0; i < 95; i++){
            printf("char '%c' : %u times\n",i+32, pcc_total[i]);
        }
        exit(0);
    }
    flag = 1;
}
/*
 * sets given array elements to zero
 */
void reset_array(uint32_t arr[]){
    int i;
    for(i = 0; i < 95; i++){
        arr[i] = 0;
    }
}

uint32_t cnt_connection_total(char* buffer, int size, uint32_t connection_total[]){
    int i;
    uint32_t cnt = 0;
    for(i = 0; i < size; i++){
        if(31 < buffer[i] && buffer[i] < 127){ /* is printable character */
            connection_total[(uint32_t)(buffer[i]-32)] += 1;
            cnt += 1;
        }
    }
    return cnt;
}

void pcc_total_update(uint32_t connection_total[]){
    int i;
    for(i = 0; i < 95; i++){
        pcc_total[i] += connection_total[i];
    }
}
/*
 * help from:
 * https://stackoverflow.com/questions/21515946/what-is-sol-socket-used-for
 */
int main(int argc, char *argv[])
{
    int listenfd = -1, connfd= -1,val = 1, /*left,leftBuff,*/ buff_size,i;
    uint16_t port;
    uint32_t N, C, left, leftBuff;
    uint32_t connection_total[95]={0};
    char *num_buff, *buffer;
    int bytes_written = 0, bytes_read=0, curr_written = 0, curr_read = 0, bytes_total=0;

    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );

    flag = 0;
    struct sigaction new_action = {//check!!
            .sa_handler = signal_handler,
            .sa_flags = SA_RESTART
    };
    if(sigaction(SIGINT,&new_action,NULL) == -1){
        perror("Signal handle registration failed");
        exit(1);
    }
    if(argc != 2){
        perror("Invalid number of arguments passed to pcc_server!");
        exit(1);
    }

    /*create socket*/
    if((listenfd  = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket");
        exit(1);
    }

    if((setsockopt(listenfd, SOL_SOCKET,SO_REUSEADDR,&val, sizeof(val))) < 0){
        perror("setsockopt failed");
        exit(1);
    }

    port = atoi(argv[1]);
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if(0 != bind(listenfd, (struct sockaddr*)&serv_addr, addrsize)){
        perror("Failed to bind");
        exit(1);
    }

    if(0 != listen(listenfd, 10)){
        perror("listen failed");
        exit(1);
    }

    while(flag == 0){
        // Accept a connection.
        connfd = accept(listenfd, NULL, &addrsize);
        if(connfd < 0)
        {
            perror("accept failed");
            continue; //? need to go to next iteration
        }

        /* get N from client */
        left = sizeof(unsigned int);
        num_buff = (char*)&N;
        bytes_read = 0;
        while(left > 0){
            curr_read = read(connfd, num_buff, left);
            bytes_read += curr_read;
            if(curr_read == 0 && bytes_read != sizeof(unsigned int)){
                perror("Client process was killed unexpectedly");
                close(connfd);
                connfd = -1;
                continue;//or break?
            }
            if(curr_read < 0){
                if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ /*TCP errors given in instructions */
                    perror("TCP error occurred while reading N");
                    close(connfd);
                    connfd = -1;
                    continue;
                }
                else{
                    perror("Failed to read N from client");
                    close(connfd);
                    connfd = -1;
                    exit(1);
                }
            }
            num_buff += curr_read;
            left -= curr_read;
        }

        if(connfd == -1){ /* connection was closed in nested while */
            continue;
        }
        N = ntohl(N);

        /* calculate printable character count (C) */
        left = N;
        C = 0;
        while(left > 0){
            buff_size = (left < BUFF_SIZE) ? left : BUFF_SIZE;
            buffer = malloc(buff_size);
            if(!buffer){//? what to do?
                perror("Failed to allocate memory for buffer");
                exit(1);
            }
            memset(buffer, 0, buff_size);
            leftBuff = buff_size;
            bytes_read = 0;
            while(leftBuff > 0){
                curr_read = read(connfd,buffer+bytes_read,leftBuff);
                bytes_read += curr_read;
                leftBuff -= curr_read;
                if(curr_read == 0){
                    perror("Client process was killed unexpectedly");
                    close(connfd);
                    connfd = -1;
                    break; //?
                }
                if(curr_read < 0){
                    if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ /*TCP errors given in instructions */
                        perror("TCP error occurred while reading N");
                        close(connfd);
                        connfd = -1;
                        break;//?
                    }
                    else{
                        perror("Failed to read file content sent by client");
                        close(connfd);
                        connfd = -1;
                        exit(1);
                    }
                }
            }

            /* count printable characters in received data */
            reset_array(connection_total);
            C += cnt_connection_total(buffer, bytes_read, connection_total);//bytes read or buff size?
            free(buffer);
            left -= bytes_read;//or buffsize?
        }

        /* sending number of printable characters (C) to client */
        C = htonl(C);
        num_buff = (char*)&C;
        left = sizeof(unsigned int);
        bytes_written = 0;
        while(left > 0){
            curr_written = write(connfd, num_buff, left);
            bytes_written += curr_written;
            if(curr_written == 0 && bytes_written != sizeof(unsigned int)){
                perror("Client process was killed unexpectedly");
                close(connfd);
                connfd = -1;
                continue;//or break?
            }
            if(curr_written < 0){
                if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ /*TCP errors given in instructions */
                    perror("TCP error occurred while writing C");
                    close(connfd);
                    connfd = -1;
                    continue;
                }
                else{
                    perror("Failed to write C to client");
                    close(connfd);
                    connfd = -1;
                    exit(1);
                }
            }
            num_buff += curr_written;
            left -= curr_written;
        }
        /* update pcc_total with calculated connections printable characters counts*/
        pcc_total_update(connection_total);
        /* close socket */
        close(connfd);
    }
    for(i = 0; i < 95; i++){
        printf("char '%c' : %u times\n",i+32, pcc_total[i]);
    }
    close(listenfd);
    exit(0);
}
