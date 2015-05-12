#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <queue>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include "sdn_types.h"

using namespace std;

//-------------------------------------------------------------------//

//last host whose demands were allocated/served
int last_queue_served = -1;

//set of queues to hold incoming demands
vector< queue<struct demand *> *> *demand_buckets;
    
//demand bucket mutex
pthread_mutex_t bucket_mutex = PTHREAD_MUTEX_INITIALIZER;

long last_timeslot = 0;

long original_timeslot = 0; //debugging

int link_size = 80;
int links[80];

//-------------------------------------------------------------------//
    



//need NUM_HOSTS queues for the demands of each host
vector< queue<struct demand *> *> *initialize_buckets() {
    
    demand_buckets = new vector< queue<struct demand *> *>();
        
    for(int i = 0; i < NUM_HOSTS; i++) {
        queue<struct demand *> *bucket = new queue<struct demand *>();
        demand_buckets->push_back(bucket);
    }
    
    return demand_buckets;
}

//get pointer to queue, given index (0-indexed) and vector of queue pointers
queue<struct demand *> *get_queue(int index) {
    return (*demand_buckets)[index];
}

void print_buckets() {
    for(int i = 0; i < demand_buckets->size(); i++) {
        queue<struct demand *> *queue = get_queue(i);
        int size = queue->size();
        printf("queue %d size: %d", i, size);
        if(size > 0) 
            printf(", front port: %s\n", (queue->front())->port);
        else
            printf("\n");
    }
}



//------------------------------------sending thread------------------------------------//


//tor switch the host is attached to
int get_tor_by_host(int host) {
    if(host % 2 != 0) {
        host++;
    }
    return host / 2;
}

//debugging: find core link based on set bit in 'links' bit field
/*
int get_bit_pos(unsigned int field) {
    unsigned int i = 1;
    int pos = 1;
    
    while (!(i & n))
    {
        i = i << 1;
        ++pos;
    }   
    return pos;
}
*/

//pod number the host is located in
int get_pod_num(int host) {
    int pod = 1;
    while(host > 4) {
        host -= 4;
        pod++;
    }
    return pod;
}

