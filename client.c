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
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>

#define SERVERPORT "4950"

#define MAXBUFLEN 100

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

//Used Beej' Guide (section 6.3) from Quercus as help

int calcdigits(int value) {
    int count = 0;
    while (value >= 10) {
        value = value/10;
        count++;
    }
    return count + 1;
}

int sig = 0;

void set_signal(int signum) {
    sig = 1;
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    int multiplier = 10;

    signal(SIGALRM, set_signal);

    if (argc != 3) {
        fprintf(stderr,"usage (write ftp as is): talker hostname port_address\n");
        exit(1);
    }

    printf("Enter a message in the following format:\nftp <filename>\n");
    char filename[MAXBUFLEN];
    scanf("ftp %s", &filename);


    if (access(filename, F_OK) != -1) {
        printf("%s exists\n", filename);
    } else {
        printf("%s does not exist\n", filename);
        exit(1);
    }

    FILE *file_pointer;
    char temp_buff[1000];

    file_pointer = fopen(filename, "rb");

    if (file_pointer == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    fseek(file_pointer, 0, SEEK_END);
    int file_size = ftell(file_pointer);
    int total_packets = (file_size + 999)/1000;
    int last_size = file_size%1000;
    if (last_size == 0) {
        last_size = 1000;
    }

    //printf("file size: %d --- total packets: %d --- last size: %d\n", file_size, total_packets, last_size);

    fclose(file_pointer);

    char file_data[total_packets][1000];

    FILE *file_descriptor;

    file_descriptor = fopen(filename, "rb");

    if (file_descriptor == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    int count = 0;
    while (fread(temp_buff, 1, 1000, file_descriptor) > 0) {
        for (int i = 0; i < 1000; i++) {
            file_data[count][i] = temp_buff[i];
            if (count == total_packets - 1) {
                if (i == last_size - 1) {
                    break;
                }
            }
        }
        count ++;
    }

    fclose(file_descriptor);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    } 

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    addr_len = sizeof their_addr;
    char buf[MAXBUFLEN];
    int current_size;
    double total_time = 0;
    int repetitions = 0;

    // fd_set readfds;
    struct timeval timeout;
    // FD_ZERO(&readfds);
    // FD_SET(sockfd, &readfds);

    // int flags = fcntl(sockfd, F_GETFL, 0);
    // if (flags == -1) {
    //     perror("fcntl");
    //     close(sockfd);
    //     return 1;
    // }

    // flags |= O_NONBLOCK;
    // if (fcntl(sockfd, F_SETFL, flags) == -1) {
    //     perror("fcntl");
    //     close(sockfd);
    //     return 1;
    // }

    for (int i = 0; i < total_packets + 1; i++) {
        if (i == total_packets) {
            current_size = last_size;
        } else {
            current_size = 1000;
        }

        //printf("total packets: %d --- i: %d --- current size: %d --- filename: %s\n", total_packets, i, current_size, filename);

        char message[calcdigits(total_packets) + calcdigits(i) + calcdigits(current_size) + strlen(filename) + current_size + 4];

        if (i > 0) {
            char str[5];
            char str2[5];
            sprintf(str, "%d", i);
            sprintf(message, "%d", total_packets);
            sprintf(str2, "%d", current_size);
            strcat(message, ":");
            strcat(message, str);
            strcat(message, ":");
            strcat(message, str2);
            strcat(message, ":");
            strcat(message, filename);
            strcat(message, ":");

            //printf("%s\n", message);

            int index = 0;

            while(message[index] != '\0') {
                index++;
            }

            for (int j = 0; j < 1000; j++) {
                message[index + j] = file_data[i - 1][j];
            }
            //printf("%s\n", message);
        }

        struct timeval start;
        struct timeval end;
        struct timeval time_taken;

        if (i == 0) {
            gettimeofday(&start, NULL);

            if ((numbytes = sendto(sockfd, "ftp", strlen("ftp"), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                perror("talker: sendto");
                exit(1);
            }

            printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);

            if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }

            gettimeofday(&end, NULL);
        } else {
            //alarm(4*total_time/(i + 1));
            repetitions = 0;

            while (true) {
                repetitions++;

                if (repetitions == 11) {
                    printf("Not Received 10 times\n");
                    exit(1);
                }
                gettimeofday(&start, NULL);

                // struct itimerval timer;
                // timer.it_value.tv_sec = 0;        // Seconds portion of initial value
                // timer.it_value.tv_usec = 60;      // Microseconds portion of initial value
                // timer.it_interval.tv_sec = 0;     // Seconds portion of interval (0 for one-time timer)
                // timer.it_interval.tv_usec = 0;    // Microseconds portion of interval

                // // Set the timer
                // if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
                //     perror("setitimer");
                //     exit(EXIT_FAILURE);
                // }

                if ((numbytes = sendto(sockfd, message, sizeof(message), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                    perror("talker: sendto");
                    exit(1);
                }

                printf("talker: sent %d bytes to %s ---- sent frag no. %d out of frags %d\n", numbytes, argv[1], i, total_packets);
                // timeout.tv_sec = 1000*total_time/(i + 1);
                // timeout.tv_usec = 0;

                // int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
                // if (ready == -1) {
                //     perror("select");
                //     exit(1);
                // }
                
                // if (!FD_ISSET(sockfd, &readfds)) {
                //     // Timeout occurred
                //     printf("No ACK packet received, preparing to retransmit frag no. %d\n", i);
                // } else {
                    // Data is available to receive
                    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        if (errno == EWOULDBLOCK || errno == EAGAIN) {
                            printf("No ACK packet received, preparing to retransmit frag no. %d\n", i);
                            continue;
                        }
                        if (sig == 1) {
                            sig = 0;
                            printf("No ACK packet received, preparing to retransmit frag no. %d\n", i);
                            continue;
                        }
                        perror("recvfrom");
                        exit(1);
                    }

                    gettimeofday(&end, NULL);

                    break;
                //}
            }
        }
        buf[numbytes] = '\0';

        time_taken.tv_sec = end.tv_sec - start.tv_sec;
        time_taken.tv_usec = end.tv_usec - start.tv_usec;

        if (i == 0) {
            if (strcmp(buf, "yes") == 0) {
                printf("A file transfer can start\n");
            } else {
                exit(1);
            }
        } else {
            printf("Received acknowledgement: %s\n", buf);
        }

        if (i == 0) {
            timeout.tv_sec = multiplier*time_taken.tv_sec;
            timeout.tv_usec = multiplier*time_taken.tv_usec;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == -1) {
                perror("setsockopt");
                exit(1);
            }
        }
    }

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}
