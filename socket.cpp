#include<iostream>
#include<WinSock2.h>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <thread>
#include <tuple>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

namespace Exia{
    class BaseSocket{
        public:
            BaseSocket(const char *address,const u_short port);
            void virtual setNoBlock(SOCKET id);
            void virtual setBlock(SOCKET id);
            void virtual dataSend(const SOCKET &target,const std::string &data);
        protected:
            sockaddr_in sin;
    };

    class ServerSocket: public BaseSocket{
        public:
            ServerSocket(const char *address,const u_short port):BaseSocket(address,port){}

            void serverStart();
            int getConnectNum(){return client.size();}
            // void waitForConnect();
            SOCKET& getServerSocket(){return std::ref(server);}
            std::vector<std::tuple<SOCKET,sockaddr_in>>& getClientSocket(){return std::ref(client);}

            virtual void deal(char * data){
                //do nothing, develop by user
            }


        private:
            SOCKET server;
            std::mutex mtx;
            std::vector<std::tuple<SOCKET,sockaddr_in>> client;
            std::thread accept_connection_thread;
            std::thread accept_data_thread;
            std::thread brocast_thread;
            int server_num;
            void serverInit();
            int Listen();
            
    };

    class ClientSocket: public BaseSocket{
        public:
            ClientSocket(const char *address,const u_short port):BaseSocket(address,port){
                clientInit();
            }

            SOCKET getServerSocket(){return server;}
            void clientHosting();
            // void sent(const string &data_send);
        private:
            void clientInit();
            std::thread receive_thread;
            std::thread get_input_send_thread;
            SOCKET server;

    };

    int SocketAPIInit();
    int SocketAPIClonse(){WSACleanup();}
}

int main(){
    std::cout<<"c++ standard: "<<__cplusplus<<std::endl;
    Exia::SocketAPIInit();

    std::cout<<"select socket server or client?(0 for client , 1 for server)"<<std::endl;
    int mode = 0;
    std::cin>>mode;

    if(mode == 0){
        Exia::ClientSocket client("127.0.0.1",8081);
        client.clientHosting();
    }
    else{
        Exia::ServerSocket server("127.0.0.1",8081);
        server.serverStart();
    }
    return 0;
}




Exia::BaseSocket::BaseSocket(const char *address,const u_short port){
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.S_un.S_addr = inet_addr(address);
}

void Exia::BaseSocket::setNoBlock(SOCKET id){
    u_long argp = 1;
    ioctlsocket(id, FIONBIO, &argp);
}

void Exia::BaseSocket::setBlock(SOCKET id){
    u_long argp = 0;
    ioctlsocket(id, FIONBIO, &argp);
}

void Exia::BaseSocket::dataSend(const SOCKET &target,const std::string&data_send){
    send(target,data_send.c_str(),data_send.size(),0);
}

void Exia::ClientSocket::clientInit(){
    server = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(server == INVALID_SOCKET){
        std::cout<<"socket error"<<std::endl;
    }
    if(connect(server,(sockaddr *)&sin,sizeof(sin)) == SOCKET_ERROR){
        std::cout<<"socket error"<<std::endl;
    }
    setNoBlock(server);
    std::cout<<"connet to: "<<inet_ntoa(sin.sin_addr)<<":"<<ntohs(sin.sin_port)<<std::endl;

    receive_thread = std::thread([&](){
        while(true){
            char revData[255];
            int ret = recv(this->getServerSocket(),revData,255,0);
            if(ret>0){
                revData[ret]=0x00;
                std::cout<<"Receive Server Data : "<<revData<<std::endl;
            }
        }
    });

    get_input_send_thread = std::thread([&](){
        while(true){
            std::string send_data;
            std::cin>>send_data;
            if(this->getServerSocket() == SOCKET_ERROR){
                std::cout<<"Server Connection Error"<<std::endl;
                break;
            }
            BaseSocket::dataSend(this->getServerSocket(),send_data);
        }
    });

}

void Exia::ClientSocket::clientHosting(){
    receive_thread.join();
    get_input_send_thread.join();
}


void Exia::ServerSocket::serverStart(){
    try{
        serverInit();
    }catch(const std::exception&e){
        std::cout<<"Panic! "<<e.what()<<std::endl;
    }
    Listen();
    accept_data_thread.join();
    accept_connection_thread.join();
    brocast_thread.join();
}

void Exia::ServerSocket::serverInit(){
    sockaddr_in remote_addr;
    int address_length = sizeof(remote_addr);
    SOCKET slisten = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(slisten == INVALID_SOCKET){
        throw "Socket Instance Error";
    }
    if(bind(slisten,(LPSOCKADDR)&sin,sizeof(sin)) == SOCKET_ERROR){
        throw "Socket Bind Error";
    }
    if(listen(slisten,5)==SOCKET_ERROR){
        throw "Listen Error";
    }

    accept_connection_thread = std::thread([&](){
        std::cout<<"thread: accept_connection_thread create"<<std::endl;
        while(true){
            auto applying_client = accept(slisten,(sockaddr *)&remote_addr,&address_length);
            if(applying_client!= SOCKET_ERROR){
                mtx.lock();
                std::cout<<"IP Adress: "<<inet_ntoa(remote_addr.sin_addr)<<":"<<ntohs(remote_addr.sin_port)<<" apply for connection"<<std::endl;
                client.push_back(std::make_tuple(applying_client,remote_addr));
                std::cout<<"Conncetion Accept"<<std::endl;
                std::cout<<"allocate client for num : "<<client.size()<<std::endl;
                setNoBlock(applying_client);
                mtx.unlock();
            }
        }
    });
}

// void Exia::ServerSocket::waitForConnect(){

// }

int Exia::ServerSocket::Listen(){
    char revData[255];
    accept_data_thread = std::thread([&](){
        std::cout<<"thread: accept_data_thread create"<<std::endl;
        while(true){
            mtx.lock();
            for(auto i:client){
                SOCKET socket = std::get<0>(i);
                auto address = std::get<1>(i);
                if(socket != SOCKET_ERROR){
                    int ret = recv(socket,revData,255,0);
                    if(ret>0){
                        revData[ret] = 0x00;
                        std::cout<<inet_ntoa(address.sin_addr)<<":"<<ntohs(address.sin_port)<<" say:"<<revData<<std::endl;
                        deal(revData);
                    }
                }
                else{
                    cout<<"Socket Error"<<endl;
                }
            }
            mtx.unlock();
        }
    });

    brocast_thread = std::thread([&](){
        std::cout<<"thread: brocast_thread create"<<std::endl;  
        while(true){
            std::string send_data;
            std::cin>>send_data;
            for(const auto& i:client){
                mtx.lock();
                SOCKET socket = std::get<0>(i);
                auto address = std::get<1>(i);
                if(socket != SOCKET_ERROR){
                    cout<<"socket size: "<<client.size()<<endl;
                    BaseSocket::dataSend(socket,send_data);
                }
                else{
                    cout<<"Socket Error"<<endl;
                }
                mtx.unlock();
            }
        }
    });

    return -1;
}

int Exia::SocketAPIInit(){
    WORD sock_version = MAKEWORD(2,2);
    WSADATA wsadata;
    if(WSAStartup(sock_version,&wsadata)!=0){
        std::cout<<"WSAStartup error"<<std::endl;
        return 0;
    }
}