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
<<<<<<< Updated upstream
#define RETRY  120 // millisecond

int next_seqno = 0;
int send_base = 0;
=======
// #define RETRY  120 // millisecond

int next_seqno = 0;
int send_base = 0;
int lastpkt_sent = 0; // last packet number that is sent
>>>>>>> Stashed changes
int cwnd = 1; // for cwnd size
float cwnd_f = 1.0; // for the actual cwnd float size
int ssthresh = 64;
int dup_ackno = 0; // to check dup ACK number
int dup_cnt = 0; // to count dup ACKs

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer;
tcp_packet *sndpkt;
tcp_packet *sndpkt_window[MAX_WINDOW_SIZE]; // packet window
tcp_packet *recvpkt;
int lastUnACKed;
int EOF_seqno;
int EOF_read = 0; // 1 if EOF has been read
sigset_t sigmask;

struct timeval sent_times[MAX_WINDOW_SIZE]; // keep track of the time that a packet was sent
struct timeval oldest_sent_time;
struct timeval cur_time;
float estimated_RTT = 10;
float sample_RTT = 50;
float dev_RTT = 0;
int rto = 180; // default RTO as 3 sec


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
        VLOG(INFO, "Timeout happened");
        // stop_timer();
        // printf("Timer stopped\n");
        ssthresh = floor(fmaxf(cwnd/2, 2));
        cwnd = 1;
        cwnd_f = 1.0;

<<<<<<< Updated upstream
        lastUnACKed = (send_base / DATA_SIZE); // lastUnACKed packet's index in the window
        // printf("send_base %d, lastUnACKed %d\n", send_base, lastUnACKed);

        for (int packet_no = lastUnACKed; packet_no < lastUnACKed + cwnd; packet_no++) { // retransmit all packets in the window
            // if (sndpkt_window[i]->hdr.seqno >= sndpkt_window[lastUnACKed]->hdr.seqno) { // if older packets
            printf("retransmitting packet %d\n", sndpkt_window[packet_no]->hdr.seqno);
            if(sendto(sockfd, sndpkt_window[packet_no], TCP_HDR_SIZE + get_data_size(sndpkt_window[packet_no]), 0, 
                        (const struct sockaddr *) &serveraddr, serverlen) < 0)
            {
                error("sendto");
            }
            // }
=======
        lastpkt_sent = (send_base / DATA_SIZE);
        printf("retransmitting packet %d\n", sndpkt_window[lastpkt_sent]->hdr.seqno);
        if(sendto(sockfd, sndpkt_window[lastpkt_sent], TCP_HDR_SIZE + get_data_size(sndpkt_window[lastpkt_sent]), 0, 
                    (const struct sockaddr *) &serveraddr, serverlen) < 0)
        {
            error("sendto");
>>>>>>> Stashed changes
        }
        // start_timer();
        // printf("Timer started\n");
        // update the sent time for this packet
    }
}

