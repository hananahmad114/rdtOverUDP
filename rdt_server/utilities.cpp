#include "utilities.h"
#include <iostream>

using namespace std;

void start_timer() {
    timer.it_value.tv_sec = timeout;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    int t = setitimer(ITIMER_REAL, &timer, NULL);
    if(t<0)
        cout << "error setting timer" << endl;
    output_file << "Server: timer started" << endl;
}

void stop_timer() {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    int t = setitimer(ITIMER_REAL, &timer, NULL);
    if(t<0)
        cout << "error setting timer" << endl;
    output_file << "Server: timer stopped" << endl;
}

bool lose_packet(double loss_prob){
    return ((rand()%100) < (loss_prob * 100));
}

void error(const char *msg) {
    perror(msg);
    exit(0);
}

uint16_t compute_packet_checksum(struct packet* pkt){
    uint32_t sum = 0x0;
    uint16_t*pkt_ptr = (uint16_t*)(pkt);
    uint16_t* ptr = (uint16_t*)&(pkt->data);
    for(int i=2; i<sizeof(struct packet);i+=2){
       if(i<8){
           sum += *(++pkt_ptr);
       }else{
           sum += *(ptr++);
       }
       if(sum > 0xffff){
           sum -= 0xffff;
       }
    }
    return ~(sum);
}


uint16_t compute_ack_checksum(struct ack_packet* pkt){
    uint32_t sum = 0x0;
    uint16_t*pkt_ptr = (uint16_t*)(pkt);
    for(int i=2; i<sizeof(struct ack_packet);i+=2){
        sum += *(++pkt_ptr);
        if(sum > 0xffff){
            sum -= 0xffff;
        }
    }
    return ~(sum);
}

void send_invalid_pkt(int len) {
    socklen_t fromlen = sizeof(struct sockaddr_in);
    struct packet pkt;
    pkt.len = len;
    int n = sendto(new_sock,(char *)&(pkt), sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
    while (n  < 0) {
        error("send in sending packet, resending...");
        n = sendto(new_sock,(char *)&pkt, sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
    }
}
