/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unordered_map>

#include "header.h"
#define INIT_CWND 30

struct sockaddr_in si_other;
int s, slen;
//int win_size = 30;
uint32_t cwnd = INIT_CWND;
uint32_t ssth = 64 << 8;
int dup_ack = 0;
uint32_t base = 1;
bool timeout;
struct timeval RTO;
long int rto=30;
double rem;

char pkg[PACKET_LEN];
struct header hdr;
FILE *fp;


void diep(char *s) {
    perror(s);
    exit(1);
}


inline size_t make_packet(char* pkg, struct header* header){
    size_t res=fread(pkg+HEADER_LEN,1,PAYLOAD_LEN,fp);
    header->pack_size=(uint32_t)res;
    memcpy(pkg,header,HEADER_LEN);
    return res;
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */
    slen = sizeof (si_other);
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    //set the timeout
    RTO.tv_sec=0;
    RTO.tv_usec=300000;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&RTO,sizeof(RTO)) < 0) {
        perror("Set timer fail!");
    }
    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }


    /* Send data and receive acknowledgements on s*/
    uint32_t num_acked = 0;
    uint32_t num_sended =0;
    uint32_t next_seq = 1;
    int timeout_counter=1;
    uint32_t total_pack = (bytesToTransfer / PAYLOAD_LEN) + (bytesToTransfer%PAYLOAD_LEN?1:0);
    //printf("total pack: %d\n",total_pack);
    char cur_state = SLOW_START;
    double  duration_time;
    bool newAck=false;
    bool dupAck=false;
    struct timeval cur_time;
    struct timeval *pack_time = new struct timeval;
    struct timeval start_time;
    gettimeofday(&start_time, nullptr);
    std::unordered_map<uint32_t, char*> unacked_packs;
    std::unordered_map<uint32_t, struct timeval*> pack_times;

    //send initial SYN
    hdr.flag=SYN;
    hdr.seq=0;
    memcpy(pkg,&hdr,sizeof(hdr));
    if (sendto(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other))<0) return;
    //printf("Send a SYN packet to %s\n", inet_ntoa(si_other.sin_addr));

    //receive ack
    socklen_t si_len=sizeof(si_other);
    while (recvfrom(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, &si_len)<0){
        //printf("Timeout, retransmitting....");
        if (sendto(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other))<0) return;
        //printf("Send a SYN packet to %s\n", inet_ntoa(si_other.sin_addr));
    };
    //printf("Received a SYN/ACK packet from %s : %hd\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));

    //reset timer
    RTO.tv_sec=0;
    RTO.tv_usec=30;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&RTO,sizeof(RTO)) < 0) {
        perror("Set timer fail!");
    }

    //start transmitting!!
    while (num_acked < total_pack) {

        //receiving packet
        dupAck=false;
        newAck=false;
        ////printf("Waiting for packet from %s : %hd\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        while(recvfrom(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, &si_len)>0){
            //printf("Received a SYN/ACK packet from %s : %hd\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
            memcpy(&hdr,pkg,HEADER_LEN);
            gettimeofday(pack_time,NULL);
            //printf("debug: hdr.seq: %d, base: %d, cwnd: %d\n",hdr.seq,base,cwnd);
            //printf("hdr.flag==ACK: %d, hdr.seq>=base-1: %d, hdr.seq+MAX_SEQ>=base-1 %d,hdr.seq-base<cwnd,%d,hdr.seq+MAX_SEQ-base<cwnd,%d ",hdr.flag==ACK
            //,hdr.seq>=base-1, hdr.seq+MAX_SEQ>=base-1,hdr.seq-base<cwnd,hdr.seq+MAX_SEQ-base<cwnd);
            if (hdr.flag==ACK && ((hdr.seq>=base-1 && hdr.seq<cwnd+base )|| (hdr.seq+MAX_SEQ>=base-1  && hdr.seq+MAX_SEQ<cwnd+base))){
                //the sequence number is in the window range
                if (unacked_packs.find(hdr.seq)!=unacked_packs.end()){
                    //printf("NEW ack of Package %d received\n",hdr.seq);
                    for (int seq=base; seq<=hdr.seq;seq++){
                        if (unacked_packs.find(seq)!=unacked_packs.end()) {
                            delete[]unacked_packs[seq];
                            unacked_packs.erase(seq);
                            //delete pack_times[seq];
                            //pack_times.erase(seq);
                            newAck = true;
                            num_acked++;
                            if (num_acked==total_pack){
                                //printf("\n");
                            };
                        };
                    }
                    base=(hdr.seq+1)%MAX_SEQ;
                    //printf("new base: %d\n",base);
//                    //move forward the window
//                    int counter=0;
//                    while (unacked_packs.find(base)==unacked_packs.end() && counter<cwnd) {
//                        base++;
//                        counter++;
//                    }
                }else{
                    //printf("DUP ack of Package %d received\n",hdr.seq);
                    dupAck=true;
                    dup_ack+=1;
                    if (dup_ack>MAX_DUPACK){
                        //printf("Retransmitting Seq %d....\n",(hdr.seq+1)%MAX_SEQ);
                        hdr.seq=(hdr.seq+1)%MAX_SEQ;
                        if (sendto(s, unacked_packs[hdr.seq], PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
                            perror("Sender error: sendto");
                            //printf("ERRNO: %s\n", strerror(errno));
                            exit(1);
                        };
                        //gettimeofday(pack_time,NULL);
                        //pack_times[hdr.seq] = pack_time;
                        dup_ack=0;
                    }
                }
            }
        };

        if (hdr.win_size>0 && cwnd>hdr.win_size) cwnd=hdr.win_size;
        uint32_t win_ceiling = (base+cwnd) % MAX_SEQ;


        // Send all unsent in-window packets;
        while (num_sended<total_pack && (next_seq < win_ceiling || next_seq + MAX_SEQ < win_ceiling)) {
            hdr.flag=PSH;
            hdr.seq=next_seq;
            hdr.pack_size=PAYLOAD_LEN;
            u_int32_t res=make_packet(pkg,&hdr);
            if (res<PAYLOAD_LEN) {
                total_pack=num_sended+1;
            }else if (num_sended==total_pack-1){
                if (bytesToTransfer%PAYLOAD_LEN){
                    hdr.pack_size=bytesToTransfer%PAYLOAD_LEN;
                    memcpy(pkg,&hdr,HEADER_LEN);
                }
                //printf("last pack: %d bytes\n",hdr.pack_size);
            }
            unacked_packs[next_seq] = new char[PACKET_LEN];
            memcpy(unacked_packs[next_seq],pkg,PACKET_LEN);
            //printf("Sending a new package %d... total pack= %d\n", hdr.seq,total_pack);
            if (sendto(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
                perror("Sender error: sendto");
                //printf("ERRNO: %s\n", strerror(errno));
                exit(1);
            };
            //gettimeofday(pack_time,NULL);
            //pack_times[next_seq] = pack_time;
            next_seq = (next_seq+1) % MAX_SEQ;
            num_sended++;
        }

        /* checking timeout*/
//        if (base>total_pack) break;
//        struct timeval *check_timeout = pack_times[base];
        gettimeofday(&cur_time, NULL);
        long int time_dur;
        //printf("time duration: %d\n",time_dur);
        time_dur = (cur_time.tv_sec - (pack_time->tv_sec))*1000 + (cur_time.tv_usec - pack_time->tv_usec)/1000;
        timeout = time_dur>rto;
        //timeout=false;

        /*change state*/
        if (timeout) {
            timeout_counter++;
            //printf("Timeout, retransmitting packet %d\n",base);
            gettimeofday(pack_time, nullptr);
            if (sendto(s, unacked_packs[base], PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1) {
                perror("Sender error: sendto");
                //printf("ERRNO: %s\n", strerror(errno));
                exit(1);
            }
            //pack_time = new struct timeval;
            //gettimeofday(pack_time,NULL);
            //pack_times[base] = pack_time;
            ssth = cwnd/2 + 1;
            cwnd = INIT_CWND;
            dup_ack = 0;
            cur_state=SLOW_START;
        }
        if (dupAck){
            if (dup_ack > MAX_DUPACK) {
                cur_state = FAST_RECOV;
                ssth = cwnd/2 + 1;
                cwnd = ssth + 3;
            }
        }
        if (newAck) {
            if (cur_state == FAST_RECOV) {
                cwnd = ssth;
                dup_ack = 0;
            } else if (cur_state == SLOW_START) {
                cwnd++;
                dup_ack = 0;
                if (cwnd>ssth){
                    cur_state=CONG_AVOID;
                }
            } else {
                rem += 1 / (double) cwnd;
                if (rem >= 1) {
                    cwnd += 1;
                    rem--;
                }
                dup_ack = 0;
            }
        }


    }

    //reset timer
    RTO.tv_sec=0;
    RTO.tv_usec=300000;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&RTO,sizeof(RTO)) < 0) {
        perror("Set timer fail!");
    }
    //printf("Closing the socket\n");
    //send initial SYN
    hdr.flag=FIN;
    hdr.seq=0;
    memcpy(pkg,&hdr,sizeof(hdr));
    if (sendto(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other))<0) return;
    //printf("Send a FIN packet to %s\n", inet_ntoa(si_other.sin_addr));

    //receive ack
    while (recvfrom(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, &si_len)<0){
        printf("Timeout, retransmitting....");
        if (sendto(s, pkg, PACKET_LEN, 0, (struct sockaddr*)&si_other, sizeof(si_other))<0) return;
        //printf("Send a FIN packet to %s\n", inet_ntoa(si_other.sin_addr));
    };
    //printf("Received a ACK packet from %s : %hd\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));

    close(s);
    struct timeval end_time;
    gettimeofday(&end_time, nullptr);
    duration_time = (end_time.tv_sec - (start_time.tv_sec)) + (end_time.tv_usec - start_time.tv_usec)/1000000;
    printf( "duration: %lf seconds\n total %d timeouts\n", duration_time ,timeout_counter);

}


/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);



    return (EXIT_SUCCESS);
}