//get the first of four vlans for a given src/dest pair
int get_vlan(struct demand *next_demand) {
	int src = atoi(next_demand->src);
    int dst = atoi(next_demand->dest);
	int num_vlans = 4;
	int possible_vlans[num_vlans];
	int vlan = 0;
	if(src > dst)
        vlan = vlan | (src << 6) | (dst << 2);
    else
        vlan = vlan | (dst << 6) | (src << 2);
	/*
	printf("possible vlans:");
	for(int i = 0; i < num_vlans; i++) {
		possible_vlans[i] = vlan + i;
		printf("%d - ", vlan+i);
	}
	vector<int> *vlan_links;
	//get links in each vlan
	for(int i = 0; i < num_vlans; i++) { //e.g. i=0 - go through first core switch
		vlan_links = new vector<int>;
		int src_tor = get_tor_by_host(src);
		int src_pod = get_pod_num(src);
		int dst_tor = get_tor_by_host(dst);
		int dst_pod = get_pod_num(dst);
		
		//src tor-agg link
		int src_agg_link;
		if(src_tor % 2 != 0) { //leftmost tor switch in pod
			if(i < 2) { //goes to first two core switches, base_vlan = 0/1
				src_agg_link = 1 + ((src_pod - 1) * 4); //first tor-agg link
			}
			else {
				src_agg_link = 2 + ((src_pod - 1) * 4); //second tor-agg link
			}
		}
		else { //rightmost tor switch in pod
			if(i < 2) { //goes to first two core switches, base_vlan = 0/1
				src_agg_link = 3 + ((src_pod - 1) * 4); //third tor-agg link
			}
			else {
				src_agg_link = 4 + ((src_pod - 1) * 4); //second tor-agg link
			}	
		}
		vlan_links->push_back(src_agg_link);
		
		//src agg-core link
		int core_link_start;
		int src_core_link;
		if(i < 2) {
			core_link_start = 17;
		}
		else {
			core_link_start = 21;
		}
		src_core_link = core_link_start + ((src_pod - 1) * 8) + i;
		vlan_links->push_back(src_core_link);
		
		//dst tor-agg link
		int dst_agg_link;
		if(dst_tor % 2 != 0) { //leftmost tor switch in pod
			if(i < 2) { //goes to first two core switches, base_vlan = 0/1
				dst_agg_link = 1 + ((dst_pod - 1) * 4); //first tor-agg link
			}
			else {
				dst_agg_link = 2 + ((dst_pod - 1) * 4); //second tor-agg link
			}
		}
		else { //rightmost tor switch in pod
			if(i < 2) { //goes to first two core switches, base_vlan = 0/1
				dst_agg_link = 3 + ((dst_pod - 1) * 4); //third tor-agg link
			}
			else {
				dst_agg_link = 4 + ((dst_pod - 1) * 4); //second tor-agg link
			}	
		}
		vlan_links->push_back(dst_agg_link);
		
		//dst agg-core link
		int dst_core_link;
		if(i < 2) {
			core_link_start = 17;
		}
		else {
			core_link_start = 21;
		}
		dst_core_link = core_link_start + ((dst_pod - 1) * 8) + i;
		vlan_links->push_back(dst_core_link);
		
		//if all links in this most recent vlan are available, use them
		int all_available = true;
        printf("checking links for vlan:%d.", i);
		for(int j = 0; j < vlan_links->size(); j++) {
			int index = (*vlan_links)[j];
            if(links[index - 1] != 0) {
				all_available = false;
				break;
                printf("no links available for vlan:%d.", i);
			}
		}
		if(all_available) {
			vlan += i;
			break;
		}
	}
	
	if(vlan) {
    	printf("vlan alloc:%d\n", vlan);
	}
    */
    return vlan;
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

/*
 * allocate a timeslot TIMESLOT_LENGTH ms after the previously allocated timeslot,
 * or TIMESLOT_LENGTH ms into the future if this scheme results in a time in the past
 */
long get_next_timeslot(long previous_timeslot) {
    if (last_timeslot < get_time_msec()){
        last_timeslot = get_time_msec();
    }
    long new_timeslot = last_timeslot + TIMESLOT_LENGTH;
    last_timeslot = new_timeslot;
    return new_timeslot;
}


int attempt_allocation(struct demand *next_demand) {
    //timeslot selection
    if(last_timeslot == 0)
        last_timeslot = get_time_msec();
    
    long timeslot = get_next_timeslot(last_timeslot);
    long currtime = get_time_msec();
    
    //****path selection: find open vlan
    int vlan = get_vlan(next_demand);
    if(vlan <= 0) {
        return false;
    }
    
    printf("alloc: tmslt=%ld, now=%ld, ts ahead=%ld ms, vlan=%d\n", timeslot, currtime, timeslot - currtime, vlan);
    
    //add vlan, timeslot to demand struct
    sprintf(next_demand->vlan, "%d", vlan);
    sprintf(next_demand->tslot, "%ld", timeslot);
    
    return true;
}

//mark all links in the network as unused during the current demand allocation
void reset_links() {
    for(int i = 0; i < link_size; i++) {
        links[i] = 0;
    }
}
//next queue to service in round robin
int get_next_queue_index(int current_queue) {
    if(current_queue == (NUM_HOSTS - 1))
        current_queue = 0;
    else
        current_queue++;
    
    return current_queue;
}

//MODIFY FOR PERFORMANCE
//project B: just get the first demand out of each queue for sending
//start of sending thread; calls send_demands()
queue<struct demand *> *allocate_demands() {
    //queue of allocated demands to send back to hosts
    queue<struct demand *> *allocated_demands = new queue<struct demand *>();

    //number of core switches allocated in the current allocation of demands
    int num_core_allocated = 0;
    
    int current_queue_index = get_next_queue_index(last_queue_served); //round robin allocation of demands
    
    //printf("allocate, cqi=%d, lqs=%d\n", current_queue_index, last_queue_served);
    
    //go through each queue once and allocate it a path
    for(int i = 0; i < NUM_HOSTS; i++) {
        pthread_mutex_lock(&bucket_mutex);
        //next queue in round robin
        queue<struct demand *> *queue = get_queue(current_queue_index);
            
        //first demand in queue
        if(queue->size() > 0) {
            struct demand *next_demand = queue->front();
            printf("allocating demand: src=%s, dst=%s size=%ld\n", next_demand->src, next_demand->dest, next_demand->size);
            if(attempt_allocation(next_demand)) {
                
                queue->pop();
                
                allocated_demands->push(next_demand);
            }
        }
        
        pthread_mutex_unlock(&bucket_mutex);
        last_queue_served = current_queue_index;
        current_queue_index = get_next_queue_index(current_queue_index);
    }
    //printf("\n");
    return allocated_demands;
}

//setup addrinfo struct for TCP connection to arbiter to send demands
int connect_host(char *host_num) {
    //get IP address of host by adding host_num to HOST_IP_PREFIX
    char host_IP[strlen(HOST_IP_PREFIX) + 2];
    memset(&host_IP, 0, sizeof host_IP);
    memcpy(&host_IP, HOST_IP_PREFIX, strlen(HOST_IP_PREFIX));
    
    char *end = host_IP;
    end += strlen(host_IP);
    memcpy(end, host_num, strlen(host_num));
    
    int sockfd = -1;
    struct addrinfo hints, *res, *next;
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    //printf("connect to: %s\n", &host_IP[0]);

    int rv = 0;
    if((rv = getaddrinfo(host_IP, HOST_DEMAND_PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(next = res; next != NULL; next = next->ai_next) {
        if ((sockfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1) {
            perror("arbiter: socket");
            continue;
        }
        if (connect(sockfd, next->ai_addr, next->ai_addrlen) == -1) {
            close(sockfd);
            perror("arbiter: connect");
            continue;
        }
        break;
    }
    
    if(next == NULL) {
        fprintf(stderr, "arbiter: failed to connect to host (send demands)\n");
        exit(2);    
    }
    
    return sockfd;
}

void send_allocations(struct demand *demand) {
    
    //irintf("to send: host=%s, dest=%s, size=%d, vlan=%s, tslot=%s\n", demand->src, demand->dest, demand->size, demand->vlan, demand->tslot);
    
    //buffer
    int msg_size = sizeof demand->tslot + sizeof demand->vlan + strlen(END_FLAG) + 1;
    char msg_buff[msg_size];
    memset(&msg_buff, 0, msg_size);
    char *msg = &(msg_buff[0]);
    
    //copy in vlan
    char *crsr_pos = msg;
    int vlan_len = strlen(demand->vlan);
    memcpy(crsr_pos, demand->vlan, vlan_len);
    crsr_pos += vlan_len;
    
    //separator
    *crsr_pos = ',';
    crsr_pos++;
    
    //copy in timeslot
    int tslot_len = strlen(demand->tslot);
    memcpy(crsr_pos, demand->tslot, tslot_len);
    crsr_pos += tslot_len;
   
    int ef_len = strlen(END_FLAG); 
    memcpy(crsr_pos, END_FLAG, ef_len);
    crsr_pos += ef_len;
    *crsr_pos = '\0';
    
    int sockfd = connect_host(demand->src);
    if(write(sockfd, msg, strlen(msg)) == -1)
        perror("write");
    
    printf("sent to host: %s\n", msg);
    close(sockfd);
    
}
   
//MODIFY FOR PERFORMANCE
//project B: new connection for each sent demand
void *send_demands(void *ptr) {
    while(1) {
        //only allocate demands every ALLOCATION_INTERVAL mseconds
        long time_before_sleep = get_time_msec();
        struct timespec t;
        int nsec_per_msec = 1000000; //1 million
        int tslot_len_nsec = TIMESLOT_LENGTH * nsec_per_msec; //e.g.: ts=100ms, tsn=100 million ns
        t.tv_sec = 0;
        t.tv_nsec = tslot_len_nsec * ((double) ALLOCATION_INTERVAL / TIMESLOT_LENGTH);
        nanosleep(&t, NULL);
        //printf("awake. slept: %ld\n", get_time_msec() - time_before_sleep);
        
        queue<struct demand *> *allocated_demands = allocate_demands();
        
        //for(int i = 0; i < allocated_demands->size(); i++) {
        while(allocated_demands->size() != 0) {
            printf("in while to send allocs: num in q = %lu\n", allocated_demands->size());
            struct demand *current = allocated_demands->front();
            //int senderfd = connect_host(cuirrent->src);
            send_allocations(current);
            allocated_demands->pop();
			free(current);			
            
            //printf("allocation sent, num in q still = %lu\n", allocated_demands->size());
        }
            
        free(allocated_demands);
    }
    
    return NULL;
}



//------------------------------------end sending thread------------------------------------//


//------------------------------------receiving thread------------------------------------//



//parse a demand, then add it to the appropriate queue - called by queue_demands()
void add_to_queue(char *src, char *demand_str, int bucket_num) { //queue<struct demand *> *queue) {
    int flag_size = 3;
    
    queue<struct demand *> *queue = get_queue(bucket_num);
    
    //allocate struct and initialize
    struct demand *demand = (struct demand *) malloc(sizeof (struct demand)); //DONT FORGET TO FREE A DEMAND WHEN POPPING OUT OF QUEUE
    memset(demand->src, 0, sizeof demand->src);
    memset(demand->dest, 0, sizeof demand->dest);
    memset(demand->port, 0, sizeof demand->port);
    memset(demand->vlan, 0, sizeof demand->vlan); //need for later - vlan allocated for each demand
    memset(demand->tslot, 0, sizeof demand->tslot); //need for later - timeslot allocated for each demand
    
    //parse demand to fill out struct
    char *dest_loc = strstr(demand_str, "-d") + flag_size + 1; //1 extra to get rid of 'h'
    char *port_loc = strstr(demand_str, "-p") + flag_size;
    char *size_loc = strstr(demand_str, "-n") + flag_size;
    int dest_len = port_loc - dest_loc - flag_size - 1;
    int port_len = size_loc - port_loc - flag_size - 1;
    
    memcpy(demand->src, src, strlen(src));
    memcpy(demand->dest, dest_loc, dest_len);
    memcpy(demand->port, port_loc, port_len);
    demand->size = atoi(size_loc);
    
    printf("queueing demand: src=%s, dst=%s size=%ld bucket=%d\n", demand->src, demand->dest, demand->size, bucket_num);
    
    pthread_mutex_lock(&bucket_mutex);
    queue->push(demand);
    size_t qsize = queue->size();
    pthread_mutex_unlock(&bucket_mutex);
}

//add the demands to a queue according to the demand source
void queue_demands(const char* src, char* demand) {
    int src_len = strlen(src);
    char buff[src_len];
    char *src_str = &(buff[0]);
    memcpy(buff, src, src_len);
    char *end = src_str + src_len;
    *end = '\0';
    
    //loop through src string to get host num for appropriate bucket
    char separator = '.';
    int host_loc = 3; //three dot separators before host num in IP
    char *pos = strchr(src_str, separator);
    for(int f = 0; f < host_loc - 1; f++) {
        pos = strchr(pos+1, separator);
    }
    char *src_num = pos+1; //host num from IP whole address
    
    int bucket_num = atoi(pos+1);
    
    //there may be multiple demands in a message - if so, split them into individual demands
    //note to self: pass src to 'add to queue' to put in struct
    char *end_demand = demand; //&(demand[strlen(demand)-1]); 
    end_demand += strlen(demand) - 1;
    
    while(demand < end_demand) {
        char *next_demand = strstr(demand, END_FLAG);
        *next_demand = '\0'; //now demand is null-terminated
        next_demand += sizeof END_FLAG - 1;
        add_to_queue(src_num, demand, bucket_num - 1);
        demand = next_demand;
    }
    //printf("\n");
}

int get_recv_socket() {
    int sockfd = -1;
    struct addrinfo hints, *res, *next;
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int rv = 0;
    if((rv = getaddrinfo("0.0.0.0", ARBITER_PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(next = res; next != NULL; next = next->ai_next) {
        if ((sockfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1) {
            perror("arbiter: socket");
            continue;
        }
        int y = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, next->ai_addr, next->ai_addrlen) == -1) {
            close(sockfd);
            perror("arbiter: bind");
            continue;
        }
        break;
    }
    if(next == NULL) {
        fprintf(stderr, "arbiter: failed to connect\n");
        exit(2);    
    }
    freeaddrinfo(res);
    
    //await connections
    if (listen(sockfd, 500) == -1) { //returns 0 if successful
        perror("listen");
        exit(1);
    }
    
    return sockfd;
}

//receive messages containing demands from hosts, put them in the corresponding host's queue
void *receive_demands(void *ptr) {
    struct sockaddr_storage connector_addr; //connector's address
    socklen_t addrlen;
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    int newfd;        // newly accept()ed socket descriptor
    int yes=1;        // for setsockopt() SO_REUSEADDR
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    char pres_addr[INET_ADDRSTRLEN];
    int i,j,numbytes;
    char *src;
    
    int listenerfd = get_recv_socket(); //socket descriptor for connections
   
    FD_SET(listenerfd, &master);
    fdmax = listenerfd; // so far, it's this one

    while(1) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                    if (i == listenerfd) {
                    // handle new connections
                    printf("arbiter: before accept\n");
                    addrlen = sizeof connector_addr;
                    newfd = accept(listenerfd,(struct sockaddr *)&connector_addr, &addrlen);

                    if (newfd == -1) {
                        perror("arbiter: accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                    }
                }else {
                    char demand[LINE_LEN * NUM_DEMANDS];
                    memset(&demand, 0, sizeof demand);
                    if ((numbytes = recv(i, &demand, sizeof demand, 0)) == -1) {
                        if (numbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("arbiter: recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        if(numbytes != 0) {
							struct sockaddr_in addr;
							socklen_t addr_size = sizeof(struct sockaddr_in);
							int res = getpeername(i, (struct sockaddr *)&addr, &addr_size);

							char *clientip = new char[20];
							strcpy(clientip, inet_ntoa(addr.sin_addr));

							printf("abiter: read in: %s\n", &demand[0]);

							queue_demands(clientip, &demand[0]);
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    }
}

void free_buckets(vector< queue<struct demand *> *> *demand_buckets) {
    for(int i = 0; i < demand_buckets->size(); i++) {
        free((*demand_buckets)[i]);
    }
    free(demand_buckets);
}



//------------------------------------end receiving thread------------------------------------//



void create_threads() {
    pthread_t recv_thread, send_thread;
    
    pthread_create(&recv_thread, 0, receive_demands, 0);
    pthread_create(&send_thread, 0, send_demands, 0);
    
    pthread_join(recv_thread, 0);
    pthread_join(send_thread, 0);
}

int main() {
    demand_buckets = initialize_buckets();
    
    original_timeslot = get_time_msec();
    printf("program begins: %ld\n", original_timeslot);
    
    create_threads();
    //queue<struct demand *> *allocated_demands = allocate_demands(demand_buckets);
    //print_buckets(demand_buckets);
    
    //allocate demands
    //send timeslots back
    
    //FREE ALL ALLOC'ED STRUCTS
    free_buckets(demand_buckets);
}
