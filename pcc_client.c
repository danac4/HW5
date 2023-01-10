#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUFF_SIZE 1000000 /* buffer max size is 1MB */

/* some of the code is from recreation 10 */


/*
 * help from:
 * https://www.geeksforgeeks.org/c-program-find-size-file/
 * https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
 */
int main(int argc, char *argv[]){
    FILE *fp;
    uint32_t N, C, buff_size, num;/*N is given file size in bytes, C is the number of printable characters received from the server */
    char *buffer,*num_buff;
    int bytes_written = 0, bytes_read=0, curr_written = 0, curr_read = 0, sockfd = -1, left;
    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );
    if(argc != 4){
        perror("Invalid number of arguments passed to pcc_client!");
        exit(1);
    }
    fp = fopen(argv[3],"rb");
    if(!fp){
        perror("Failed to open given file");
        exit(1);
    }

    /*get size of given file in bytes*/
    fseek(fp, 0L, SEEK_END);
    N = (uint32_t)ftell(fp);
    rewind(fp);

    /* allocate buffer memory */
    buff_size = (N < BUFF_SIZE) ? N : BUFF_SIZE;
    buffer = malloc(buff_size);
    if(!buffer){
        perror("Failed to allocate memory for buffer");
        exit(1);
    }
    memset(buffer, 0, buff_size);

    /*create socket*/
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket");
        exit(1);
    }

    /*set server params*/
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if((inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)) != 1)
    {
        perror("Failed to convert ip address with inet_pton");
        exit(1);
    }

    /* connect to server */
    if(connect(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Failed to connect to server");
        exit(1);
    }

    /* send N to server */
    num = htonl(N);
    num_buff = (char*)&num;
    left = sizeof(num);
    while(left > 0){
        curr_written = write(sockfd, num_buff, left);
        if(curr_written <= 0){//<= or <
            perror("Write failed while trying to send N to server");
            exit(1);
        }
        num_buff += curr_written;
        left -= curr_written;
    }

    /* send files contents to server */
    bytes_written = 0;
    while(bytes_read < N){
        curr_read = fread(buffer, 1,buff_size,fp);
        if(ferror(fp) != 0){ //?
            perror("Failed to read file's content");
            exit(1);
        }
        while(bytes_written < curr_read){
            curr_written = write(sockfd, buffer+bytes_written, curr_read - bytes_written);
            if(curr_written <= 0){ //<= or <
                perror("Failed to write file's content to server");
                exit(1);
            }
            bytes_written += curr_written;
        }
        bytes_written = 0;
        bytes_read += curr_read;
        memset(buffer, 0, buff_size);
    }
    free(buffer);

    /* get the number of printable characters received from the server */
    bytes_read = 0;
    num_buff = (char *)&C;
    left = sizeof(C);
    while(left > 0){
        curr_read = read(sockfd, num_buff, left);
        if(curr_read <= 0){
            perror("Failed to read the number of printable characters from server");
            exit(1);
        }
        num_buff += curr_read;
        left -= curr_read;
    }
    C = htonl(C); //?
    printf("# of printable characters: %u\n", C);
    fclose(fp);
    close(sockfd);
    exit(0);
}