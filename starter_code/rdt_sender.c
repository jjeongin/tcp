#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  120 //millisecond

int next_seqno = 0;
int send_base = 0;
int window_size = 1;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt;
tcp_packet *sndpkt_window[WINDOW_SIZE]; // 10 packets in this window
struct timeval current_packet_time;
struct timeval packet_times[WINDOW_SIZE]; // keep track of time that a packet was sent
tcp_packet *recvpkt;
sigset_t sigmask;


void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {
        //Resend all packets range between 
        //sendBase and nextSeqNum
        VLOG(INFO, "Timeout happened");
        for (int i; i < WINDOW_SIZE; i++) { // retransmit all packets in the window
            if(sendto(sockfd, sndpkt_window[i], TCP_HDR_SIZE + get_data_size(sndpkt_window[i]), 0, 
                        (const struct sockaddr *) &serveraddr, serverlen) < 0)
            {
                error("sendto");
            }
        }
    }
}


void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}


void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}


/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int)) 
{
    signal(SIGALRM, sig_handler);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}


int main (int argc, char **argv)
{
    int portno, len;
    int next_seqno;
    char *hostname;
    char buffer[DATA_SIZE];
    FILE *fp;

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL) {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");


    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    //Stop and wait protocol

    init_timer(RETRY, resend_packets);
    send_base = 0;
    next_seqno = 0;
    while (1)
    {
        send_base = next_seqno;

        for (int packet_no = 0; packet_no < WINDOW_SIZE; packet_no++) {
            // read the file data
            len = fread(buffer, 1, DATA_SIZE, fp);
            if (len <= 0)
            {
                VLOG(INFO, "End Of File has been reached");
                sndpkt = make_packet(0);
                sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                        (const struct sockaddr *) &serveraddr, serverlen);
                break;
            }

            sndpkt = make_packet(len);
            sndpkt->hdr.seqno = next_seqno;
            next_seqno += len;
            memcpy(sndpkt->data, buffer, len);
            sndpkt_window[packet_no] = sndpkt; // add packet to the window

            /*
            * If the sendto is called for the first time, the system will
            * will assign a random port number so that server can send its
            * response to the src port.
            */
            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                        (const struct sockaddr *) &serveraddr, serverlen) < 0)
            {
                error("sendto");
            }

            gettimeofday(&packet_times[packet_no], 0);

            // start timer after sending the first packet
            if (packet_no == 0) {
                current_packet_time = packet_times[packet_no]; // set current_packet_time as the time that 1st packet was sent
                start_timer(); // start timer for the window
            }
        }

        //Wait for ACK
        do {
            VLOG(DEBUG, "Sending packet %d to %s", 
                    send_base, inet_ntoa(serveraddr.sin_addr));

            // start_timer();
            //ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
            //struct sockaddr *src_addr, socklen_t *addrlen);
            do
            {
                if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                            (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
                {
                    error("recvfrom");
                }
                recvpkt = (tcp_packet *) buffer;
                assert(get_data_size(recvpkt) <= DATA_SIZE);
            }while(recvpkt->hdr.ackno < next_seqno);    // ignore duplicate ACKs
            
            stop_timer();
            
            printf("ackno %d, next_seqno %d\n", recvpkt->hdr.ackno, next_seqno);
            /* resend pack if don't recv ACK */
        } while(recvpkt->hdr.ackno != next_seqno);

        init_timer(RETRY + recvpkt->hdr.ackno/len, resend_packets);
    }

    free(sndpkt);
    return 0;

}



