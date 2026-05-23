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
inline float balance = 10000;

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
                        j["gender"].get<std::string>(), j["address"].get<std::string>(),balance
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
                        res["message"] = "This account already exists!";
                    }
                    else {
                        res["status"] = "success";
                        res["message"] = "Account successfully created!";
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
                        res["message"] = "The user is already connected!";
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
                            userData["id"] = user->getId();
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
                                userData["balance"] = c->getBalance();
                            }
                            else {
                                userData["role"] = 1;
                                RentalUnit* adminRental = nullptr;
                                try {
                                    std::lock_guard<std::mutex> lock(mtx);
                                    this->loadAllRentals();
                                    adminRental = findRentalByAdmin(user->getMail());
                                }
                                catch (...) {
                                    adminRental = nullptr;
                                }
                                if (adminRental != nullptr) {
                                    userData["location_id"] = adminRental->getId();
                                    userData["location_name"] = adminRental->getName();
                                }
                                else {
                                    userData["location_id"] = -1;
                                    userData["location_name"] = "Fara locatie alocata";
                                }
                            }
                            res["data"] = userData;
                        }
                        else {
                            res["status"] = "error";
                            res["message"] = "Incorrect data.";
                        }
                    }
                    sendAll(clientSocket, res.dump());
                }

                else if (j["type"] == "GET_RENTALS") {
                    json response;
                    response["type"] = "GET_RENTALS";
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
                    for (auto* acc : snapshot) {
                        json accJson;
                        accJson["id"] = acc->getId();
                        accJson["name"] = acc->getName();
                        accJson["location"] = acc->getLocation();
                        accJson["address"] = acc->getAddress();
                        accJson["discount"] = acc->getDiscount();

                        accJson["promo_name"] = "Special Discount";
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

                else if (j["type"] == "FORCE_LOGOUT") {
                    std::string email = j["email"].get<std::string>();

                    json res;
                    res["type"] = "FORCE_LOGOUT";
                    res["status"] = "success";

                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        for (auto it = users.begin(); it != users.end(); ++it) {
                            if ((*it)->getMail() == email) {
                                delete* it;
                                users.erase(it);
                                break;
                            }
                        }
                    }

                    sendAll(clientSocket, res.dump());
                }

                else if (j["type"] == "GET_LOCATION_BOOKINGS") {
                    int locationId = j["location_id"].get<int>();
                    std::vector<Accomodation> rezervariHotel = this->findAccomodationsByRental(locationId);

                    json response;
                    response["type"] = "GET_LOCATION_BOOKINGS_RESPONSE";
                    json dataArray = json::array();

                    for (const Accomodation& acc : rezervariHotel) {
                        json bJson;
                        bJson["id"] = acc.getAccomodationId();

                        if (acc.getUser() != nullptr) {
                            bJson["client_name"] = acc.getUser()->getName();
                            bJson["client_email"] = acc.getUser()->getMail();
                        }
                        else {
                            bJson["client_name"] = "Unknown Client";
                            bJson["client_email"] = "-";
                        }

                        if (acc.getRentedRoom() != nullptr) {
                            int roomId = acc.getRentedRoom()->getId();
                            bJson["room_id"] = roomId; 

                            std::string tipCamera = "Room";
                            int paturi = 1;
                            if (dynamic_cast<Single*>(acc.getRentedRoom())) { tipCamera = "Single"; paturi = 1; }
                            else if (dynamic_cast<Double*>(acc.getRentedRoom())) { tipCamera = "Double"; paturi = 2; }
                            else if (dynamic_cast<Triple*>(acc.getRentedRoom())) { tipCamera = "Triple"; paturi = 3; }
                            bJson["room_type"] = tipCamera + " Room (" + std::to_string(paturi) + " beds)";
                        }
                        else {
                            bJson["room_id"] = -1;
                            bJson["room_type"] = "Unknown Room Type";
                        }

                        if (acc.getReservationTime() != nullptr && acc.getReservationTime()->getCheckIn() != nullptr && acc.getReservationTime()->getCheckOut() != nullptr) {
                            auto ci = acc.getReservationTime()->getCheckIn();
                            auto co = acc.getReservationTime()->getCheckOut();

                            // Formatăm string YYYY-MM-DD uniform
                            std::string ciStr = std::to_string(ci->getYear()) + "-" + (ci->getMonth() < 10 ? "0" : "") + std::to_string(ci->getMonth()) + "-" + (ci->getDay() < 10 ? "0" : "") + std::to_string(ci->getDay());
                            std::string coStr = std::to_string(co->getYear()) + "-" + (co->getMonth() < 10 ? "0" : "") + std::to_string(co->getMonth()) + "-" + (co->getDay() < 10 ? "0" : "") + std::to_string(co->getDay());

                            bJson["date_range"] = ciStr + " to " + coStr;
                            bJson["raw_check_in"] = ciStr;
                            bJson["raw_check_out"] = coStr;
                        }
                        else {
                            bJson["date_range"] = "Dates N/A";
                            bJson["raw_check_in"] = "";
                            bJson["raw_check_out"] = "";
                        }

                        bJson["status"] = (acc.getAccomodationStatus() == cancelled) ? "cancelled" :
                            (acc.getAccomodationStatus() == finished) ? "finished" : "confirmed";

                        dataArray.push_back(bJson);
                    }
                    response["data"] = dataArray;
                    sendAll(clientSocket, response.dump());
                    }
                else if (j["type"] == "GET_CLIENT_BOOKINGS") {
                    int clientId = j["id"].get<int>();
                    std::vector<Accomodation> listaRezervari = this->findAccomodationsByClient(clientId);

                    json response;
                    response["type"] = "GET_CLIENT_BOOKINGS_RESPONSE";

                    response["data"] = json::array();

                    for (const auto& acc : listaRezervari) {
                        json bJson;

                        std::string numeLocatie = "Cazare Trypo";
                        std::string tipCamera = "Camera Standard";

                        if (acc.getRentedRoom() != nullptr) {
                            int roomId = acc.getRentedRoom()->getId();

                            if (dynamic_cast<Single*>(acc.getRentedRoom())) { tipCamera = "Single Room"; }
                            else if (dynamic_cast<Double*>(acc.getRentedRoom())) { tipCamera = "Double Room"; }
                            else if (dynamic_cast<Triple*>(acc.getRentedRoom())) { tipCamera = "Triple Room"; }
                            for (auto* rental : this->rentals) {
                                if (rental != nullptr) {
                                    for (auto* room : rental->getRooms()) {
                                        if (room != nullptr && room->getId() == roomId) {
                                            numeLocatie = rental->getName();
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        bJson["id"] = acc.getAccomodationId();
                        bJson["hotel_name"] = numeLocatie;
                        bJson["room_type"] = tipCamera;

                        if (acc.getReservationTime() != nullptr &&
                            acc.getReservationTime()->getCheckIn() != nullptr &&
                            acc.getReservationTime()->getCheckOut() != nullptr) {

                            auto ci = acc.getReservationTime()->getCheckIn();
                            auto co = acc.getReservationTime()->getCheckOut();

                            std::string ciStr = std::to_string(ci->getYear()) + "-" +
                                (ci->getMonth() < 10 ? "0" : "") + std::to_string(ci->getMonth()) + "-" +
                                (ci->getDay() < 10 ? "0" : "") + std::to_string(ci->getDay());

                            std::string coStr = std::to_string(co->getYear()) + "-" +
                                (co->getMonth() < 10 ? "0" : "") + std::to_string(co->getMonth()) + "-" +
                                (co->getDay() < 10 ? "0" : "") + std::to_string(co->getDay());

                            bJson["date_range"] = ciStr + " to " + coStr;
                            bJson["raw_check_in"] = ciStr;
                            bJson["raw_check_out"] = coStr;

                            // Calculează și trimite costul total
                            if (acc.getRentedRoom() != nullptr) {
                                int nights = calculateNights(ciStr, coStr); // funcția există deja
                                bJson["total_cost"] = acc.getRentedRoom()->getPrice() * nights;
                            }
                            else {
                                bJson["total_cost"] = 0.0;
                            }
                        }
                        else {
                            bJson["date_range"] = "Dates N/A";
                        }

                        bJson["status"] = (acc.getAccomodationStatus() == cancelled) ? "cancelled" :
                            (acc.getAccomodationStatus() == finished) ? "finished" : "confirmed";

                        response["data"].push_back(bJson);
                    }

                    sendAll(clientSocket, response.dump());
                    }

                else if (j["type"] == "ADMIN_CANCEL_BOOKING") {
                    int bookingId = j["id"].get<int>();

                    bool success = this->cancelBookingInDb(bookingId);

                    json res;
                    res["type"] = "ADMIN_CANCEL_BOOKING_RESPONSE";

                    if (success) {
                        res["status"] = "success";
                    }
                    else {
                        res["status"] = "error";
                        res["message"] = "Eroare la nivelul bazei de date SQL.";
                    }

                    sendAll(clientSocket, res.dump());
                }

                else if (j["type"] == "CREATE_RESERVATION") {
                    int clientId = j["client_id"].get<int>();
                    int roomId = j["room_id"].get<int>();
                    std::string checkIn = j["check_in"].get<std::string>();
                    std::string checkOut = j["check_out"].get<std::string>();
                    double totalCost = j["total_cost"].get<double>();

                    double newBalance = 0.0;

                    bool success = this->createReservation(clientId, roomId, checkIn, checkOut, totalCost, newBalance);

                    json res;
                    res["type"] = "CREATE_RESERVATION_RESPONSE";

                    if (success) {
                        res["status"] = "success";
                        res["new_balance"] = newBalance;
                        res["message"] = "Reservation has been successfully registered!";
                    }
                    else {
                        res["status"] = "error";
                        res["message"] = "Rezervarea a esuat. Fonduri insuficiente sau eroare de conexiune server.";
                    }

                    sendAll(clientSocket, res.dump());
                    }
                else if (j["type"] == "CLIENT_CANCEL_BOOKING") {
                    
                    std::string clientMail = j["client_mail"].get<std::string>();
                    int bookingId = j["b_id"].get<int>();
                    double newbalance = j["new_balance"].get<double>();
                    json res;
                    bool success = this->cancelBookingByClient(clientMail, bookingId, newbalance);
                    res["type"] = "CLIENT_CANCEL_BOOKING_RESPONSE";

                    if (success) {
                        res["status"] = "success";
                        res["message"] = "Rezervarea a fost anulata cu succes !";
                    }
                    else {
                        res["status"] = "error";
                        res["message"] = "Anularea a esuat. Va rog luati legatura cu administratorul sau reincercati.";
                    }
                    sendAll(clientSocket, res.dump());
                }

            }
            catch (const std::exception& e) {
                std::cout << "Eroare la procesarea cererii: " << e.what() << std::endl;
            }
        } 

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
            stopServer();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::lock_guard<std::mutex> lock(mtx);

            for (auto r : rentals) {
                if (r != nullptr) {
                    delete r;
                }
            }
            rentals.clear();

            for (auto u : users) {
                if (u != nullptr) {
                    delete u;
                }
            }
            users.clear();
     }
   

    void registerUser(IUsers* newUser);
    bool searchUser(IUsers* newUser);
    IUsers* loginUser(string mail, string password);
    IUsers* searchUserById(int id);
    void printUsers();
    void printRentals();
    void loadAllRentals();
    RentalUnit* findRentalById(int id);
    RentalUnit* findRentalByAdmin(string mail);
    void populateRoomsOfRental(int id);
    IRoom* findRoomById(int roomId);
    std::vector<Accomodation> findAccomodationsByRental(int rentalId);
    std::vector<Accomodation> findAccomodationsByClient(int ClientId);
    bool cancelBookingInDb(int bookingId);
    bool createReservation(int clientId, int roomId, const std::string& checkIn, const std::string& checkOut, double totalCost, double& outNewBalance);
    bool cancelBookingByClient(std::string& mail, int bookingId, double newBalance);
    int calculateNights(const std::string& startStr, const std::string& endStr);
};