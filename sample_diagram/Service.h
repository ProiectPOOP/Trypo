#pragma once
#define WIN32_LEAN_AND_MEAN
#define _HAS_STD_BYTE 0
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <thread>
#include <nanodbc/nanodbc.h>
#include "IUsers.h"
#include "Accomodation.h"
#include "Logger.h"
#include "Admin.h"
#include "Client.h"
#include "RentalUnit.h"
#include "Promotions.h"
#include "Single.h"
#include "Double.h"
#include "Triple.h"
#include "Extras.h"
#include "Facilities.h"
#include "MyException.h"
#include "json.hpp"
#include <mutex>
using json = nlohmann::json;
#pragma comment(lib, "ws2_32.lib")


static bool sendAll(SOCKET s, const std::string& data) {
    uint32_t len = htonl(static_cast<uint32_t>(data.size()));
    if (send(s, reinterpret_cast<const char*>(&len), 4, 0) != 4)
        return false;
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.c_str() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

static bool recvAll(SOCKET s, std::string& out) {
    uint32_t netLen = 0;
    int got = recv(s, reinterpret_cast<char*>(&netLen), 4, MSG_WAITALL);
    if (got != 4) return false;
    uint32_t len = ntohl(netLen);
    if (len == 0 || len > 64 * 1024 * 1024) return false;
    out.resize(len);
    size_t received = 0;
    while (received < len) {
        int n = recv(s, &out[received], static_cast<int>(len - received), 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}


class Service {
private:
    nanodbc::connection conn;
    SOCKET serverSocket;
    bool serverRunning;
    std::thread workerThread;
    std::vector<IUsers*> users;
    std::vector<Accomodation*> accom;
    std::vector<RentalUnit*> rentals;
    std::mutex mtx;

public:

    nanodbc::connection& getConn() {
        return conn;
    }

    Service(string connection_string) : serverRunning(false), serverSocket(INVALID_SOCKET) {
        try {
            conn = nanodbc::connection(connection_string);
            if (conn.connected()) {
                cout << "Conexiune reusita la BD.";
                Logger::getInstanceLogger().setMessage("Conexiune reusita la BD.");
                Logger::getInstanceLogger().printMessageOnFile();
            }
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw MyException("Eroare la WSAStartup");
            }

            workerThread = std::thread(&Service::startServer, this);
            workerThread.detach();
        }
        catch (const MyException& e) {
            Logger::getInstanceLogger().setMessage("Eroare la conectare: " + std::string(e.what()));
            Logger::getInstanceLogger().printMessageOnFile();
        }
    }

    void handleClient(SOCKET clientSocket, std::string clientIP) {
        std::string emailLogat = "";

        while (serverRunning) {
            std::string request;
            if (!recvAll(clientSocket, request)) {
                Logger::getInstanceLogger().setMessage("[INFO] Conexiunea cu " + clientIP + " inchisa.");
                Logger::getInstanceLogger().printMessageOnFile();
                break;
            }

            try {
                auto j = json::parse(request);


                if (j.contains("type") && j["type"] == "REGISTER_USER") {
                    IUsers* newUser = new Client(
                        j["name"].get<std::string>(), j["password"].get<std::string>(),
                        j["email"].get<std::string>(), j["phone"].get<std::string>(),
                        j["dob"].get<std::string>(), j["country"].get<std::string>(),
                        j["gender"].get<std::string>(), j["address"].get<std::string>()
                    );

                    json res;
                    res["type"] = "REGISTER_RESPONSE";

                    bool exists = false;
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        exists = this->searchUser(newUser);
                        if (!exists)
                            this->registerUser(newUser);
                    }

                    if (exists) {
                        res["status"] = "error";
                        res["message"] = "Acest email este deja utilizat!";
                    }
                    else {
                        res["status"] = "success";
                        res["message"] = "Cont creat cu succes!";
                    }

                    sendAll(clientSocket, res.dump());
                    delete newUser;
                }
                else if (j.contains("type") && j["type"] == "LOGIN_USER") {
                    std::string email = j["email"].get<std::string>();
                    std::string password = j["password"].get<std::string>();

                    json res;
                    res["type"] = "LOGIN_RESPONSE";

                    bool isAlreadyConnected = false;
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        for (auto u : this->users) {
                            if (u->getMail() == email) {
                                isAlreadyConnected = true;
                                break;
                            }
                        }
                    }

                    if (isAlreadyConnected) {
                        res["status"] = "error";
                        res["message"] = "Utilizatorul este deja conectat";
                    }
                    else {
                        IUsers* user = this->loginUser(email, password);
                        if (user != nullptr) {
                            emailLogat = email;
                            {
                                std::lock_guard<std::mutex> lock(mtx);
                                this->users.push_back(user);
                            }
                            res["status"] = "success";

                            json userData;
                            userData["name"] = user->getName();
                            userData["email"] = user->getMail();
                            userData["phone"] = user->getPhone();

                            Client* c = dynamic_cast<Client*>(user);
                            if (c) {
                                userData["role"] = 0;
                                userData["dob"] = c->getBirthDate();
                                userData["country"] = c->getCountry();
                                userData["gender"] = c->getGender();
                                userData["address"] = c->getAddress();
                            }
                            else {
                                userData["role"] = 1;
                            }
                            res["data"] = userData;
                        }
                        else {
                            res["status"] = "error";
                            res["message"] = "Date incorecte.";
                        }
                    }
                    sendAll(clientSocket, res.dump());
                }

                else if (j["type"] == "GET_ACCOMMODATIONS") {
                    json response;
                    response["type"] = "GET_ACCOMMODATIONS";
                    json dataArray = json::array();

                    std::vector<RentalUnit*> snapshot;
                    {
                        std::lock_guard<std::mutex> lock(mtx);

                        for (auto* r : this->rentals) delete r;
                        this->rentals.clear();

                        this->loadAllRentals();

                        for (auto* acc : this->rentals)
                            this->populateRoomsOfRental(acc->getId());

                        snapshot = this->rentals; // copy pointers for read-only iteration
                    }

                    // Build JSON outside the lock (read-only access to snapshot)
                    for (auto* acc : snapshot) {
                        json accJson;
                        accJson["id"] = acc->getId();
                        accJson["name"] = acc->getName();
                        accJson["location"] = acc->getLocation();
                        accJson["address"] = acc->getAddress();

                        json roomsArray = json::array();
                        for (auto* room : acc->getRooms()) {
                            json rJson;
                            rJson["id"] = room->getId();
                            rJson["price"] = room->getPrice();

                            Facilities* f = room->getFacilities();
                            rJson["facilities"] = json::array();
                            if (f->getAC())      rJson["facilities"].push_back("AC");
                            if (f->getBalcony()) rJson["facilities"].push_back("Balcony");
                            if (f->getCouch())   rJson["facilities"].push_back("Couch");
                            if (f->getFridge())  rJson["facilities"].push_back("Fridge");
                            if (f->getTV())      rJson["facilities"].push_back("TV");

                            if (dynamic_cast<Single*>(room)) {
                                rJson["type"] = "Single";
                                rJson["beds"] = 1;
                            }
                            else if (dynamic_cast<Double*>(room)) {
                                rJson["type"] = "Double";
                                rJson["beds"] = 2;
                            }
                            else if (dynamic_cast<Triple*>(room)) {
                                rJson["type"] = "Triple";
                                rJson["beds"] = 3;
                            }

                            roomsArray.push_back(rJson);
                        }

                        accJson["rooms"] = roomsArray;
                        dataArray.push_back(accJson);
                    }

                    response["data"] = dataArray;
                    sendAll(clientSocket, response.dump());
                }

                // ----------------------------------------------------------
                // FORCE_LOGOUT
                // ----------------------------------------------------------
                else if (j["type"] == "FORCE_LOGOUT") {
                    std::string email = j["email"].get<std::string>();

                    json res;
                    res["type"] = "FORCE_LOGOUT";

                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        for (auto it = users.begin(); it != users.end(); ++it) {
                            if ((*it)->getMail() == email) {
                                delete* it;
                                users.erase(it);
                                if (emailLogat == email)
                                    emailLogat = "";
                                break;
                            }
                        }
                    }

                    res["status"] = "success";
                    res["message"] = "Utilizatorul a fost deconectat";
                    sendAll(clientSocket, res.dump());
                }

            }
            catch (const std::exception& e) {
                std::cout << "Eroare la procesarea cererii: " << e.what() << std::endl;
            }
        } // end while(serverRunning)

        // Session cleanup when the connection drops
        if (!emailLogat.empty()) {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto it = users.begin(); it != users.end(); ++it) {
                if ((*it)->getMail() == emailLogat) {
                    delete* it;
                    users.erase(it);
                    Logger::getInstanceLogger().setMessage("[CLEANUP] Sesiune inchisa pentru: " + emailLogat);
                    Logger::getInstanceLogger().printMessageOnFile();
                    break;
                }
            }
        }

        closesocket(clientSocket);
    }

    void startServer() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            Logger::getInstanceLogger().setMessage("Eroare la crearea socket-ului.");
            Logger::getInstanceLogger().printMessageOnFile();
            return;
        }

        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(12345);

        if (::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            Logger::getInstanceLogger().setMessage("Eroare la Bind. Cod: " + std::to_string(WSAGetLastError()));
            Logger::getInstanceLogger().printMessageOnFile();
            closesocket(serverSocket);
            return;
        }

        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            Logger::getInstanceLogger().setMessage("Eroare la Listen.");
            Logger::getInstanceLogger().printMessageOnFile();
            closesocket(serverSocket);
            return;
        }

        serverRunning = true;
        Logger::getInstanceLogger().setMessage("Serverul C++ asculta pe portul 12345...");
        Logger::getInstanceLogger().printMessageOnFile();

        while (serverRunning) {
            sockaddr_in clientAddr;
            int clientSize = sizeof(clientAddr);

            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);

            if (clientSocket != INVALID_SOCKET && serverRunning) {
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
                Logger::getInstanceLogger().setMessage(std::string("[CONEXIUNE] Client nou: ") + std::string(clientIP));
                cout << "Client nou conectat: " << clientIP << endl;

                std::thread t(&Service::handleClient, this, clientSocket, std::string(clientIP));
                t.detach();
            }
        }
    }

    void stopServer() {
        serverRunning = false;
        closesocket(serverSocket);
        WSACleanup();
    }

    ~Service() {
        for (auto a : accom) delete a;
        accom.clear();
        for (auto u : users) delete u;
        users.clear();
        for (auto r : rentals) delete r;
        rentals.clear();
        stopServer();
    }

    void registerUser(IUsers* newUser);
    bool searchUser(IUsers* newUser);
    IUsers* loginUser(string mail, string password);
    void printUsers();
    void printRentals();
    void loadAllRentals();
    RentalUnit* findRentalById(int id);
    void populateRoomsOfRental(int id);
    void loadAllAccomodation();
    void loadAccomodationsByClient(int id);
    void loadAccomodationsByRoom(int id);
    void createAccomodations(Accomodation* a);
    void updateAccomodations();
    void cancelAccomodation(int id);
};