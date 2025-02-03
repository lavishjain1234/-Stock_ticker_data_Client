#include <iostream>
#include <winsock2.h>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include <fstream>
#include "json.hpp"

using std::cout;
using std::endl;
using std::cerr;

#pragma comment(lib, "ws2_32.lib")
using json = nlohmann::json;

struct ReqP {
    uint8_t callType;  
    uint8_t resendSeq; 
};

#pragma pack(1)

struct Packet{
    char buysell;
    char symbol[5];
    int32_t quantity;
    int32_t price;
    int32_t packet_sequence;
};

#pragma pack()

int maxseq = 0;
std::map<int, Packet> pckseq;
json jsonArray = json::array();

Packet Parse(const char* buffer, int srt){
    Packet p1;

    memcpy(p1.symbol, &(buffer[srt]), 4);
    p1.symbol[4] = '\0';
    p1.buysell = buffer[srt+4];

    memcpy(&p1.quantity, &(buffer[srt+5]), 4);
    p1.quantity = ntohl(p1.quantity);
    memcpy(&p1.price, &(buffer[srt+9]), 4);
    p1.price = ntohl(p1.price);
    memcpy(&p1.packet_sequence, &(buffer[srt+13]), 4);
    p1.packet_sequence = ntohl(p1.packet_sequence);

    return p1;
}

void printpacket(Packet& p){
    cout << "Packet details: " << p.symbol << " " << p.buysell << " " << p.quantity << " " << p.price << " " << p.packet_sequence << endl;
}

void convert_json(){
    std::map<int, Packet>::iterator i1;
    for(i1 = pckseq.begin(); i1 != pckseq.end(); i1++){
        json packetJson;
        packetJson["symbol"] = i1->second.symbol;

        packetJson["buySell"] = std::string(1, i1->second.buysell);

        packetJson["quantity"] = i1->second.quantity;
        packetJson["price"] = i1->second.price;
        packetJson["packetSequence"] = i1->second.packet_sequence;
        jsonArray.push_back(packetJson);
    }
}

void export_json(){
    std::ofstream outputFile("stock_ticker_data.json");
    if (outputFile.is_open()) {
        outputFile << std::setw(4) << jsonArray << std::endl; 
        outputFile.close();
        std::cout << "JSON written to stock_ticker_data.json" << std::endl;
    } else {
        std::cerr << "Unable to open file for writing" << std::endl;
    }
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    SOCKADDR_IN serverAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    serverAddr.sin_port = htons(3000);

    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connect failed: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server!" << endl;

    ReqP rp1;
    rp1.callType = 1;
    char c1[sizeof(ReqP)] ;
    memcpy(c1, &rp1, sizeof(rp1));
    

    if (send(clientSocket, c1, sizeof(c1), 0) == SOCKET_ERROR) {
        cerr << "Send failed: " << WSAGetLastError() << endl;
    }

    cout << "Request Sent to server!" << endl;
    
    int recvsize = 2048;
    char receiveBuffer[recvsize];
    int pcksize = sizeof(Packet) - 1; 
    while(true){
        int bytesReceived = recv(clientSocket, receiveBuffer, recvsize, 0);
        if (bytesReceived == SOCKET_ERROR) {
            cerr << "Receive failed: " << WSAGetLastError() << endl;
        } 
        else if (bytesReceived == 0) {
            cout << "Connection closed by server." << endl;
            break;
        }
        else if (bytesReceived % pcksize != 0) {
            cerr << "Receive failed: " << WSAGetLastError() << " " << bytesReceived << "  " << sizeof(Packet) << endl;
        }
        else {
            receiveBuffer[bytesReceived] = '\0';
            cout << "Received from server: " << bytesReceived << " bytes" << endl;
        }

        for(int i = 0; i < bytesReceived; i+=pcksize){
            Packet pckt = Parse(receiveBuffer, i);
            printpacket(pckt);
            pckseq.insert({pckt.packet_sequence, pckt});
            maxseq = pckt.packet_sequence;
        }
    }

    //request for missing packets
    closesocket(clientSocket);

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(3000);

    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connect failed: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server!" << endl;

    for(int t = 1; t <= maxseq; t++){
        if(pckseq.find(t)==pckseq.end()){
            ReqP rp2;
            rp2.callType = 2;
            rp2.resendSeq = t;
            char c2[sizeof(ReqP)] ;
            memcpy(c2, &rp2, sizeof(rp2));

            if (send(clientSocket, c2, sizeof(c2), 0) == SOCKET_ERROR) {
                cerr << "Send failed: " << WSAGetLastError() << endl;
            }

            cout << "Request Sent to server for missing packet " <<  t << endl;
            int recvsize2 = 2048;
            char receiveBuffer2[recvsize2];
            int bytesReceived2 = recv(clientSocket, receiveBuffer2, recvsize2, 0);
            if (bytesReceived2 == SOCKET_ERROR) {
                cerr << "Receive failed: " << WSAGetLastError() << endl;
            } 
            else if (bytesReceived2 % pcksize != 0) {
                cerr << "Recieved packets incomplete: " << WSAGetLastError() << endl;
            }
            else {
                receiveBuffer2[bytesReceived2] = '\0';
                cout << "Received from server: " << bytesReceived2 << " bytes" << endl;
            }


            for(int i = 0; i < bytesReceived2; i+=pcksize){
                Packet pckt = Parse(receiveBuffer2, i);
                printpacket(pckt);
                pckseq.insert({pckt.packet_sequence, pckt});
            }
        }
    }
    closesocket(clientSocket);
    WSACleanup();

    convert_json();
    export_json();


    return 0;
}
