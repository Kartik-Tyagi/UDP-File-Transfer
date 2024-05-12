// The code majorly utilises section 6.3 of Beej's listener.c file, with some modifications for the lab

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <time.h>

#define MYPORT "4950"   // the port users will be connecting to

#define MAXBUFLEN 1500

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

double genNum() {
    double random_number = (double)rand() / RAND_MAX;
    return random_number;
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    char buf2[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    double droprate = 0.1;

    srand(time(NULL));
    if (argc != 2)
    {
        printf("usage: server <port number>\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n",
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s));
    // printf("listener: packet is %d bytes long\n", numbytes);
    //buf[numbytes] = '\0';
    // printf("listener: packet contains \"%s\"\n", buf);

    // bool checker = strcmp(buf, "ftp") != 0;

    //if (buf[0] ==  "f") && (buf[1] == "t") 
    int file_transfer_started  = 0;

    // if (strcmp(buf, "ftp") == 0)
    if ( (buf[0] ==  'f') && (buf[1] == 't') && (buf[2] == 'p') )
    {
        printf("yes\n");
        char yes[] = "yes";

        if ((numbytes = sendto(sockfd, yes, strlen(yes), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("talker: sendto");
            exit(1);
        }
        file_transfer_started++;
    }
    else
    {
        printf("no\n");
        char no[] = "no";

        if ((numbytes = sendto(sockfd, no, strlen(no), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("talker: sendto");
            exit(1);
        }       
    }

    if (file_transfer_started)
    {
        printf("The file transfer is about to begin\n");
    }

    if (file_transfer_started)
    {
        while(true) {
            double random = genNum();
            printf("Reached\n");
            if ((numbytes = recvfrom(sockfd, buf2, MAXBUFLEN-1 , 0,
                (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }

            char ack[50];
            sprintf(ack, "ACK: Received packet number: %d", 1);

            printf("value of random is %lf\n", random);

            if (random > droprate) {
                if ((numbytes = sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
                    perror("talker: sendto");
                    exit(1);
                }
                break;
            }

            printf("Packet dropped by server\n");
        }   
        // printf("listener: first packet contains \"%s\"\n", buf2);

             
        int frag_no;
        int fileSize;
        char filename[485];        
  
        sscanf(buf2, "%d:%*d:%d:%485[^:]", &frag_no, &fileSize, filename);
        // printf("Total Frags: %d\n", frag_no);
        // printf("Filename: %s\n", filename);
        // printf("Filesize: %d\n", fileSize);
        char content[fileSize];

        FILE *newFile = fopen(filename, "wb");
        if (newFile == NULL) {
            printf("Error opening file!\n");
            return 1;
        }        
        // fclose(newFile);

        int last_colon_index = -1;
        int colon_counter = 0; 

        for (int i = 0; buf2[i] != '\0'; i++) {
            if (buf2[i] == ':') {
                last_colon_index = i;
            }
            if (colon_counter == 4)
            {
                break;
            }            
        }        
        memcpy(content, buf2+last_colon_index+1 , fileSize);
        fwrite(content, sizeof(content[0]), sizeof(content), newFile);

        printf("Content: %s\n", content);



        for (int i = 0; i < frag_no-1; i++)
        {
            while(true) {
                double random = genNum();
                if ((numbytes = recvfrom(sockfd, buf2, MAXBUFLEN-1 , 0,
                    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                    perror("recvfrom");
                    exit(1);
                }            
                // printf("listener: packet contains \"%s\"\n", buf2);

                // fprintf(newFile, content2);

                char ack[50];
                sprintf(ack, "ACK: Received packet number: %d", i+2);

                printf("At packet %d, random is %lf\n", i + 2, random);

                if (random > droprate) {
                    if ((numbytes = sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
                        perror("talker: sendto");
                        exit(1);
                    }   
                    printf("Sending ACK\n");
                    break;
                }

                printf("Packet dropped by server\n");
            }

            int last_colon_index = -1;
            int colon_counter = 0;
            for (int i = 0; buf2[i] != '\0'; i++) {
                if (buf2[i] == ':') {
                    last_colon_index = i;
                    colon_counter++;
                }
                if (colon_counter == 4)
                {
                    break;
                }

            }       
            sscanf(buf2, "%d:%*d:%d:%485[^:]", &frag_no, &fileSize, filename);
            // printf("Total Frags: %d\n", frag_no);
            // printf("Filename: %s\n", filename);
            // printf("Filesize: %d\n", fileSize);
            char content2[fileSize];
            memcpy(content2, buf2+last_colon_index+1 , fileSize);
            // printf("Content: %s\n", content2);
            fwrite(content2, sizeof(content2[2]), sizeof(content2), newFile);

        }
        printf("Closing new file\n");
        fclose(newFile);

    }

    close(sockfd);

    return 0;
}