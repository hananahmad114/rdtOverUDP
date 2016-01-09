#ifndef UTILITIES_H
#define UTILITIES_H

#include <fstream>
#include <stdint.h>
#include <sys/time.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>

struct packet {
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data[504]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};

extern std::ofstream file;

uint16_t compute_packet_checksum(struct packet* pkt);
uint16_t compute_ack_checksum(struct ack_packet* pkt);
void error(const char *);

#endif // UTILITIES_H