void timeout(int sig)
{
    if (sig == SIGALRM)
    {
        VLOG(INFO, "Timeout happened");
        // resend_packets();
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

    // read and buffer all the data that we want to send in an array
<<<<<<< Updated upstream
    int last_seqno = 0; // last seqno available
=======
    int EOF_seqno = 0; // last seqno available
>>>>>>> Stashed changes
    int buffer_len = 0; // total num of packets in the buffer
    while (1)
    {
        len = fread(buffer, 1, DATA_SIZE, fp);
        if (len <= 0)
        {
            // VLOG(INFO, "End Of File has been reached");
            sndpkt = make_packet(0);
<<<<<<< Updated upstream
            sndpkt->hdr.seqno = -1;
        }

        sndpkt = make_packet(len);
        sndpkt->hdr.seqno = last_seqno;
        last_seqno += len;
=======
            sndpkt->hdr.seqno = EOF_seqno;
            sndpkt_window[buffer_len] = sndpkt;
            buffer_len++;
            break;
        }

        sndpkt = make_packet(len);
        sndpkt->hdr.seqno = EOF_seqno;
        EOF_seqno += len;
>>>>>>> Stashed changes
        memcpy(sndpkt->data, buffer, len);
        sndpkt_window[buffer_len] = sndpkt; // add packet to the buffer
        buffer_len++;
    }
<<<<<<< Updated upstream

    // Stop and wait protocol
    init_timer(RETRY, resend_packets);
    send_base = 0;
    next_seqno = 0;

    // send the first packet
    // // read the file data
    // len = fread(buffer, 1, DATA_SIZE, fp);
    // if (len <= 0)
    // {
    //     VLOG(INFO, "End Of File has been reached");
    //     sndpkt = make_packet(0);
    //     sndpkt->hdr.seqno = next_seqno;
    //     EOF_seqno = next_seqno;
    //     EOF_read = 1; // mark as read
    //     sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
    //             (const struct sockaddr *) &serveraddr, serverlen);
    // }

    // sndpkt = make_packet(len);
    // sndpkt->hdr.seqno = next_seqno;
    // next_seqno += len;
    // memcpy(sndpkt->data, buffer, len);
    // sndpkt_window[0] = sndpkt; // add packet to the window

    // if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
    //             (const struct sockaddr *) &serveraddr, serverlen) < 0)
    // {
    //     error("sendto");
    // }
    // VLOG(DEBUG, "Sending packet %d to %s", 
    //         sndpkt_window[0]->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

    // gettimeofday(&sent_times[0], 0);
    // start_timer();

    while (1)
    {
        // lastpkt_sent = 
        // sndpkt = sndpkt_window[];
        if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                    (const struct sockaddr *) &serveraddr, serverlen) < 0)
        {
            error("sendto");
        }
        /*
        //Wait for ACK
        do {
            do {
                // for every ACK received
                if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                            (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
                {
                    error("recvfrom");
                }
                recvpkt = (tcp_packet *) buffer;
                assert(get_data_size(recvpkt) <= DATA_SIZE);
                send_base = recvpkt->hdr.ackno; // slide the window (set send_base to the next byte we should send)
                if (recvpkt->hdr.ackno == EOF_seqno && EOF_read == 1) { // if EOF, terminate the program
                    // printf("EOF_seqno %d\n", EOF_seqno);
                    free(sndpkt);
                    return 0;
                }

                if (dup_ackno == recvpkt->hdr.ackno) {
                    dup_cnt++; // increase counter for dup ACKs
                }
                dup_ackno = recvpkt->hdr.ackno;
                if (dup_cnt == 3) { // Fast Retransmit
                    // resend packets
                    // ssthresh = max(cwnd/2, 2)
                    // cwnd = 1
                    // ssthresh = floor(fmaxf(cwnd/2, 2));
                    // cwnd = 1;
                    // cwnd_f = 1.0;
                    dup_cnt = 0; // set dup count to 0
                }
            }while(recvpkt->hdr.ackno < next_seqno); // ignore duplicate ACKs

            // change window size (congestion control)
            if (cwnd < ssthresh) { // Slow Start
                cwnd++;
            }
            else { // Congestion Avoidance
                cwnd_f += 1/cwnd;
                cwnd = floor(cwnd_f);
            }

            // lastUnACKed = (send_base / DATA_SIZE) % cwnd;
            // if (lastUnACKed == 0) {
            //     lastUnACKed = cwnd;
            // }
            lastUnACKed = (send_base / DATA_SIZE);

            // reconfigure the timer with the lastUnACKed packet's sent_time
=======
    printf("EOF_seqno %d\n", EOF_seqno);

    // Stop and wait protocol
    init_timer(rto, resend_packets);

    // send_base: last_packet_sent (start of the window in bytes)
    // next_seqno: end of the window in bytes
    while (1)
    {
        // send packets in the current window (cwnd)
        lastpkt_sent = (send_base / DATA_SIZE);
        for (int packet_no = lastpkt_sent; packet_no < lastpkt_sent + cwnd; packet_no++) {
            sndpkt = sndpkt_window[packet_no];
            printf("Sending the packet %d, cwnd %d\n", sndpkt->hdr.seqno, cwnd);
            // printf("cwnd %d\n", cwnd);
            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                        (const struct sockaddr *) &serveraddr, serverlen) < 0)
            {
                error("sendto");
            }
            gettimeofday(&sent_times[packet_no], 0);
            next_seqno += get_data_size(sndpkt); // update next_seqno for this window
            if (get_data_size(sndpkt) == 0) {
                VLOG(INFO, "End Of File has been reached");
                EOF_read = 1;
                break;
            }
        }
        
        start_timer();
        // printf("Timer started\n");

        // wait for an ACK
        do {
            if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                    (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
            {
                error("recvfrom");
            }
            recvpkt = (tcp_packet *) buffer;
            assert(get_data_size(recvpkt) <= DATA_SIZE);
            send_base = recvpkt->hdr.ackno; // slide the window (set send_base to the next byte we should send)
            
            if (recvpkt->hdr.ackno == EOF_seqno && EOF_read == 1) { // if EOF, terminate the program
                // printf("EOF_seqno %d\n", EOF_seqno);
                free(sndpkt);
                return 0;
            }
            // printf("recvpkt->hdr.ackno %d, next_seqno %d\n", recvpkt->hdr.ackno, next_seqno);
            
            // Fast Retransmit
            if (dup_ackno == recvpkt->hdr.ackno) {
                dup_cnt++; // increase counter for dup ACKs
            }
            dup_ackno = recvpkt->hdr.ackno;
            if (dup_cnt == 3) { // Fast Retransmit
                // resend packets
                printf("Fast Retrasmit for packet %d\n", dup_ackno);
                resend_packets(SIGALRM);
                dup_cnt = 0; // set dup count to 0
            }
            
            // handle timer
>>>>>>> Stashed changes
            stop_timer();
            // printf("Timer stopped\n");

            // update timer with RTT estimator
            lastUnACKed = send_base / DATA_SIZE;
            gettimeofday(&cur_time, 0);
            oldest_sent_time = sent_times[lastUnACKed];
            sample_RTT = (int) timedifference_msec(oldest_sent_time, cur_time); // sample RTT
            estimated_RTT = (1.0 - 0.125) * estimated_RTT + 0.125 * sample_RTT;
            dev_RTT = (1.0 - 0.25) * dev_RTT + 0.25 * fabs(sample_RTT - estimated_RTT);
            rto = (int) (estimated_RTT + (4 * dev_RTT));
            // elapsed_time = RETRY - elapsed_time;
            // timer.it_value.tv_sec = elapsed_time / 1000;
            // timer.it_value.tv_usec = (elapsed_time % 1000) * 1000;

            start_timer();
<<<<<<< Updated upstream
            
            // send the next window
            for (int packet_no = lastUnACKed; packet_no < lastUnACKed + cwnd; packet_no++) {
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
=======
            // printf("Timer started\n");
        } while(recvpkt->hdr.ackno < next_seqno);   // ignore duplicate ACKs
>>>>>>> Stashed changes

        stop_timer();
        // printf("Timer stopped\n");

<<<<<<< Updated upstream
                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                            (const struct sockaddr *) &serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }

                // window
                // rtt estimator
                // slow start, congestion avoidance , etc
                
                VLOG(DEBUG, "Sending packet %d to %s", 
                        sndpkt_window[packet_no]->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

                gettimeofday(&sent_times[packet_no], 0);
            }
        } while(recvpkt->hdr.ackno != next_seqno); // wait until the ACK is received for the end of the window
        */
=======
        // update the window size (cwnd)
        if (cwnd < ssthresh) { // Slow Start
            cwnd_f += 1.0;
        }
        else { // Congestion Avoidance
            cwnd_f += 1/cwnd;
        }
        cwnd = floor(cwnd_f);
>>>>>>> Stashed changes
    }

    free(sndpkt);
    return 0;

}



