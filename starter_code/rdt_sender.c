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
#include <math.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  120 //millisecond

int next_seqno = 0;
int send_base = 0;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt;
tcp_packet *sndpkt_window[WINDOW_SIZE]; // 10 packets in this window
tcp_packet *recvpkt;
int lastUnACKed;
int EOF_seqno;
int EOF_read = 0; // 1 if EOF has been read
sigset_t sigmask;

struct timeval sent_times[WINDOW_SIZE]; // keep track of time that a packet was sent
struct timeval oldest_sent_time;
struct timeval cur_time;

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

void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {
        //Resend all packets range between sendBase and nextSeqNum
        VLOG(INFO, "Timeout happened");

        lastUnACKed = (send_base / DATA_SIZE) % WINDOW_SIZE; // lastUnACKed packet's index in the window
        printf("send_base %d, lastUnACKed %d\n", send_base, lastUnACKed);

        for (int i = lastUnACKed; i < WINDOW_SIZE; i++) { // retransmit all packets in the window
            if (sndpkt_window[i]->hdr.seqno >= sndpkt_window[lastUnACKed]->hdr.seqno) { // if older packets
                printf("retransmitting packet %d\n", sndpkt_window[i]->hdr.seqno);
                
                // sndpkt = &sndpkt_window[i];
                if(sendto(sockfd, sndpkt_window[i], TCP_HDR_SIZE + get_data_size(sndpkt_window[i]), 0, 
                            (const struct sockaddr *) &serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }
            }
        }
    }
}

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return fabs((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
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

    // send first 10 packets
    for (int packet_no = 0; packet_no < WINDOW_SIZE; packet_no++) {
        // read the file data
        len = fread(buffer, 1, DATA_SIZE, fp);
        if (len <= 0)
        {
            VLOG(INFO, "End Of File has been reached");
            sndpkt = make_packet(0);
            sndpkt->hdr.seqno = next_seqno;
            // sndpkt_window[packet_no] = *sndpkt; // add packet to the window
            EOF_seqno = next_seqno;
            EOF_read = 1; // mark as read
            sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                    (const struct sockaddr *) &serveraddr, serverlen);
            break;
        }

        sndpkt = make_packet(len);
        sndpkt->hdr.seqno = next_seqno;
        next_seqno += len;
        memcpy(sndpkt->data, buffer, len);
        sndpkt_window[packet_no] = sndpkt; // add packet to the window

        if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                    (const struct sockaddr *) &serveraddr, serverlen) < 0)
        {
            error("sendto");
        }
        VLOG(DEBUG, "Sending packet %d to %s", 
                sndpkt_window[packet_no]->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

        gettimeofday(&sent_times[packet_no], 0);
        if (packet_no == 0) {
            start_timer();
        }
    }

    while (1)
    {
        //Wait for ACK
        do {
            do {
                if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                            (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
                {
                    error("recvfrom");
                }
                recvpkt = (tcp_packet *) buffer;
                assert(get_data_size(recvpkt) <= DATA_SIZE);
                send_base = recvpkt->hdr.ackno; // slide the window

                if (recvpkt->hdr.ackno == EOF_seqno && EOF_read == 1) { // if EOF, terminate the program
                    printf("EOF_seqno %d\n", EOF_seqno);
                    free(sndpkt);
                    return 0;
                }
            }while(recvpkt->hdr.ackno < next_seqno);   //ignore duplicate ACKs

            lastUnACKed = (send_base / DATA_SIZE) % WINDOW_SIZE;

            if (lastUnACKed == 0) {
                lastUnACKed = 10;
            }
            stop_timer();
            gettimeofday(&cur_time, 0);
            oldest_sent_time = sent_times[lastUnACKed];
            int elapsed_time = (int) timedifference_msec(oldest_sent_time, cur_time);
            elapsed_time = RETRY - elapsed_time;
            timer.it_value.tv_sec = elapsed_time / 1000;       // sets an initial value
            timer.it_value.tv_usec = (elapsed_time % 1000) * 1000;
            start_timer();
            
            for (int packet_no = 0; packet_no < lastUnACKed; packet_no++) {
                // read new file data
                len = fread(buffer, 1, DATA_SIZE, fp);
                if (len <= 0)
                {
                    VLOG(INFO, "End Of File has been reached");
                    sndpkt = make_packet(0);
                    sndpkt->hdr.seqno = next_seqno;
                    sndpkt_window[packet_no] = sndpkt; // add packet to the window
                    EOF_seqno = next_seqno;
                    EOF_read = 1; // mark as read
                    sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                            (const struct sockaddr *) &serveraddr, serverlen);
                    break;
                }

                sndpkt = make_packet(len);
                sndpkt->hdr.seqno = next_seqno;
                next_seqno += len;
                memcpy(sndpkt->data, buffer, len);
                sndpkt_window[packet_no] = sndpkt; // add packet to the window

                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                            (const struct sockaddr *) &serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }
                VLOG(DEBUG, "Sending packet %d to %s", 
                        sndpkt_window[packet_no]->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

                gettimeofday(&sent_times[packet_no], 0);
            }
        } while(recvpkt->hdr.ackno != next_seqno); // wait until the ACK is received for the end of the window
    }

    free(sndpkt);
    return 0;

}



