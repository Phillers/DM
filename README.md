# DM
Rozproszony monitor w c++ oparty o wiadomości ZeroMQ.
W celu użycia należy dołączć do programu plik DM.h i do kompilacji DM.cpp. 
Wymagane też dolinkowanie zeroMQ (-lzmq) oraz pthread (-pthread), obecność nagłówka zeroMQ dla c++ (zmq.hpp) 
oraz wykorzystanie standardu C++11 (-std=c++11). (przykład w Makefile)

Główną częśćią monitora jest klasa DM, o następujących metodach publicznych:

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
    
 W celu utworzenia monitora, należy wywołać jego konstruktor o następujących parametrach:
 - int id - identyfikator węzła, w każdym monitorze z systemu powinien być inny
 - SharedMemory* sharedMemory - wskaźnik na pamięć współdzieloną, dziedziczącą po następującej klasie: 

       class SharedMemory{
        public:
        int size;//liczba bajtów, które mają być współdzielone, 
        virtual void serialize(char*)=0;//metoda, która zapisuje size bajtów w adresie wskazanym przez parametr,
        virtual void deserialize(char*)=0;//metoda oczytuje size bajtów z wskazanego adresu, uaktualniając lokalny stan
    };

- int port - port pod którym dany węzeł będzie wysyłać wiadomości (będzie on z nim powiązany funkcją bind)
- std::vector<std::string> addr - lista adresów pod którymi będą wszystkie węzły systemu (łącznie z własnym), w formacie dla zmq: 
  tcp://adres:port

Aby zdefiniować metodę lub blok kodu, który ma zostać wykonany w monitorze należy rozpocząć za pomocą DM::enter(),
oznaczającej wejście do monitora, a zakończyć DM::leave czyli wyjściem. 
Aby użyć domyślnej zmiennej warunkowej służą metody DM::wait(), DM::notify() i DM::notify_all(). 
Aby użyć więcej zmiennych warunkowych należy je utworzyć za pomocą metody DM::createNewConditionVariable(), zwracającej 
typ DM::conditionVariable, który można przyekazywać jako parametr powyższych metod (np void wait(conditionVariable condVar);)
WAŻNE: W każdym węźle zmienne warunkowe muszą być tworzone w tej samej kolejności.

Przykład dla procesu producenta:

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
        
    }}
    
    w pliku main.cpp znajduje się rozwiązanie prostego problemu producenta-konsumenta. 
    Należy go uruchomić w kilku kopiach podając jako parametr numer portu będący jednocześnie identyfikatorem 
    (w obecnej postaci należy uruchomić 3 kopie z id 1234, 4321 oraz 2222), 
    a następnie kliknąć w nich k aby uruchomić konsumenta lub p dla producenta.
