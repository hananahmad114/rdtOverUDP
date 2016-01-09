/* UDP client in the internet domain */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <fstream>
#include <iostream>
#include "utilities.h"

using namespace std;

int start_stop_and_wait_client() {
    int sock, n;
    unsigned int length;
    struct sockaddr_in server, from;
    struct hostent *hp;
    char pkt_buf[512];

    string server_address;
    int server_port_num;
    int client_port_num;
    string file_name;
    int receiving_window_size;

    ifstream inputFile("client.in");
    if(!inputFile.is_open())
        cout << "ERROR: can't open file\n";
    else {
        string line;
        getline(inputFile, server_address);
        getline(inputFile, line);
        server_port_num = atoi(line.c_str());
        getline(inputFile, line);
        client_port_num = atoi(line.c_str());
        getline(inputFile, file_name);
        getline(inputFile, line);
        receiving_window_size = atoi(line.c_str());

        sock= socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) error("Client: error in socket");

        server.sin_family = AF_INET;
        hp = gethostbyname(server_address.c_str());
        if (hp==0) error("Client: unknown host");

        bcopy((char *)hp->h_addr,(char *)&server.sin_addr,hp->h_length);
        server.sin_port = htons(server_port_num);
        length=sizeof(struct sockaddr_in);

        struct packet file_name_pkt;
        file_name_pkt.seqno = 0;
        file_name_pkt.len = 8 + strlen(file_name.c_str());
        memcpy(file_name_pkt.data, file_name.c_str(), strlen(file_name.c_str()));
        n=sendto(sock, (char *)&file_name_pkt, sizeof(struct packet), 0, (const struct sockaddr *)&server, length);
        if (n < 0) error("Sendto");

        ofstream output_file(file_name.c_str(), ios::binary);

        n = recvfrom(sock,pkt_buf,sizeof(struct packet),0,(struct sockaddr *)&from, &length);
        while(n){
           cout << "Client: received packet" << endl;
           file << "Client: received packet" << endl;
           struct packet *pkt = (struct packet *) pkt_buf;
           if(pkt->len == 0) {
               cout << "Client: Ending pkt received" << endl;
               file << "Client: Ending pkt received" << endl;
               output_file.close();
               break;
           } else if(pkt->len > sizeof(struct packet)) {
               cout << "Client: ERROR file not found at server" << endl;
               output_file.close();
               break;
           }
           cout << "pkt length = " << pkt->len << endl;
           cout << "pkt seqNo = " << pkt->seqno << endl;
           file << "pkt length = " << pkt->len << endl;
           file << "pkt seqNo = " << pkt->seqno << endl;

           if (pkt->cksum == compute_packet_checksum(pkt)) {
               int data_size = pkt->len - 8;
               for(int i=0; i<data_size; i++) {
                   output_file << pkt->data[i];
               }
               /*send ACK*/
               struct ack_packet *ack = (struct ack_packet *)malloc(sizeof(ack_packet));
               ack->ackno = pkt->seqno;
               ack->len = 8;
               ack->cksum = compute_ack_checksum(ack);
               n = sendto(sock, (char *)ack, 8, 0, (const struct sockaddr *)&from, length);
               if(n<0) cout << "error in sending ack\n";
               cout << "Client: ack "<< ack->ackno <<" sent\n\n";
               file << "Client: ack "<< ack->ackno <<" sent\n\n";
           } else {
               cout << "Client: packet corrupt, not sending ack" << endl;
               file << "Client: packet corrupt, not sending ack" << endl;
           }
           n = recvfrom(sock,pkt_buf,sizeof(struct packet),0,(struct sockaddr *)&from, &length);
      }
      output_file.close();
      file.close();
      close(sock);
   }
   return 0;
}

