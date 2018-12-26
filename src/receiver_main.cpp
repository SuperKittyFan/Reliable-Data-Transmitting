/*
 * File:   receiver_main.c
 * Author:
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "header.h"
#include <map>
#include <vector>
#include <deque>
#include <sys/stat.h>
#include <fcntl.h>

#define RCWD 100

typedef struct{
    uint32_t base;
    std::deque<bool> acked;
    std::deque<char*> buffer;
}window;

struct cmp_str
{
    bool operator()(char const *a, char const *b) const
    {
        return strcmp(a, b) < 0;
    }
};

struct sockaddr_in si_me, si_other;
int s, slen; //s: file descripter
char pkg[PACKET_LEN];
char header[HEADER_LEN];
struct packet* pkt;
struct header* hdr;
struct header rthdr;
unsigned long currRCWD=RCWD;
std::map <char*,window,cmp_str> wdMap;
FILE * file;


void diep(char *s) {
    perror(s);
    exit(1);
}

struct packet* _get_pkt(char* pkg, char* dst){
    memcpy(dst,pkg,PACKET_LEN);
    return (struct packet* )dst;
}

inline uint16_t getWdPos(uint16_t base, uint16_t wd_size){

    return 0;
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    struct sockaddr cli;
    socklen_t cliLen;
    slen = sizeof (si_other);
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket");
        exit(1);
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    //printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) ==-1) {
        perror("bind");
        exit(1);
    }

    //printf("Server's ip is %s\n", inet_ntoa(si_me.sin_addr));
    //printf("Server is listening on port %hu\n", myUDPport);


    /* Now receive data and send acknowledgements */
    cliLen =sizeof cli;
    while (true){
        recvfrom(s,pkg,PACKET_LEN,0,&cli,&cliLen);
        //printf("Received a packet from %s : %hu\n", inet_ntoa(((struct sockaddr_in*)&cli)->sin_addr), ntohs(((struct sockaddr_in*)&cli)->sin_port));
        memcpy(header,pkg,HEADER_LEN);
        hdr=(struct header*)header;
        if (hdr->flag==SYN){
            //if new connection: send ack
            //printf("new connection request received\n");
            rthdr.flag=SYN_ACK;
            rthdr.seq=hdr->seq;
            window wd;
            wd.base=hdr->seq+1;
            std::deque<bool> acked(currRCWD,false);
            wd.acked=acked;
            wd.buffer=std::deque<char*>(currRCWD);
            wdMap[cli.sa_data] = wd;
            currRCWD=(uint16_t) RCWD/(wdMap.size());
            rthdr.win_size=currRCWD;

        }else if (hdr->flag==PSH && wdMap.find(cli.sa_data)!=wdMap.end()){
            // if in the middle of transimission: send the corresponding ack and slide the window
            window *currWd=&wdMap[cli.sa_data];

            //update the window size and buffer size
            while (currWd->acked.size()<currRCWD){
                currWd->acked.push_back(false);
                char* newBuf;
                currWd->buffer.push_back(newBuf);
            }
            while (currWd->acked.size()>currRCWD){
                currWd->acked.pop_back();
                if (currWd->buffer[currWd->buffer.size()-1]){
                    delete []currWd->buffer[currWd->buffer.size()-1];
                    currWd->buffer.pop_back();
                }
            }
            int pos=(hdr->seq)-currWd->base;
            if (pos>=currRCWD){
                //printf("discarding package: seq %d, wdBase %d\n",hdr->seq,currWd->base);
            }
            if (pos< 0 && pos+MAX_SEQ>=currRCWD){
                //printf("outdated package: seq %d, wdBase %d\n",hdr->seq,currWd->base);
                rthdr.seq=hdr->seq;
            }else{
                //printf("package received: seq %d, wdBase %d\n",hdr->seq,currWd->base);
                if (pos<0) pos+=MAX_SEQ;
                currWd->buffer[pos] = new char[PACKET_LEN];
                memcpy(currWd->buffer[pos],pkg,PACKET_LEN);
                currWd->acked[pos]=true;
                while (currWd->acked[0]) {
                    rthdr.seq=currWd->base;
                    //get packet_len;
                    struct header tmp_hdr;
                    memcpy(&tmp_hdr,currWd->buffer[0],HEADER_LEN);
                    currWd->base=(currWd->base+1) % MAX_SEQ;
                    currWd->acked.pop_front();
                    currWd->acked.push_back(false);
                    //printf("Writing to file pack num %d size %d\n",rthdr.seq,tmp_hdr.pack_size);
                    write(fileno(file),currWd->buffer[0]+HEADER_LEN,tmp_hdr.pack_size);
                    delete []currWd->buffer[0];
                    currWd->buffer[0];
                    currWd->buffer.pop_front();
                    char* newBuf;
                    currWd->buffer.push_back(newBuf);
                }
            }
            rthdr.flag=ACK;
            rthdr.win_size=(uint16_t) RCWD/(wdMap.size());
        }else if (hdr->flag==FIN && wdMap.find(cli.sa_data)!=wdMap.end()){
            //printf("FIN received\n");
            wdMap.erase(cli.sa_data);
            rthdr.flag=ACK;
            rthdr.ack=hdr->seq;
            rthdr.win_size=currRCWD;
            if (sendto(s,pkg,PACKET_LEN,0,&cli,cliLen)<0) return;
            //printf("Send an ACK packet to %s\n",inet_ntoa(((struct sockaddr_in*)&cli)->sin_addr));
            break;
        }
        //generate response packet
        memcpy(pkg,&rthdr,sizeof(rthdr));
        if (sendto(s,pkg,PACKET_LEN,0,&cli,cliLen)<0) return;
        //printf("Send an ACK packet to %s\n",inet_ntoa(((struct sockaddr_in*)&cli)->sin_addr));
    }
    close(s);
    //printf("%s received.\n", destinationFile);
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;


    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }
    file=fopen(argv[2], "wb");
    if (!file){
        perror("open file fail!");
        exit(0);
    };

    udpPort = (unsigned short int) atoi(argv[1]);

    while (true) reliablyReceive(udpPort, argv[2]);
}

