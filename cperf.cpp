#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <queue>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <tins/tins.h>
#include "sdn_types.h"

using namespace std;

//hX.tr
char *file_name;
char *this_host;

//queue of demands; queued in the order of the hX.tr file, split according to CHUNK_SIZE
queue<struct demand *> *finalized_demands;
    
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

vector<char *> *split_demands; //FREE ALL DEMAND VECTORS/QUEUES
queue<char *> *allocations; //allocations from arbiter

//chunk size in bytes
long CHUNK_SIZE = 1000000;

//size of payload of each packet
int packet_size = 1000;

//amount of time to sleep between sending individual packets
long ind_packet_delay = 100000;

//------------------------------thread I: send demands to arbiter------------------------------//



//return int representation of number of bytes specified by the string 'num'
long get_num_bytes(char *num) {
    char *last = &num[strlen(num) - 1];
    //printf("num:%s, last:%s.\n", num, last);
    
    while(*last != 'M' && *last != 'K') {
        last--;
    }
    long mult = 1;
    if(*last == 'M') {
        mult = 1000000;
        *last = '\0';
    }
    else if(*last == 'K') {
        mult = 1000;
        *last = '\0';
    }
    long num_bytes = atol(num) * mult;
    return num_bytes;
}

//setup addrinfo struct for TCP connection to arbiter to send demands
int connect_arbiter() {
    int sockfd = -1;
    struct addrinfo hints, *res, *next;
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = 0;
    if((rv = getaddrinfo(ARBITER_IP, ARBITER_PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(next = res; next != NULL; next = next->ai_next) {
        if ((sockfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, next->ai_addr, next->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    
    if(next == NULL) {
        fprintf(stderr, "a client: failed to connect\n");
        exit(2);    
    }
    
    return sockfd;
}

void send_demands(vector<char *> *demands) {

    for(int i = 0; i < demands->size(); i++) {
        int sockfd = connect_arbiter(); //new connection for each message sent - avoid read/write race condition
        char *demand = (*demands)[i];
        size_t length = strlen(demand);
        size_t num_written = 0;
        printf("sending: %s\n", demand);
        
        if((num_written = write(sockfd, demand, length)) == -1)
            perror("write");
        
        close(sockfd);
    }
}

//compose strings of multiple demands to make fewer calls to the arbiter
vector<char *> *aggregate_demands() {
    vector<char *> *aggregate = new vector<char *>();
    
    char buff[LINE_LEN * NUM_DEMANDS];
    memset(buff, 0, sizeof buff);
    char *curr_pos = &buff[0];
    int curr_agg_count = 0; //number of demands in the current aggregation
    for(int i = 0; i < split_demands->size(); i++) {
      
        //printf("a agg: %s\n", &buff[0]);
        char *demand = (*split_demands)[i];
        memcpy(curr_pos, demand, strlen(demand));
        curr_pos += strlen(demand);
        memcpy(curr_pos, END_FLAG, strlen(END_FLAG));
        curr_pos += strlen(END_FLAG);
        
        curr_agg_count ++;
        //printf("b agg: %s\n", &buff[0]);
   
        if(curr_agg_count == NUM_DEMANDS || i == split_demands->size() - 1) {
            //add aggregation to list of demands
            //printf("c agg: %s\n", &buff[0]);
            size_t new_len = strlen(buff) + 1; //string length - only copy useful bytes
            char *agg_demand = (char *) malloc(new_len);
            memset(agg_demand, 0, new_len);
            memcpy(agg_demand, buff, strlen(buff));
            aggregate->push_back(agg_demand);
            //printf("new aggregation: %s\n", agg_demand);
            
            //reset buffer for new aggregation
            memset(buff, 0, sizeof buff);
            curr_pos = &buff[0];
            curr_agg_count = 0;
        }
    }
        
    return aggregate;
}

//parse a demand, then add it to the appropriate queue - called by queue_demands()
void add_to_queue(char *demand_str) { //queue<struct demand *> *queue) {
    int flag_size = 3;
        
    //allocate struct and initialize
    struct demand *demand = (struct demand *) malloc(sizeof (struct demand)); //DONT FORGET TO FREE A DEMAND WHEN POPPING OUT OF QUEUE
    memset(demand->src, 0, sizeof demand->src);
    memset(demand->dest, 0, sizeof demand->dest);
    memset(demand->port, 0, sizeof demand->port);
    memset(demand->vlan, 0, sizeof demand->vlan); //need for later - vlan allocated for each demand
    
    //parse demand to fill out struct
    char *dest_loc = strstr(demand_str, "-d") + flag_size + 1; //1 extra to get rid of 'h'
    char *port_loc = strstr(demand_str, "-p") + flag_size;
    char *size_loc = strstr(demand_str, "-n") + flag_size;
    int dest_len = port_loc - dest_loc - flag_size - 1;
    int port_len = size_loc - port_loc - flag_size - 1;
    
    //memcpy(demand->src, src, strlen(src)); //src is redundant in cperf
    memcpy(demand->dest, dest_loc, dest_len);
    memcpy(demand->port, port_loc, port_len);
    demand->size = atol(size_loc);
    
    //printf("host queueing demand: dst=%s size=%d\n", demand->dest, demand->size);
    
    pthread_mutex_lock(&queue_mutex);
    finalized_demands->push(demand);
    pthread_mutex_unlock(&queue_mutex);
}

//create demand of specified size - called by split_all_demands to create smaller demands from a large one
//also places a struct demand in the finalized_queue for easy sending once allocated demands are returned from the arbiter
void create_demand(char *info, long size) {
    //convert size to a string to be added to the demand string - safe version if chunk size > 10
    int flag_size = 3;
    int num_digits = 1;
    long orig = size;
    int dec_radix = 10;
    while(size > dec_radix) {
        size = size / dec_radix; 
        num_digits++;
    }
    char buff[num_digits + 1];
    memset(buff, 0, sizeof buff);
    sprintf(buff, "%ld", orig);
    
    //concatenate strings
    size_t demand_len = strlen(info) + strlen(buff) + 1;
    char *demand = (char *) malloc(demand_len);
    memset(demand, 0, demand_len);
    char *size_loc = strstr(info, "-n") + flag_size;
    int cpylen = size_loc - info;
    memcpy(demand, info, cpylen);
    memcpy(&demand[cpylen], buff, strlen(buff));
    
    printf("new demand: %s\n", demand);
    split_demands->push_back(demand);
    add_to_queue(demand);
}

/* raw demands from input file sent here;
 * split each demand into 'chunk-sized' demands and queue for sending to arbiter
 */
void split_all_demands(vector<char *> *demands) {
    for(int i = 0; i < demands->size(); i++) {
        char *demand = (*demands)[i];
        
        int size_flag_pos = 3; //-d, -p, -n
        char flag_start = '-';
        int size_start = 3; //index of first char of size (index of 5 in "-n 50M")
        
        //loop through string to get number of bytes in unsplit demand
        char *pos = strchr(demand, flag_start);
        for(int f = 0; f < size_flag_pos - 1; f++) {
            pos = strchr(pos+1, flag_start);
        }
        pos += size_start; //pos would now be '50M'
        
        //remove 'M', get int representation of size
        long demand_size = get_num_bytes(pos); //would now be 50,000,000
            
        /*
        char buff[strlen(pos)];
        memset(&buff, 0, sizeof buff);
        if(pos[strlen(pos)-1] == '\n')
            memcpy(&buff, pos, strlen(pos)-2); //get rid of '\0' and 'M'
        else
            memcpy(&buff, pos, strlen(pos)-1); //get rid of '\0' and 'M'

        int demand_size = atoi(&buff[0]);
        */
        
        //create new demand of chunk size or less
        char info[strlen(demand) - strlen(pos) + 10]; //demand without num bytes, reuse for new demand
        memset(&info, 0, sizeof info);
        memcpy(&info, demand, sizeof info - 1); //keeps an extra space at the end - taken advantage of in create_demand
        
        while(demand_size > 0) {
            if(demand_size > CHUNK_SIZE) {
                create_demand(&info[0], CHUNK_SIZE);
                demand_size = demand_size - CHUNK_SIZE;
            }
            else {
                create_demand(&info[0], demand_size);
                demand_size = 0;
            }
        }
    }
}

vector<char *> *read_input(char *file_name) {
    vector<char *> *demands = new vector<char *>();
    int flag_size = 3;
    
    FILE *traffic_spec = fopen(file_name, "r");
    
    char buff[LINE_LEN];
    size_t buff_size = sizeof buff;
    char *next_demand = &buff[0];
    memset(&buff, 0, buff_size);
    
    while(getline(&next_demand, &buff_size, traffic_spec) > 0 and *next_demand != '\n') {
        char *demand = (char *) malloc(buff_size);
        memset(demand, 0, buff_size);
        memcpy(demand, next_demand, buff_size);
        demands->push_back(demand);
        //printf("read in: %s.\n", demand);
    }
    
    fclose(traffic_spec);
    return demands;
}

void free_demands(vector<char *> *demands) {
    for(int i = 0; i < demands->size(); i++) {
        free((*demands)[i]);
    }
    free(demands);
}

void *initiate_send(void *ptr) {
    vector<char *> *demands = aggregate_demands();
    
    send_demands(demands);
    free_demands(demands);
    
    return NULL;
}



//------------------------------------end sending thread------------------------------------//




//------------------------------------receiving thread------------------------------------//
//receiving thread receives demands from the arbiter, and subsequently sends them to the appropriate host



int get_recv_socket() {
    int sockfd = -1;
    struct addrinfo hints, *res, *next;
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = 0;
    size_t srcip_len = strlen(HOST_FLOW_IP_PREFIX) + strlen(this_host) + 1;
    char *srcip = (char *) malloc(srcip_len);
    memset(srcip, 0, srcip_len);
    memcpy(srcip, HOST_FLOW_IP_PREFIX, strlen(HOST_FLOW_IP_PREFIX));
    memcpy(&srcip[strlen(HOST_FLOW_IP_PREFIX)], this_host, strlen(this_host));
    
    printf("cperf accepting at: %s : %s\n", srcip, HOST_DEMAND_PORT);

    if((rv = getaddrinfo("0.0.0.0", HOST_DEMAND_PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(next = res; next != NULL; next = next->ai_next) {
        if ((sockfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1) {
            perror("cperf: socket");
            continue;
        }
        int y = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &y, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, next->ai_addr, next->ai_addrlen) == -1) {
            close(sockfd);
            perror("cperf: bind");
            continue;
        }
        break;
    }
    if(next == NULL) {
        fprintf(stderr, "b cperf: failed to connect\n");
        exit(2);    
    }
    freeaddrinfo(res);
    
    //await connections
    if (listen(sockfd, NUM_CONNECTIONS) == -1) { //returns 0 if successful
        perror("listen");
        exit(1);
    }
    
    return sockfd;
}

//time in milliseconds since UNIX epoch
long get_time_msec() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long sec_in_msec = tp.tv_sec * 1000; //long version of milliseconds
    long usec_in_msec = tp.tv_usec / 1000; //num u.seconds in mseconds
    
    long m_sec = sec_in_msec + usec_in_msec; //milliseconds since UNIX epoch
    
    return m_sec;
}

void sleep_num_ms(long num_ms) {
    struct timespec t;
    int nsec_per_msec = 1000000; //1 million
    long tts_ns = num_ms * nsec_per_msec; //convert num_ms to ns
    t.tv_sec = 0;
    int tvnsint = num_ms * nsec_per_msec;
    printf("sleep milli sec: tv_nsec:%ld. - tv_nsec as int:%d.\n", tts_ns, tvnsint);
    t.tv_nsec = tts_ns;
    nanosleep(&t, NULL);
}

void get_IP(char *container, const char *base_IP, char *host_num) {
    memcpy(container, HOST_FLOW_IP_PREFIX, strlen(HOST_FLOW_IP_PREFIX));
    
    char *end = container;
    end += strlen(container);
    memcpy(end, host_num, strlen(host_num));
    
    end += strlen(host_num);
    *end = '\0';
}

void sleep_num_ns(long num_ns) {
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = num_ns;
    //printf("about to sleep ns. ");
    nanosleep(&t, NULL);
    //printf("wake up\n");
}

void transmit_flow(int vlan) {
    using namespace Tins;
    
    //get the first demand to send from the queue, send it with vlan at timeslot
    struct demand *current_demand = finalized_demands->front();
    
    char *src = this_host;
    char *dest = current_demand->dest;
    char *port = current_demand->port;
    
    //get IPs
    int buff_size = strlen(HOST_FLOW_IP_PREFIX) + strlen(src) + 2;
    char src_IP_buff[buff_size];
    char *src_IP = &src_IP_buff[0];
    memset(src_IP, 0, buff_size);
    
    get_IP(src_IP, HOST_IP_PREFIX, src);
    
    char dest_IP_buff[buff_size];
    char *dest_IP = &dest_IP_buff[0];
    memset(dest_IP, 0, buff_size);
    get_IP(dest_IP, HOST_IP_PREFIX, dest);
   
    uint16_t iport = (uint16_t) atoi(port); 
    printf("transmit: src=%s, dest=%s, srcIP=%s, destIP=%s, port=%d, vlan=%d\n", src, dest, src_IP, dest_IP, iport, vlan);

    //printf("vlan: %s, vlanint: %d, ts: %s", vlan_str, vlan, timeslot_str);
    EthernetII eth("77:22:33:ff:ff:ff");//macA, macB);
    Dot1Q *v_eth = new Dot1Q(vlan, true);
    
    
    IP *ip = new IP(dest_IP, src_IP);
    UDP *udp = new UDP(iport, iport);
    
    uint8_t p[packet_size];
    uint8_t *payload = &p[0];
    RawPDU *pld = new RawPDU(payload, packet_size); 

    udp->inner_pdu(pld);
    ip->inner_pdu(udp);
    v_eth->inner_pdu(ip);
    eth.inner_pdu(v_eth);
    
    PacketSender sender;
    
    //remove the demand and allocation just sent
    pthread_mutex_lock(&queue_mutex);
    finalized_demands->pop();
    pthread_mutex_unlock(&queue_mutex);

    int ifbuff_size = 30;
    char ifbuff[ifbuff_size];
    memset(ifbuff, 0, ifbuff_size);
    char *iface = &ifbuff[0];
    strcat(iface, "h");
    strcat(iface, this_host);
    strcat(iface, "-eth0");

    //printf("iface=%s\n", iface);
    //printf("transmission loop, %ld packets\n", current_demand->size/1000);
    for(int i = 0; i < current_demand->size/1000; i++) {
        sleep_num_ns(ind_packet_delay);
        sender.send(eth, iface);
    }
}

//ASSUMES ONLY ONE DEMAND ALLOCATION IN THE MESSAGE
//send the allocated flow to the appropriate host
void send_flow() {
    //parse allocation to obtain vlan and timeslot
    char *allocation = allocations->front();
    char *vlan_str = allocation;
    char *timeslot_str = strstr(allocation, ALLOCATION_SEPARATOR);
    *timeslot_str = '\0';
    timeslot_str++;
    char *end = strstr(timeslot_str, END_FLAG);
    *end = '\0';
    int vlan = atoi(vlan_str);
    long tslot = atol(timeslot_str);

    //time before the allocated timeslot in ms
    long tts = tslot - get_time_msec();
    printf("tmslt=%ld, currtm=%ld, until_tmlst=%ld\n", tslot, get_time_msec(), tts);
    sleep_num_ms(tts);
    //printf("wakeup=%ld\n", get_time_msec());
    
    long trans_start = get_time_msec();
    
    printf("starting transmission:%ld\n", trans_start);
    transmit_flow(vlan);
    printf("transmission complete, time taken=:%ld\n", get_time_msec() - trans_start);
    
    allocations->pop();
}

//receive allocated demands from the arbiter
void receive_allocation() {
	struct sockaddr_storage connector_addr; //connector's address
    socklen_t addrlen;
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    int newfd;        // newly accept()ed socket descriptor
    int yes=1;        // for setsockopt() SO_REUSEADDR
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    int listenerfd = get_recv_socket(); //socket descriptor for connections
    int i,j,numbytes;
	FD_SET(listenerfd, &master);
    fdmax = listenerfd; // so far, it's this one
    
    printf("this host:%s\n", this_host);
    while(!finalized_demands->empty()) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                    if (i == listenerfd) {
                    // handle new connections
                    printf("cperf: before accept\n");
                    addrlen = sizeof connector_addr;
                    newfd = accept(listenerfd,(struct sockaddr *)&connector_addr, &addrlen);

                    if (newfd == -1) {
                        perror("cperf: accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                    }
                }else {
                    //read allocated demands
        			size_t numbytes = -1;
        			int msg_size = tslot_l + vlan_l + strlen(END_FLAG) + 1;
					char alloc_buff[msg_size];
					char *allocation = &(alloc_buff[0]);
                    if ((numbytes = recv(i, allocation, msg_size, 0)) == -1) {
                        if (numbytes == 0) {
                            // connection closed
                            printf("cperf: selectserver: socket %d hung up\n", i);
                        } else {
                            perror("cperf: recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        if(numbytes != 0) {
							printf("reading in new allocation: %s\n", allocation);

							allocations->push(allocation);
        					send_flow();
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    }
}

void *receive_demands(void *ptr) {
    receive_allocation();
    return NULL;
}




//------------------------------------end receiving thread------------------------------------//
int main(int argc, char *argv[]) {
    char buff[3];
    this_host = &buff[0];
    memset(this_host, 0, 3);

    char begin = 'h';
    char last = '.';

    char *end = NULL;
    
    int i = 0;
    while(end == NULL) {
        end = strstr(argv[i], ".tr");
        i++;
    }

    while(*end != begin)
        end--;
       
    char *file_name = end;
    end++;
    while(*end != last) {
       *this_host = *end;
       this_host++;
       end++;
    }
   
    this_host = &buff[0];   
    
    
    finalized_demands = new queue<struct demand *>();
    split_demands = new vector<char *>();
    allocations = new queue<char *>();
    
    
    vector<char *> *unsplit = read_input(argv[1]);
    split_all_demands(unsplit);
    
    pthread_t send_thread, recv_thread;
    
    pthread_create(&send_thread, 0, initiate_send, 0);
    pthread_create(&recv_thread, 0, receive_demands, 0);
    
    pthread_join(send_thread, 0);
    pthread_join(recv_thread, 0);
    
}
//printf("here\n");
