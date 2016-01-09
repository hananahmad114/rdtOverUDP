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
#include <sys/time.h>
#include <signal.h>
#include "utilities.h"

using namespace std;

int expected_seq_no = 0, timeout = 4;
int sock;
struct sockaddr_in server;
struct packet file_name_pkt;
struct itimerval timer;
ofstream file("client.out");

void start_timer() {
    timer.it_value.tv_sec = timeout;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    int t = setitimer(ITIMER_REAL, &timer, NULL);
    if(t<0)
        cout << "error setting timer" << endl;
    cout << "Client: timer started" << endl;
    file << "Client: timer started" << endl;
}

void stop_timer() {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    int t = setitimer(ITIMER_REAL, &timer, NULL);
    if(t<0)
        cout << "error setting timer" << endl;
    cout << "Client: timer stopped" << endl;
    file << "Client: timer stopped" << endl;
}

void timeout_handler(int sig_no) {
    int n = sendto(sock,(char *)&file_name_pkt, sizeof(struct packet), 0,(struct sockaddr *)&server,sizeof(struct sockaddr_in));
    cout << "in timer handler : file_name "<< file_name_pkt.data <<" "<< n<<endl;
    file << "in timer handler : file_name "<< file_name_pkt.data <<" "<< n<<endl;
    while (n  < 0) {
        error("send in sending packet, resending...");
        n = sendto(sock,(char *)&file_name_pkt, sizeof(struct packet), 0,(struct sockaddr *)&server,sizeof(struct sockaddr_in));
    }
    start_timer();
}


int start_gbn_client() {
    int n;
    unsigned int length;
    struct sockaddr_in from;
    struct hostent *hp;
    char pkt_buf[sizeof(struct packet)];
    string server_address;
    int server_port_num;
    int client_port_num;
    string file_name;
    int receiving_window_size;

    ifstream input_file("client.in");
    if(!input_file.is_open())
        cout << "ERROR: can't open file\n";
    else {
        string line;
        getline(input_file, server_address);
        getline(input_file, line);
        server_port_num = atoi(line.c_str());
        getline(input_file, line);
        client_port_num = atoi(line.c_str());
        getline(input_file, file_name);
        getline(input_file, line);
        receiving_window_size = atoi(line.c_str());

        struct sigaction sig_action;
        sig_action.sa_handler = timeout_handler;
        sig_action.sa_flags = SA_RESTART;
        sigaction (SIGALRM, &sig_action, NULL);

        sock= socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) error("socket");

        server.sin_family = AF_INET;
        hp = gethostbyname(server_address.c_str());
        if (hp==0) error("Unknown host");

        bcopy((char *)hp->h_addr,
             (char *)&server.sin_addr,
              hp->h_length);
        server.sin_port = htons(server_port_num);
        length=sizeof(struct sockaddr_in);

        file_name_pkt.seqno = 0;
        file_name_pkt.len = 8 + strlen(file_name.c_str());
        memcpy(file_name_pkt.data, file_name.c_str(), strlen(file_name.c_str()));
        n=sendto(sock, (char *)&file_name_pkt, sizeof(struct packet), 0, (const struct sockaddr *)&server, length);
        start_timer();
        if (n < 0) error("Sendto");
        ofstream output_file(file_name.c_str(), ios::binary);

        n = recvfrom(sock,pkt_buf,sizeof(struct packet),0,(struct sockaddr *)&from, &length);
        stop_timer();
        while(n){
           if (n < 0) error("recvfrom");
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

           if(pkt->seqno == expected_seq_no && pkt->cksum == compute_packet_checksum(pkt)) {
               int data_size = pkt->len - 8;
               for(int i=0; i<data_size; i++) {
                   output_file << pkt->data[i];
               }
               expected_seq_no++;
           }

           /*send ACK*/
           struct ack_packet *ack = (struct ack_packet *)malloc(sizeof(ack_packet));
           ack->ackno = expected_seq_no-1;
           ack->len = 8;
           ack->cksum = compute_ack_checksum(ack);
           n = sendto(sock, (char *)ack, 8, 0, (const struct sockaddr *)&from, length);
           if(n<0) cout << "error in sending ack\n";
           cout << "Client: ack "<< ack->ackno <<" sent\n\n";
           file << "Client: ack "<< ack->ackno <<" sent\n\n";
           n = recvfrom(sock,pkt_buf,sizeof(struct packet),0,(struct sockaddr *)&from, &length);
       }
       file.close();
       close(sock);
   }

   return 0;
}
