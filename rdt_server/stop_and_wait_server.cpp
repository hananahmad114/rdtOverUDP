/* Creates a datagram server.  The port
   number is passed as an argument.  This
   server runs forever */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <sys/time.h>
#include <signal.h>
#include <cstdlib>
#include <cerrno>
#include "utilities.h"

using namespace std;

int expected_seq_no = 0;
int dropped_pkts = 0;
struct packet curr_pkt;
int new_sock, timeout=2;
struct itimerval timer;
socklen_t fromlen = sizeof(struct sockaddr_in);
ofstream output_file("server.out");
struct sockaddr_in from;
int retries = 0;

void send_pkt(double loss_prob) {
    int n=0;
    output_file << "Server: sending packet " << curr_pkt.seqno << endl;
    cout << "Server: sending packet " << curr_pkt.seqno << endl;
    bool loss = lose_packet(loss_prob);
    if(!loss) {
        n = sendto(new_sock,(char *)&curr_pkt, sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
        while (n < 0) {
            error("Server: send in sending packet, resending...");
            n = sendto(new_sock,(char *)&curr_pkt, sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
        }
    }
    else {
        output_file << endl << "Server: packet " << curr_pkt.seqno << " dropped ..." << endl << endl;
        cout << endl << "Server: packet " << curr_pkt.seqno << " dropped ..." << endl << endl;
        dropped_pkts++;
    }

    /*start timer*/
    start_timer();

    /*wait for ACK*/
    char ack_buf[8];
    n = recvfrom(new_sock,ack_buf,8,0,(struct sockaddr *)&from,&fromlen);
    if (n < 0) error("recvfrom error in send_pkt() ");

    struct ack_packet *ack = (struct ack_packet *) ack_buf;
    while(ack->len == 8 && (ack->ackno != expected_seq_no || compute_ack_checksum(ack) != ack->cksum)) {
        n = recvfrom(new_sock,ack_buf,8,0,(struct sockaddr *)&from,&fromlen);
        if (n < 0) error("recvfrom error in send_pkt() ");
        ack = (struct ack_packet *) ack_buf;
    }
    ack = (struct ack_packet *) ack_buf;
    output_file << "Server: received ACK " << endl;
    cout << "Server: received ACK " << endl;
    output_file << "ACK no. :" << ack->ackno << endl;
    cout << "ACK no. :" << ack->ackno << endl;
    output_file << "ACK len :" << ack->len << endl;
    output_file << "ACK chksum :" << ack->cksum << endl << endl;

    retries = 0;
    stop_timer();

}

void retransmit(int signo) {
    output_file << "Server: retransmitting packet " << curr_pkt.seqno << endl << endl;
    cout << "Server: retransmitting packet " << curr_pkt.seqno << endl << endl;
    int n = sendto(new_sock,(char *)&curr_pkt, sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
    while (n  < 0) {
        error("Server: send in sending packet, resending...");
        n = sendto(new_sock,(char *)&curr_pkt, sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
    }
    retries++;
    if (retries > 6) {
        cout << "Server: maximum retries (6) reached, child exiting ..." << endl;
        output_file << "Server: maximum retries (6) reached, child exiting ..." << endl;
        _exit(0);
    }
    start_timer();
}


int start_stop_and_wait_server() {
    int sock;
    struct sockaddr_in server;
    int data_size = 504;
    char file_name[data_size];
    int port_num;
    int sending_window_size;
    int random_seed;
    double loss_prob;

    ifstream inputFile("server.in");
    if(!inputFile.is_open())
        cout << "ERROR: can't open file\n";
    else {
        string line;
        getline(inputFile, line);
        port_num = atoi(line.c_str());
        getline(inputFile, line);
        sending_window_size = atoi(line.c_str());
        getline(inputFile, line);
        random_seed = atoi(line.c_str());
        getline(inputFile, line);
        loss_prob = atof(line.c_str());

        struct sigaction sig_action;
        sig_action.sa_handler = retransmit;
        sig_action.sa_flags =  SA_RESTART;
        sigaction (SIGALRM, &sig_action, NULL);

        sock=socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) error("Opening socket");
        int length = sizeof(server);
        bzero(&server,length);
        server.sin_family=AF_INET;
        server.sin_addr.s_addr=INADDR_ANY;
        server.sin_port=htons(port_num);
        if (bind(sock,(struct sockaddr *)&server,length)<0)
           error("Server: binding");
        fromlen = sizeof(struct sockaddr_in);
        while (1) {
            char file_name_buf[sizeof(struct packet)];
            int n = recvfrom(sock,file_name_buf,sizeof(struct packet),0,(struct sockaddr *)&from,&fromlen);
            if (n < 0) error("recvfrom");
            struct packet *file_name_pkt = (struct packet *)&file_name_buf;
            memcpy(file_name, file_name_pkt->data, file_name_pkt->len - 8);
            output_file << "Server: received file name: " << file_name << endl;

            pid_t pid;
            pid = fork();
            if (pid == -1) perror("Server: Fork failed");
            else if (pid == 0) {   // child process
               new_sock = socket(AF_INET, SOCK_DGRAM, 0);
               if (new_sock < 0) error("Server: Opening socket");
               FILE *input = fopen(file_name, "rb");
               if (!input) {
                   send_invalid_pkt(10000);
                   error("File does not exist!");
                   _exit(0);
               }
               char buff[data_size];
               bzero(buff, data_size);
               int length = fread(buff, 1, data_size, input);
               while(length) {
                   curr_pkt.len = 8+length;
                   curr_pkt.seqno = expected_seq_no;
                   memcpy(curr_pkt.data,buff,data_size);
                   curr_pkt.cksum = compute_packet_checksum(&curr_pkt);

                   /*send and wait for ACK*/
                   send_pkt(loss_prob);
                   expected_seq_no = (++expected_seq_no) % 2;
                   bzero(buff, data_size);
                   length = fread(buff, 1, data_size, input);
                }
                send_invalid_pkt(0);
                fclose(input);
                cout << "Server: end of sending file\n";
                cout << "Server: total dropped packets: " << dropped_pkts << endl;
                output_file << "Server: end of sending file\n";
                output_file << "Server: total dropped packets: " << dropped_pkts << endl;
                break;
            }
        }
        close(sock);
    }
    return 0;
}

