# DM

Rozproszony monitor w c++ oparty o wiadomości ZeroMQ.
W celu użycia należy dołączć do programu plik DM.h i do kompilacji DM.cpp. 
Wymagane też dolinkowanie zeroMQ (-lzmq) oraz pthread (-pthread), obecność nagłówka zeroMQ dla c++ (zmq.hpp) 
oraz wykorzystanie standardu C++11 (-std=c++11). (przykład w Makefile)

## Wykorzystanie

Główną częśćią monitora jest klasa DM, o następujących metodach publicznych:
```C
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
```    
 W celu utworzenia monitora, należy wywołać jego konstruktor o następujących parametrach:
 - int id - identyfikator węzła, w każdym monitorze z systemu powinien być inny
 - SharedMemory* sharedMemory - wskaźnik na pamięć współdzieloną, dziedziczącą po następującej klasie: 
 ```C
        class SharedMemory{
        public:
            int size;//liczba bajtów, które mają być współdzielone, 
            virtual void serialize(char*)=0;//metoda, która zapisuje size bajtów w adresie wskazanym przez parametr,
            virtual void deserialize(char*)=0;//metoda oczytuje size bajtów z wskazanego adresu, uaktualniając lokalny stan
        };
```
- int port - port pod którym dany węzeł będzie wysyłać wiadomości (będzie on z nim powiązany funkcją bind)
- std::vector<std::string> addr - lista adresów pod którymi będą wszystkie węzły systemu (łącznie z własnym), w formacie dla zmq: 
  tcp://adres:port

Aby zdefiniować metodę lub blok kodu, który ma zostać wykonany w monitorze należy rozpocząć za pomocą DM::enter(),
oznaczającej wejście do monitora, a zakończyć DM::leave czyli wyjściem. 

Aby użyć domyślnej zmiennej warunkowej służą metody DM::wait(), DM::notify() i DM::notify_all(). 

Aby użyć więcej zmiennych warunkowych należy je utworzyć za pomocą metody DM::createNewConditionVariable(), zwracającej 
typ DM::conditionVariable, który można przyekazywać jako parametr powyższych metod (np void wait(conditionVariable condVar);)

**WAŻNE: W każdym węźle zmienne warunkowe muszą być tworzone w tej samej kolejności.**

Przykład dla procesu producenta:

```C
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
 ```   
w pliku main.cpp znajduje się rozwiązanie prostego problemu producenta-konsumenta. 
Należy go uruchomić w kilku kopiach podając jako parametr numer portu będący jednocześnie identyfikatorem 
(w obecnej postaci należy uruchomić 3 kopie z id 1234, 4321 oraz 2222), 
a następnie kliknąć w nich k aby uruchomić konsumenta lub p dla producenta.

## Zasada działania

Rozproszony monitor działa na zasadzie głosowania. Proces chcący wejsć do monitora, wysyła do wszystkich żądanie. Każdy proces po otrzymaniu żądania umieszcza je w kolejce, i jeśli jest pierwsze to rozsyła zgodę na wejscie tego procesu do monitora. Po otrzymaniu zgód od wszystkich procesów, każdy proces sprawdza, który proces dostał ich najwięcej, i jeśli jest to jego proces to pozwala mu wejść. Następnie proces, który wszedł do monitora zostaje usunięty z wszystkich kolejek.

Proces wychodzący z monitora rozyła tę informację, co powoduje, że każdy proces wyraża zgodę na wejście kolejnego procesu z jego kolejki. 

Proces oczekujacy na zmiennej warunkowej, również rozyła tę informację. Każdy kto ja dostanie zapamiętuje ten proces w kolejce związanej ze zmienną, i traktuje to jak wyjście.

Proces sygnalizujący zmienną warunkową, rozsyła informację z procesem, który u niego jest pierwszy w kolejce do tej zmiennej, a każdy proces przenosi go z tej kolejki do kolejki procesów oczekujących na monitor. Sygnał na zmiennej warunkjowej skierowany do wszystkich, powoduje, że wszystkie procesy przenoszą tak całą kolejkę do tej zmiennej.
