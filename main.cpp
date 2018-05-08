#include <iostream>
#include <unistd.h>
#include"DM.h"
using namespace std;
class SharedBuffer : public SharedMemory{
public:
    char* buffer;
    void serialize(char* share){
        memcpy(share, buffer, 10);
    }
    void deserialize(char* share){
        memcpy(buffer, share, 10);
    }
    int take(){
        int res=buffer[3+buffer[1]++];
        buffer[0]--;
        buffer[1]%=7;
        return res;
    }
    void put(int val){
        buffer[3+buffer[2]++]=val;
        buffer[0]++;
        buffer[2]%=7;
    };

    int count(){
        return buffer[0];
    }

    int place(){
        return 7-buffer[0];
    }
};


void konsument(DM* dm, SharedBuffer* sharedBuffer ){
    DM::conditionVariable somethingIn = dm->createNewConditionVariable();
    DM::conditionVariable placeForNew = dm->createNewConditionVariable();
    int val=0;
    while(1){
        sleep(1);
        dm->enter();
        while(sharedBuffer->count()==0)dm->wait(somethingIn);
        val=sharedBuffer->take();
        std::cout<<"Took value "<<val<<std::endl;
        dm->notify(placeForNew);
        dm->leave();
    }


}


void producent(DM* dm, SharedBuffer* sharedBuffer){

    DM::conditionVariable somethingIn = dm->createNewConditionVariable();
    DM::conditionVariable placeForNew = dm->createNewConditionVariable();
    int val=0;
    while(1){
        sleep(3);
        dm->enter();
        while(sharedBuffer->place()==0)dm->wait(placeForNew);
        sharedBuffer->put(val);
        std::cout<<"Put value "<<val++<<std::endl;
        dm->notify(somethingIn);
        dm->leave();
    }


}

int main(int argc, char* argv[]) {

    cout << "Hello, World!" << endl;

    int port;
    if(argc<2)port=1234;
    else port=stoi(argv[1]);
    SharedBuffer sharedBuffer;
    sharedBuffer.buffer = new char[10]{0,0,0,4,5,6,7,8,9,10};
    sharedBuffer.size=10;
    std::vector<std::string> addr={"tcp://localhost:1234","tcp://localhost:4321","tcp://localhost:2222"};
    DM dm(port, &sharedBuffer, port, addr );

    std::string x;
    char c;

    while(cin>>c){
        switch(c){
            case 'a':
                cin>>x;
                dm.addHost(x);
                break;
            case 'e':
                dm.enter();
                for(int i=0;i<10;i++)sharedBuffer.buffer[i]++;
                break;
            case 'l':
                dm.leave();
                break;

            case 'c':
                for(int i=0;i<10;i++)printf("%d ", sharedBuffer.buffer[i]);
            std::cout<<endl;
                    break;
            case 'w':
                dm.wait();
                break;
            case 'n':
                dm.notify();
                break;
            case 'b':
                dm.notify_all();
                break;

            case 'p':
                producent(&dm, &sharedBuffer);

            case 'k':
                konsument(&dm, &sharedBuffer);

            case 's':

            default:
                cin>>x;

        }


    }

    return 0;
}