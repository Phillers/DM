//
// Created by phillers on 03.05.18.
//

#ifndef DM_DM_H
#define DM_DM_H
#include <zmq.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <deque>
#include <map>
#include <mutex>
#include <condition_variable>

#define DM_ENTER 1
#define DM_AGREE 2
#define DM_LEAVE 3
#define DM_WAIT 4
#define DM_NOTIFY 5
#define DM_NOTIFY_ALL 6
#define DM_WAKE 7

class SharedMemory{
public:
    int size;
    virtual void serialize(char*)=0;
    virtual void deserialize(char*)=0;
};

class DM {
public:
    typedef int conditionVariable;
private:
    struct dm_message{
        char command;
        int id;
        int dataSize;
    };

    class SendBuffer{
    private:
        std::deque<dm_message> send_buffer;
        std::mutex mutex;
        std::condition_variable condVar;
        zmq::socket_t *publisher;
    public:
        void send(dm_message msg);
        void doSend(std::string port, DM* dm);
        ~SendBuffer();
    } sendBuffer;

    class Receiver {
    private:
        zmq::socket_t *subscriber;
        std::mutex mutex;
        std::condition_variable condVar;
        std::deque<int> enterQueue;
        std::map<int, int> permissions;
        int hosts=0;
        void agree(int id, SendBuffer* sendBuffer);
        int permissionCount=0;
        void resolve(DM* dm);
        int nextVar=0;
        int wakeCount=0;

    public:
        void doReceive(std::vector<std::string> addr, DM* dm);
        void waitForEnter(DM* dm);
        void waitForVariable(DM* dm);
        void addHost(std::string addr);
        std::vector<std::deque<int>> waitQueue;
        int waitingProcess(conditionVariable condVar);
        conditionVariable createNewConditionVariable();
        ~Receiver();
    }receiver;

public:

    void addHost(std::string addr);

    void enter();
    void leave();
    conditionVariable createNewConditionVariable();
    void wait(conditionVariable condVar);
    void wait();
    void notify();
    void notify(conditionVariable condVar);
    void notify_all();
    void notify_all(conditionVariable condVar);


    DM(int id, SharedMemory* sharedMemory, int port, std::vector<std::string> addr);

    ~DM();
private:
    void startSender(std::string port);
    void listen(std::vector<std::string> addr);
    int inMonitor;
    std::thread* listener,*sender;
    conditionVariable waitingFor;

    int id;



    SharedMemory* sharedMemory;

};



#endif //DM_DM_H
