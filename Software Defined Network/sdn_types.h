#define ARBITER_PORT "5000"
#define HOST_DEMAND_PORT "2345"
#define HOST_TRAFFIC_PORT "3456"

#define ARBITER_IP "20.0.0.100"
//"127.0.0.1"
//"20.0.0.100"

//arbiter->host connection
#define HOST_IP_PREFIX "20.0.0."

//host->host connection
#define HOST_FLOW_IP_PREFIX "10.0.0."

//number of hosts in network
#define NUM_HOSTS 16

//size of queue for connections to arbiter
#define NUM_CONNECTIONS 300

//max possible length of a line in hX.tr
#define LINE_LEN 30

//number of demands to send to the arbiter in one message
#define NUM_DEMANDS 300

//flag between aggregated demands 
#define END_FLAG "{e}"

//in ms
#define TIMESLOT_LENGTH 10

//in ms
#define ALLOCATION_INTERVAL 50

#define MIN_FUTURE_TIMESLOT 100

//separator between vlan and timeslot in demand allocation message
#define ALLOCATION_SEPARATOR ","

//number of core switches in the network
#define NUM_CORE 4

#define false 0
#define true 1

#define src_l 3
#define dest_l 3
#define port_l 6
#define vlan_l 5
#define tslot_l 30


struct demand {
    char src[src_l]; //01 - 16
    char dest[dest_l]; //01 - 16
    char port[port_l]; //max 2^16
    long size;
    char vlan[vlan_l]; //max 1087: 16<<4 + 15<<2 + 3
    char tslot[tslot_l]; //max 1087:?
};
