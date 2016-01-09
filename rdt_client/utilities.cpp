#include "utilities.h"
#include <stdint.h>

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

void error(const char *msg) {
    perror(msg);
    exit(0);
}
