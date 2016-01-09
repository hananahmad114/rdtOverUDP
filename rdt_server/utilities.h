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

extern int new_sock;
extern struct itimerval timer;
extern int timeout;
extern std::ofstream output_file;
extern struct sockaddr_in from;

void start_timer();
void stop_timer();
bool lose_packet(double loss_prob);
void error(const char *msg);
uint16_t compute_packet_checksum(struct packet* pkt);
uint16_t compute_ack_checksum(struct ack_packet* pkt);
void send_invalid_pkt(int len);

#endif // UTILITIES_H
