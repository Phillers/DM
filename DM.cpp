//
// Created by phillers on 03.05.18.
//


#include <unistd.h>
#include "DM.h"

void DM::startSender(std::string port) {
    sender = new std::thread(&DM::SendBuffer::doSend, &sendBuffer, port, this);

}

void DM::listen(std::vector<std::string> addr) {
    listener = new std::thread(&DM::Receiver::doReceive, &receiver, addr, this);
}

void DM::SendBuffer::doSend(std::string port, DM* dm) {


    zmq::context_t context(1);
    publisher = new zmq::socket_t(context, ZMQ_PUB);
    std::string str("tcp://*:");
    str += port;
    publisher->bind(str);

    while (1) {
        std::unique_lock<std::mutex> lock(mutex);
        while (send_buffer.empty())condVar.wait(lock);
        dm_message msg = send_buffer.front();
        send_buffer.pop_front();
        lock.unlock();
        char* data;
        data=(char*)malloc(sizeof(msg)+msg.dataSize);
        memcpy(data, &msg, sizeof(msg));
        if(msg.command==DM_LEAVE){
            dm->sharedMemory->serialize(data+sizeof(msg));
        } else if(msg.command==DM_WAIT){
            dm->sharedMemory->serialize(data+sizeof(msg));
            msg.dataSize-=dm->waitingFor;
        } else if(msg.command==DM_NOTIFY){
            msg.dataSize-=dm->waitingFor;
        }

        zmq::message_t message(data, sizeof(msg)+msg.dataSize);

        publisher->send(message);
        free(data);
    }
}

void DM::Receiver::doReceive(std::vector<std::string> addr, DM* dm) {


    zmq::context_t context(1);

    subscriber = new zmq::socket_t(context, ZMQ_SUB);
    for (std::string a : addr) {
        subscriber->connect(a);
        hosts++;
    }
    const char *filter = "";
    subscriber->setsockopt(ZMQ_SUBSCRIBE, filter, strlen(filter));

    while (1) {

        zmq::message_t update;

        subscriber->recv(&update);
        dm_message *msg = (dm_message *) malloc(update.size());
        memcpy(msg, update.data(), update.size());
        switch (msg->command) {
            case DM_ENTER:
                enterQueue.push_back(msg->id);
                if (enterQueue.front() == msg->id)agree(msg->id, &dm->sendBuffer);
                break;

            case DM_AGREE:
                if (permissions.count(msg->id) == 0) {
                    permissions.emplace(msg->id, 1);
                } else {
                    permissions[msg->id]++;
                }
                permissionCount++;
                if (permissionCount == hosts)resolve(dm);
                break;

            case DM_LEAVE:
                if (enterQueue.front() == msg->id) {
                    enterQueue.pop_front();
                    if(!enterQueue.empty()){
                        agree(enterQueue.front(), &dm->sendBuffer);
                    };
                } else {
                    enterQueue.erase(std::remove(enterQueue.begin(), enterQueue.end(), msg->id), enterQueue.end());
                }
                dm->inMonitor=-1;
                std::cout << msg->id << " left monitor.\n";
                dm->sharedMemory->deserialize((char*)msg+sizeof(*msg));
                break;

            case DM_WAIT: {
                if (enterQueue.front() == msg->id) {
                    enterQueue.pop_front();
                    if (!enterQueue.empty()) {
                        agree(enterQueue.front(), &dm->sendBuffer);
                    };
                } else {
                    enterQueue.erase(std::remove(enterQueue.begin(), enterQueue.end(), msg->id), enterQueue.end());
                }

                dm->inMonitor = -1;
                conditionVariable c = msg->dataSize - dm->sharedMemory->size;
                std::cout << msg->id << " waiting for " << c << std::endl;
                while (waitQueue.size()<=c)waitQueue.push_back(std::deque<int>());
                waitQueue[c].push_back(msg->id);
                dm->sharedMemory->deserialize((char *) msg + sizeof(*msg));
                break;
            }
            case DM_NOTIFY_ALL:
                waitQueue[msg->id].clear();
                if(dm->waitingFor==msg->id){
                    dm->waitingFor=-1;
                    condVar.notify_one();
                }
                break;

            case DM_NOTIFY:
                enterQueue.push_back(msg->id);
                waitQueue[msg->dataSize].erase(std::remove(waitQueue[msg->dataSize].begin(), waitQueue[msg->dataSize].end(), msg->id), waitQueue[msg->dataSize].end());
                if(msg->id ==dm->id){
                    dm->waitingFor=-1;
                    condVar.notify_one();
                }
                std::cout << msg->id << " was notified.\n";
                break;


        }


       // printf("received %d %d %d\n", msg->command, msg->id, msg->dataSize);
        free(msg);
    }


}

