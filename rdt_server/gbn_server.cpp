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
#include <errno.h>
#include <mutex>
#include <ctime>
#include "utilities.h"

using namespace std;

struct packet *pkts;
timer_t **timer_ids;
int MAX_WINDOW_SIZE;
int sending_window_size = 50;
int remaining_chunks;
int base=0,  next_seq_no=0;
int duplicate_ack_num=0, resend_base=0;
int last_resent=0, timeout_counter=0;
recursive_mutex rec_mutex;

int start_time, end_time;
ofstream congWin("congestionwin.txt");

void send_pkt(int i) {
    output_file << "Server: sending packet " << i << endl;
    cout << "Server: sending packet " << i << endl;
    int index = i%MAX_WINDOW_SIZE;
    socklen_t fromlen = sizeof(struct sockaddr_in);
    int n = sendto(new_sock,(char *)&(pkts[index]), sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
    while (n  < 0) {
        error("send in sending packet, resending...");
        n = sendto(new_sock,(char *)&pkts[index], sizeof(struct packet), 0,(struct sockaddr *)&from,fromlen);
    }
}

void handle_loss_event() {
    output_file << "Server: loss event occured, cnwd became: ";
    cout << "Server: loss event occured, cnwd became: ";
    end_time = clock();
    stop_timer();
    sending_window_size = sending_window_size/2;
    if(sending_window_size == 0)
        sending_window_size = 1;
    output_file << sending_window_size << endl;
    cout << sending_window_size << endl;
    congWin << (end_time-start_time)/double(CLOCKS_PER_SEC)*1000 << "\t" << sending_window_size << endl;
    duplicate_ack_num = 0;
    resend_base = min(base + sending_window_size, next_seq_no);
    output_file << "Server: resending all window...base = " << base << endl << endl;
    cout << "Server: resending all window...base = " << base << endl << endl;
    start_timer();
    for(int i=base; i<resend_base; i++) {
        send_pkt(i);
    }
}

void timeout_handler(int sig_no) {
    output_file << "Server: base timed out, base = " << base<< endl;
    if(sending_window_size == 1)
        timeout_counter++;
    if(timeout_counter==3) {
        cout << endl << "Server: child is not getting responses, it will terminate..." << endl << endl;
        exit(0);
    }
    handle_loss_event();
}


void init_buffers() {
    pkts = (struct packet *)malloc(MAX_WINDOW_SIZE*sizeof(struct packet));
    output_file << "Server: buffers initialized" << endl;
}

void *recv_acks(void *arg) {
    ofstream ackFile("ackfile.txt");
    /*wait for ACKs*/
    int k=0;
    char ack_buf[8];
    socklen_t fromlen = sizeof(struct sockaddr_in);
    while(remaining_chunks || (base == 0 || base != next_seq_no)) {
        int n = recvfrom(new_sock,ack_buf,sizeof(struct ack_packet),0,(struct sockaddr *)&from,&fromlen);
        while (n < 0)  {
            if(errno != EINTR) error("recvfrom");
            errno = 0;
            n = recvfrom(new_sock,ack_buf,sizeof(struct ack_packet),0,(struct sockaddr *)&from,&fromlen);
        }
        struct ack_packet *ack = (struct ack_packet *) ack_buf;
        if(ack->len == 8 && ack->cksum == compute_ack_checksum(ack)) {
	    rec_mutex.lock();
            if(ack->ackno < base) {
                if(ack->ackno != last_resent) { //if duplicate ack was sent but for a newly lost pkt
                    output_file << "Server: incrementing duplicate acks, ackno: " << ack->ackno << endl;
                    duplicate_ack_num ++;
                }
                if(duplicate_ack_num >= 3) {
                    last_resent = ack->ackno;
                    handle_loss_event();
                }
            } else {
                // base was acked ==> sending window slided and increased
                sending_window_size = min(MAX_WINDOW_SIZE, sending_window_size+1);
                end_time = clock();
                congWin << (end_time-start_time)/double(CLOCKS_PER_SEC)*1000 << "\t" << sending_window_size << endl;
                if(resend_base != 0 && resend_base < next_seq_no) {
                    output_file << "Server: resending the already read but not resent pkts, resend_base: " << resend_base << endl;
                    cout << "Server: resending the already read but not resent pkts, resend_base: " << resend_base << endl;
                    int indx = sending_window_size - (resend_base - (base+1));
                    indx = min(resend_base+indx, next_seq_no);
                    for(int i=resend_base; i<indx; i++) {
                        send_pkt(i);
                    }
                    resend_base = indx;
                    output_file << "Server: resend_base became: " << resend_base << endl;
                    cout << "Server: resend_base became: " << resend_base << endl;
                }
            }
            base = ack->ackno + 1;
            if(base == next_seq_no)
                stop_timer();
            else
                start_timer();

            ackFile << " Base became: " << base << endl;
            output_file << " base became: "<<base << endl;
            ackFile << "Server: received ACK " << endl;
            cout << "Server: received ACK " << endl;
            ackFile << "ACK no. :" << ack->ackno << endl;
            cout << "ACK no. :" << ack->ackno << endl;
            ackFile << "ACK len :" << ack->len << endl;
            ackFile << "ACK chksum :" << ack->cksum << endl << endl;
            rec_mutex.unlock();
        }
    }
    ackFile << "Sending ending packet" << endl;
    send_invalid_pkt(0);
    ackFile << "Closing ACK thread" << endl << endl;
    cout << "Closing ACK thread" << endl << endl;
    ackFile.close();
    output_file.close();
    pthread_exit(NULL);
}


int start_gbn_server() {
    int sock, data_size = 504;
    struct sockaddr_in server;
    char file_name[sizeof(struct packet)];

    int port_num;
    int random_seed;
    double loss_prob;
    bool lose = false;


    ifstream inputFile("server.in");
    if(!inputFile.is_open())
        output_file << "ERROR: can't open file\n";
    else {
        string line;
        getline(inputFile, line);
        port_num = atoi(line.c_str());
        getline(inputFile, line);
        MAX_WINDOW_SIZE = atoi(line.c_str());
        getline(inputFile, line);
        random_seed = atoi(line.c_str());
        getline(inputFile, line);
        loss_prob = atof(line.c_str());
        srand(random_seed);
        sock=socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) error("Opening socket");
        int length = sizeof(server);
        bzero(&server,length);
        server.sin_family=AF_INET;
        server.sin_addr.s_addr=INADDR_ANY;
        server.sin_port=htons(port_num);
        if (bind(sock,(struct sockaddr *)&server,length)<0)
           error("binding");
        socklen_t fromlen = sizeof(struct sockaddr_in);

        struct sigaction sig_action;
        sig_action.sa_handler = timeout_handler;
        sigaction (SIGALRM, &sig_action, NULL);

        int dropped_pkts = 0;
        while (1) {
            char file_name_buf[sizeof(struct packet)];
            int n = recvfrom(sock,file_name_buf,sizeof(struct packet),0,(struct sockaddr *)&from,&fromlen);
            if (n < 0) error("recvfrom");
            struct packet *file_name_pkt = (struct packet *)&file_name_buf;
            memcpy(file_name, file_name_pkt->data, file_name_pkt->len - 8);
            output_file << "Server: received file name: " << file_name << endl;
            pid_t pid;
            pid = fork();
            if (pid == -1) perror("Fork failed");
            else if (pid == 0) {   // child process
                init_buffers();
                new_sock = socket(AF_INET, SOCK_DGRAM, 0);
                if (new_sock < 0) error("Opening socket");
                FILE *input = fopen(file_name, "rb");
                if (!input) {
                    send_invalid_pkt(10000);
                    error("File does not exist!");
                    _exit(0);
                }
                char buff[data_size];
                bzero(buff, data_size);
                int length = fread(buff, 1, data_size, input);
                char buff_next[data_size];
                bzero(buff_next, data_size);
                int length_next = fread(buff_next, 1, data_size, input);
                remaining_chunks = length_next;
                pthread_t thread;
                pthread_create(&thread, NULL, recv_acks, NULL);
                while(length > 0) {
                   if((resend_base == next_seq_no) && next_seq_no < base+sending_window_size) {
                       struct packet pkt;
                       pkt.len = length + 8;
                       pkt.seqno = next_seq_no;
                       memcpy(pkt.data, buff, data_size);
                       pkt.cksum = compute_packet_checksum(&pkt);
                       pkts[next_seq_no%MAX_WINDOW_SIZE] = pkt;
                       lose = lose_packet(loss_prob);
                       rec_mutex.lock();
                       if(!lose){
                           send_pkt(next_seq_no);
                       } else {
                           output_file << endl << "Server: packet " << next_seq_no << " dropped.." << endl << endl;
                           cout << endl << "Server: packet " << next_seq_no << " dropped.." << endl << endl;
                           dropped_pkts++;
                       }
                       if(next_seq_no == base)
                            start_timer();
                       next_seq_no++;
                       resend_base++;
                       rec_mutex.unlock();
                       bzero(buff, data_size);
                       memcpy(buff, buff_next, length_next);
                       length = length_next;
                       bzero(buff_next, data_size);
                       length_next = fread(buff_next, 1, data_size, input);
                       remaining_chunks = length_next;
                    }

                }
                cout << "Server: end of sending file\n";
                cout << "Server: total dropped packets: " << dropped_pkts << endl;
                pthread_join(thread, NULL);
                break;
            }
        }
    }
    return 0;
}

