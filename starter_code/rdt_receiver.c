#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"


/*
 * You are required to change the implementation to support
 * window size greater than one.
 * In the current implementation the window size is one, hence we have
 * only one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;
int wanted_seq_num = 0;
int lastpkt_read = 0;

tcp_packet *buffpkt;
tcp_packet recvpkt_buffer[MAX_WINDOW_SIZE]; // packet window
int buffpkt_no = 0;

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;

    /* 
     * check command line arguments 
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        
        if (get_data_size(recvpkt) == 0 && recvpkt->hdr.seqno == wanted_seq_num) {
            VLOG(INFO, "End Of File has been reached");
            fclose(fp);
            break;
        }
        /* 
         * sendto: ACK back to the client 
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);

        sndpkt = make_packet(0);
        sndpkt->hdr.ctr_flags = ACK;
        if (recvpkt->hdr.seqno == wanted_seq_num) { // if the seq number is correct
            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
            sndpkt->hdr.ackno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
            wanted_seq_num = sndpkt->hdr.ackno;

            // if the buffer is not empty
            if (buffpkt_no > 0) {
                // deliver all packets in the buffer
                for (int i = 0; i < buffpkt_no; i++) {
                    buffpkt = &recvpkt_buffer[i];
                    printf("buffer packet seqno: %d\n", buffpkt->hdr.seqno);
                    if (buffpkt->hdr.data_size == 0 && buffpkt->hdr.seqno == wanted_seq_num) {
                        VLOG(INFO, "End Of File has been reached");
                        fclose(fp);
                        break;
                    }
                    fseek(fp, buffpkt->hdr.seqno, SEEK_SET);
                    fwrite(buffpkt->data, 1, buffpkt->hdr.data_size, fp);

                    sndpkt->hdr.ackno = buffpkt->hdr.seqno + buffpkt->hdr.data_size;
                    wanted_seq_num = sndpkt->hdr.ackno;
                }
                // reset buffer
                buffpkt_no = 0;
                memset(recvpkt_buffer, 0, sizeof recvpkt_buffer);
            }
        }
        else if (recvpkt->hdr.seqno > wanted_seq_num) { // if the seq number is out of order
            printf("recvpkt->hdr.seqno %d, wanted_seq_num %d\n", recvpkt->hdr.seqno, wanted_seq_num);
            recvpkt_buffer[buffpkt_no] = *recvpkt; // buffer the packet for the future use
            buffpkt_no++; // increase the buffer index     
            sndpkt->hdr.ackno = wanted_seq_num; // advertise the correct wanted_seq_num
        }
        else { // if dup packet
            sndpkt->hdr.ackno = wanted_seq_num; // advertise the correct wanted_seq_num
        }
        
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        }
    }

    return 0;
}
