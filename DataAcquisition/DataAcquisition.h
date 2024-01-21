#ifndef DATAACQUISITION_H_
#define DATAACQUISITION_H_

#include "SeismicData.h"
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <queue>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/ip.h>     
#include <arpa/inet.h>  
#include <pthread.h>
#include <map>
#include <set>
#include <algorithm>

static void signalHandler(int signum);

class DataAcquisition {

    struct DataPacket {
        uint8_t packetNo;
        uint16_t packetLen;
        std::string data;
    };

    struct Subscriber {
        char username[BUF2_LEN];
        char IP_addr[INET_ADDRSTRLEN];
        int port;
    };


    bool is_running;
    pthread_mutex_t lock_x;
    std::queue<DataPacket> packet;
    std::map<std::string, Subscriber> subscribers; 
    std::map<std::string, int> list1;
    std::map<std::string, Subscriber> list2; 
    sem_t *sem_id1;
    key_t  ShmKey;
    int    ShmID, sv_sock;
    struct SeismicMemory *ShmPTR;
    pthread_t read_tid, write_tid;

public:
    static DataAcquisition* data;
    DataAcquisition();
    void authenticate(char cl_msg[BUF_LEN], struct sockaddr_in *cl_addr, int sv_sock);
    void setSharedMemory();
    void setSignalHandler();
    void socketSetup();
    void createThreads();
    void readMemory();
    static void* recv_func(void *arg);
    static void* send_func(void *arg);
    void shutdown();
};


#endif
