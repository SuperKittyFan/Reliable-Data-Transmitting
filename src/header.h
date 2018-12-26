#ifndef HEADER_FILE
#define HEADER_FILE
#include <stdint.h>
#include <string.h>

#define SYN 1
#define FIN 2
#define ACK 4
#define SYN_ACK 8
#define RST 16
#define HEADER_LEN 16
#define PACKET_LEN 1416
#define MAX_WDSIZE 2000
#define MAX_DUPACK 2
// Normal packet
#define PSH 16

#define PAYLOAD_LEN  1400
#define MAX_SEQ 65536

#define SLOW_START 'S'
#define CONG_AVOID 'C'
#define FAST_RECOV 'F'

struct header
{
    uint32_t seq;
    uint32_t ack;
    
    uint32_t pack_size;
    uint16_t win_size;
    uint8_t flag; // flag: SYN, FIN, ACK, SYN_ACK, PSH
};

struct packet{
    struct header header;
    char *payload;
};

/**
 * read header, return header struct
 **/
struct header read_header(char *bytes)
{
    struct header res;
    memcpy(&res, bytes, sizeof(struct header));
    return res;
}

#endif
