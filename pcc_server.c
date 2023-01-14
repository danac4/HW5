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
int sigint, connfd = -1, listenfd = -1;

void finish(){
    int i;
    for(i = 0; i < 95; i++){
        printf("char '%c' : %u times\n",i+32, pcc_total[i]);
    }
    close(listenfd);
    exit(0);
}
void signal_handler(){
    if(connfd < 0){
        finish();
    }
    sigint = 0;
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
    uint32_t integer, cnt = 0;
    for(i = 0; i < size; i++){
        integer = (uint32_t)buffer[i];
        if(31 < integer && integer < 127){ /* is printable character */
            connection_total[(uint32_t)(integer-32)] += 1;
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
    int val = 1, buff_size;
    uint16_t port;
    uint32_t N, C, left, leftBuff;
    uint32_t connection_total[95]={0};
    char *integer_buff, *buffer;
    int bytes_written = 0, bytes_read=0, curr_written = 0, curr_read = 0;
    listenfd = -1;

    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);

    sigint = 1; /* SIGINT flag */
    struct sigaction new_action = {
            .sa_handler = signal_handler,
            .sa_flags = SA_RESTART
    };
    if(sigaction(SIGINT,&new_action,NULL) == -1){
        perror("Signal handle registration failed");
        exit(1);
    }
    if(argc != 2){
        perror("Invalid integerber of arguments passed to pcc_server!");
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

    port = (uint16_t)atoi(argv[1]);
    if(port < 1024){
        perror("Invalid port!");
        exit(1);
    }

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

    while(sigint){
        // Accept a connection.
        connfd = accept(listenfd, NULL, NULL);
        if(connfd < 0)
        {
            perror("accept failed");
            exit(1);
        }

        /* get N from client */
        left = sizeof(unsigned int);
        integer_buff = (char*)&N;
        bytes_read = 0;
        while(left > 0){
            curr_read = read(connfd, integer_buff, left);
            bytes_read += curr_read;
            if(curr_read == 0 && bytes_read != sizeof(unsigned int)){
                perror("Client process was killed unexpectedly");
                close(connfd);
                connfd = -1;
                break;
            }
            if(curr_read < 0){
                if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ /*TCP errors given in instructions */
                    perror("TCP error occurred while reading N");
                    close(connfd);
                    connfd = -1;
                    break;
                }
                else{
                    perror("Failed to read N from client");
                    close(connfd);
                    connfd = -1;
                    exit(1);
                }
            }
            integer_buff += curr_read;
            left -= curr_read;
        }

        if(connfd == -1){ /* connection failed in nested while */
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
            curr_read = read(connfd,buffer,buff_size);
            if(curr_read == 0){
                perror("Client process was killed unexpectedly");
                close(connfd);
                connfd = -1;
                break;
            }
            if(curr_read < 0){
                if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ /*TCP errors given in instructions */
                    perror("TCP error occurred while reading N");
                    close(connfd);
                    connfd = -1;
                    break;
                }
                else{
                    perror("Failed to read file content sent by client");
                    close(connfd);
                    connfd = -1;
                    exit(1);
                }
            }

            if(connfd == -1){ /* connection failed in nested while */
                break;
            }

            /* count printable characters in received data */
            C += cnt_connection_total(buffer, curr_read, connection_total);
            free(buffer);
            left -= curr_read;
        }

        if(connfd == -1){ /* connection failed in nested while */
            reset_array(connection_total);
            continue;
        }

        /* sending integerber of printable characters (C) to client */
        C = htonl(C);
        integer_buff = (char*)&C;
        left = sizeof(unsigned int);
        bytes_written = 0;
        while(left > 0){
            curr_written = write(connfd, integer_buff, left);
            bytes_written += curr_written;
            if(curr_written == 0 && bytes_written != sizeof(unsigned int)){
                perror("Client process was killed unexpectedly");
                close(connfd);
                connfd = -1;
                break;
            }
            if(curr_written < 0){
                if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){ /*TCP errors given in instructions */
                    perror("TCP error occurred while writing C");
                    close(connfd);
                    connfd = -1;
                    break;
                }
                else{
                    perror("Failed to write C to client");
                    close(connfd);
                    connfd = -1;
                    exit(1);
                }
            }
            integer_buff += curr_written;
            left -= curr_written;
        }

        if(connfd == -1){ /* connection failed in nested while */
            reset_array(connection_total);
            continue;
        }

        /* update pcc_total with calculated connections printable characters counts*/
        pcc_total_update(connection_total);
        reset_array(connection_total);
        /* close socket */
        close(connfd);
        connfd = -1;
    }
    finish();
}