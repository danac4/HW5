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

//*
// * sets N to file size and transfers file content to buffer.
// * return buffer
// */
//char* process_file(FILE* fp, unsigned int* N){
//    unsigned int buff_size;
//
//}

/*
 * help from:
 * https://www.geeksforgeeks.org/c-program-find-size-file/
 * https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
 */
int main(int argc, char *argv[]){
    FILE *fp;
    uint32_t N, C, buff_size;/*N is given file size in bytes, C is the number of printable characters received from the server */
    u_long n;
    char *buffer,*C_buff;
    int bytes_written = 0, bytes_read=0, curr_written = 0, curr_read = 0, sockfd = -1, left;
    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );
    if(argc != 4){
        //incorrect number of arg
    }
    fp = fopen(argv[3],"rb");
    if(!fp){
        //error- cant open given file
    }

    /*get size of given file in bytes*/
    fseek(fp, 0L, SEEK_END);
    N = (uint32_t)ftell(fp);
    rewind(fp);

    /* allocate buffer memory */
    buff_size = (N < BUFF_SIZE) ? N : BUFF_SIZE;
    buffer = malloc(buff_size);
    if(!buffer){
        //error in allocating memory
    }
    memset(buffer, 0, buff_size);

    /*create socket*/
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        //cant create socket
    }

    /*set server params*/
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if((inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)) != 1)
    {
       //error
    }

    /* connect to server */
    if(connect(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        //cant connect
    }

    /* send N to server */
    n = htonl(N);
    while(bytes_written < N){
        curr_written = write(sockfd, &n, N);
        if(curr_written <= 0){//<= or <
            //error
        }
        bytes_written += curr_written;
    }

    /* send files contents to server */
    bytes_written = 0;
    while(bytes_read < N){
        curr_read = fread(buffer, 1,buff_size,fp);
        if(ferror(fp) != 0){ //?
            //error in reading
        }
        while(bytes_written < curr_read){
            curr_written = write(sockfd, buffer+bytes_written, curr_read - bytes_written);
            if(curr_written <= 0){ //<= or <
                //error
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
    C_buff = (char *)&C;
    left = sizeof(C);
    while(left > 0){
        curr_read = read(sockfd, C_buff, left);
        if(curr_read <= 0){
            //error
        }
        C_buff += curr_read;
        left -= curr_read;
    }
    C = htonl(C);
    printf("# of printable characters: %u\n", C);
    //open file for reading and to see that it can be open and detect errors in read
    //create TCP connection to port argv[2] on the specified ip argv[1]
    //transfer the contents of the file to the server over the tcp connection and receive count of printable characters from the server:
    //send to the server the number of bytes that will be transferred (size of file)
    //send file contents to server
    //save server answer
    //print to stdout "# of printable characters: %u\n" that got from server
    fclose(fp);
    close(sockfd);
    exit(0);
}