DM::~DM() {
    delete listener, sender;


}

void DM::SendBuffer::send(dm_message msg) {
    mutex.lock();
    send_buffer.push_back(msg);
    condVar.notify_one();
    mutex.unlock();
}

void DM::addHost(std::string addr) {
    receiver.addHost(addr);
}

void DM::Receiver::addHost(std::string addr) {
    subscriber->connect(addr);
    hosts++;
}

DM::DM(int id,SharedMemory* sharedMemory,int port, std::vector<std::string> addr) {
    this->id = id;
    this->sharedMemory=sharedMemory;
    startSender(std::to_string(port));
    listen(addr);
    std::deque<int> q;
    receiver.createNewConditionVariable();
}

void DM::enter() {
    if(inMonitor!=id) {
        dm_message msg;
        msg.id = id;
        msg.command = DM_ENTER;
        msg.dataSize = 0;
        sendBuffer.send(msg);
        receiver.waitForEnter(this);
    }
}

void DM::Receiver::agree(int id, SendBuffer* sendBuffer) {
    dm_message msg;
    msg.id = id;
    msg.command = DM_AGREE;
    msg.dataSize = 0;
    sendBuffer->send(msg);
}

void DM::Receiver::resolve(DM* dm) {
    int maxValue = 0, maxKey = 0;
    for (auto x : enterQueue) {
        if (permissions[x] > maxValue) {
            maxValue = permissions[x];
            maxKey = x;
        }
    }

    permissionCount -= permissions[maxKey];
    permissions[maxKey] = 0;
    dm->inMonitor = maxKey;
    if(dm->inMonitor==dm->id)condVar.notify_one();
    std::cout << maxKey << " entered monitor.\n";
}

void DM::leave() {
    if (inMonitor == id) {
        dm_message msg;
        msg.id = id;
        msg.command = DM_LEAVE;
        msg.dataSize = sharedMemory->size;
        inMonitor=-1;
        sendBuffer.send(msg);

    }
}

DM::SendBuffer::~SendBuffer() {
    delete publisher;
}

DM::conditionVariable DM::createNewConditionVariable() {
    return receiver.createNewConditionVariable();
}

DM::conditionVariable DM::Receiver::createNewConditionVariable() {
    if(nextVar>=waitQueue.size())waitQueue.push_back(std::deque<int>());
    return nextVar++;
}

void DM::wait(DM::conditionVariable condVar) {
    if (inMonitor == id) {
        dm_message msg;
        msg.id = id;
        msg.command = DM_WAIT;
        msg.dataSize = sharedMemory->size+condVar;
        waitingFor=condVar;
        sendBuffer.send(msg);
        receiver.waitForVariable(this);
    }
}

void DM::wait() {
    wait(0);
}

void DM::notify() {
    notify(0);
}

void DM::notify(DM::conditionVariable condVar) {
    if (inMonitor == id) {
        dm_message msg;
        msg.id = receiver.waitingProcess(condVar);
        msg.command = DM_NOTIFY;
        msg.dataSize = condVar;
        if(msg.id>=0)
        sendBuffer.send(msg);
    }
}

void DM::notify_all() {
    notify_all(0);
}

void DM::notify_all(DM::conditionVariable condVar) {
    if (inMonitor == id) {
        dm_message msg;
        msg.id = condVar;
        msg.command = DM_NOTIFY_ALL;
        msg.dataSize = 0;
        sendBuffer.send(msg);
    }
}

DM::Receiver::~Receiver() {
    delete subscriber;
}

void DM::Receiver::waitForEnter(DM *dm){
    std::unique_lock<std::mutex> lock(mutex);
    while(dm->inMonitor!=dm->id) condVar.wait(lock);
}

void DM::Receiver::waitForVariable(DM *dm){
    std::unique_lock<std::mutex> lock(mutex);
    while(dm->waitingFor>0) condVar.wait(lock);
    while(dm->inMonitor!=dm->id) condVar.wait(lock);
}



int DM::Receiver::waitingProcess(DM::conditionVariable condVar) {
    return waitQueue[condVar].empty()? -1 : waitQueue[condVar].front();
}
