#include "Service.h"

void Service::registerUser(IUsers* newUser) {
    if (!newUser) return;

    try {

        nanodbc::statement stmt(this->conn);
        nanodbc::prepare(stmt, "INSERT INTO Users (Name, Password, Mail, PhoneNumber, BirthDate, Country, Gender, Address,Role,Balance) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        std::string n = newUser->getName();
        std::string p = newUser->getPassword();
        std::string m = newUser->getMail();
        std::string ph = newUser->getPhone();
        std::string bd = newUser->getBirthDate();
        std::string c = newUser->getCountry();
        std::string g = newUser->getGender();
        std::string a = newUser->getAddress();
        int r = 0;
        float balance = newUser->getBalance();

        stmt.bind(0, n.c_str());
        stmt.bind(1, p.c_str());
        stmt.bind(2, m.c_str());
        stmt.bind(3, ph.c_str());
        stmt.bind(4, bd.c_str());
        stmt.bind(5, c.c_str());
        stmt.bind(6, g.c_str());
        stmt.bind(7, a.c_str());
        stmt.bind(8, &r);
        stmt.bind(9, &balance);
        nanodbc::execute(stmt);

        Logger::getInstanceLogger().setMessage("Utilizator inregistrat cu succes: " + n);
        Logger::getInstanceLogger().printMessageOnFile();
    }
    catch (const std::exception& e) {
        Logger::getInstanceLogger().setMessage("Eroare la inregistrare: " + std::string(e.what()));
        Logger::getInstanceLogger().printMessageOnFile();
        throw;
    }

}
bool Service::searchUser(IUsers* newUser) {
    try {
        nanodbc::statement stmt(this->conn);
        nanodbc::prepare(stmt, "SELECT * FROM Users WHERE Mail = ?");

        stmt.bind(0, newUser->getMail().c_str());

        auto result = nanodbc::execute(stmt);

        if (result.next())
            return true;
        else
            return false;
    }
    catch (const std::exception& e) {
        Logger::getInstanceLogger().setMessage("Eroare SQL Login: " + std::string(e.what()));
        Logger::getInstanceLogger().printMessageOnFile();
        return true;
    }
}

IUsers* Service::loginUser(string mail, string password) {
    for (auto u : this->users) {
        if (u->getMail() == mail) {
            Logger::getInstanceLogger().setMessage("Tentativa de login dublu pentru: " + mail);
            Logger::getInstanceLogger().printMessageOnFile();
            return nullptr;
        }
    }
    try {
        nanodbc::statement stmt(this->conn);
        nanodbc::prepare(stmt, "SELECT * FROM Users WHERE Mail = ? AND Password = ?");

        stmt.bind(0, mail.c_str());
        stmt.bind(1, password.c_str());

        auto result = nanodbc::execute(stmt);

        if (result.next()) {
            int id = result.get<int>("Id");
            string nume = result.get<string>("Name");
            string phone = result.get<string>("PhoneNumber");
            string birthDate = result.get<string>("BirthDate");
            string country = result.get<string>("Country");
            string gender = result.get<string>("Gender");
            string address = result.get<string>("Address");
            float balance = result.get<float>("Balance");
            int rol = result.get<int>("Role");
            string role;
            IUsers* u = nullptr;
            if (rol == 1) {
                u = new Admin(nume, password, mail, phone);
                role = "Admin";


            }
            else {
                u = new Client(nume, password, mail, phone, birthDate, country, gender, address,balance);
                role = "Client";

            }
            Logger::getInstanceLogger().setMessage("Login reusit pentru: " + nume + " [" + role + "]");
            Logger::getInstanceLogger().printMessageOnFile();
            u->setId(id);
            return u;
        }
        else {
            return nullptr;
            Logger::getInstanceLogger().setMessage("Tentativa de login esuata pentru: " + mail);
            Logger::getInstanceLogger().printMessageOnFile();
        }
    }
    catch (const std::exception& e) {
        Logger::getInstanceLogger().setMessage("Eroare SQL Login: " + std::string(e.what()));
        Logger::getInstanceLogger().printMessageOnFile();
    }
    return nullptr;
}
IUsers* Service::searchUserById(int id) {
    try {
        nanodbc::statement stmt(conn);

        nanodbc::prepare(stmt, "SELECT * FROM Users WHERE Id = ?");
        stmt.bind(0, &id);

        auto result = nanodbc::execute(stmt);

        if (result.next()) {
            std::string nume = result.get<std::string>("Name");
            std::string password = result.get<std::string>("Password");
            std::string mail = result.get<std::string>("Mail");
            std::string phone = result.get<std::string>("PhoneNumber", "");
            std::string birthDate = result.get<std::string>("BirthDate", "");
            std::string country = result.get<std::string>("Country", "");
            std::string gender = result.get<std::string>("Gender", "");
            std::string address = result.get<std::string>("Address", "");
            float bal = result.get<float>("Balance", 0.0f);
            int rol = result.get<int>("Role", 0);

            IUsers* u = nullptr;
            // Instanțiem tipul corect de obiect în funcție de rol
            if (rol == 1) {
                u = new Admin(nume, password, mail, phone);
            }
            else {
                u = new Client(nume, password, mail, phone, birthDate, country, gender, address, bal);
            }
            return u;
        }
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] searchUserById a esuat: " << e.what() << std::endl;
    }
    return nullptr; // Returnează nullptr dacă utilizatorul nu a fost găsit
}
void Service::printUsers() {
    cout << "Lista utilizatorilor: " << endl;
    auto results = nanodbc::execute(getConn(), "SELECT Id, Name, Mail, Country, Role FROM Users");

    while (results.next()) {
        int id = results.get<int>("Id");
        string nume = results.get<string>("Name");
        string email = results.get<string>("Mail");
        string tara = results.get<string>("Country");
        string adr = results.get<string>("Address");
        int rol = results.get<int>("Role");
        string role;
        if (rol == 0)
            role = "Client";
        else
            role = "Admin";

        cout << "ID: " << id << " | Nume: " << nume << " | Email: " << email << " | Tara: " << tara << " | Rol: " << role << " | Adresa: " << adr << endl;
    }
}

void Service::printRentals() {
    cout << "Lista unitatilor de cazare: " << endl;
    auto results = nanodbc::execute(getConn(), "SELECT id, name, address,capacity FROM RentalUnit");

    while (results.next()) {
        int id = results.get<int>("id");
        string nume = results.get<string>("name");
        string address = results.get<string>("address");
        int capacity = results.get<int>("capacity");
        cout << "ID: " << id << " | Nume: " << nume << " | Adresa: " << address << " | Capacity: " << capacity << endl;
    }
}

void Service::loadAllRentals() {
    auto results = nanodbc::execute(getConn(), "SELECT * FROM RentalUnit");
    while (results.next()) {
        int id = results.get<int>("id");
        string nume = results.get<string>("name");
        string address = results.get<string>("address");
        string location = results.get<string>("location");
        int capacity = results.get<int>("capacity");
        float discount = results.get<float>("discount");
        string mailAdmin = results.get<string>("ownerEmail");
        RentalUnit* r = new RentalUnit(id, nume, location, address, capacity, discount,mailAdmin);
        if (findRentalById(id) == nullptr)
            this->rentals.push_back(r);
    }

}
RentalUnit* Service::findRentalById(int id) {
    for (auto r : this->rentals)
        if (r->getId() == id)
            return r;
    return nullptr;
}

RentalUnit* Service::findRentalByAdmin(string mail) {
    for (auto r : this->rentals)
        if (r->getAdmin() == mail)
            return r;
    return nullptr;
}
void Service::populateRoomsOfRental(int id) {
    nanodbc::statement stmt(this->conn);
    nanodbc::prepare(stmt, ("SELECT * FROM Rooms WHERE rentalUnitID = ?"));
    stmt.bind(0, &id);
    auto results = nanodbc::execute(stmt);
    while (results.next()) {
        int capacity = results.get<int>("capacity");
        int ID = results.get<int>("id");
        float price = results.get<float>("pricePerNight");
        bool bf = results.get<int>("breakfast");
        bool eC = results.get<int>("extraCleaning");
        bool park = results.get<int>("parking");
        bool pool = results.get<int>("pool");
        bool sauna = results.get<int>("sauna");
        bool AC = results.get<int>("AC");
        bool balcony = results.get<int>("balcony");
        bool couch = results.get<int>("couch");
        bool fridge = results.get<int>("fridge");
        int numBeds = results.get<int>("numBeds");
        bool TV = results.get<int>("TV");
        Extras* e = new Extras(bf, park, pool, sauna, eC);
        Facilities* f = new Facilities(balcony, fridge, AC, numBeds, TV, couch);
        RentalUnit* r = findRentalById(id);
        IRoom* c=nullptr;
        if (capacity == 1)
            c = new Single(ID, price, f, e);
        else
            if (capacity == 2)
                c = new Double(ID, price, f, e);
            else
                c = new Triple(ID, price, f, e);
        r->addRoom(c);
    }
}
IRoom* Service::findRoomById(int roomId) {
    try {
        std::lock_guard<std::mutex> lock(mtx);
        nanodbc::statement stmt(conn);
        nanodbc::prepare(stmt, "SELECT * FROM Rooms WHERE id = ?");
        stmt.bind(0, &roomId);

        auto results = nanodbc::execute(stmt);

        if (results.next()) {
            int capacity = results.get<int>("capacity");
            float price = results.get<float>("pricePerNight");

            bool bf = results.get<int>("breakfast") != 0;
            bool eC = results.get<int>("extraCleaning") != 0;
            bool park = results.get<int>("parking") != 0;
            bool pool = results.get<int>("pool") != 0;
            bool sauna = results.get<int>("sauna") != 0;

            bool AC = results.get<int>("AC") != 0;
            bool balcony = results.get<int>("balcony") != 0;
            bool couch = results.get<int>("couch") != 0;
            bool fridge = results.get<int>("fridge") != 0;
            bool TV = results.get<int>("TV") != 0;
            int numBeds = results.get<int>("numBeds");

            Extras* e = new Extras(bf, park, pool, sauna, eC);
            Facilities* f = new Facilities(balcony, fridge, AC, numBeds, TV, couch);

            IRoom* room = nullptr;
            if (capacity == 1)      room = new Single(roomId, price, f, e);
            else if (capacity == 2) room = new Double(roomId, price, f, e);
            else                    room = new Triple(roomId, price, f, e);

            return room;
        }
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] findRoomById a esuat: " << e.what() << std::endl;
    }
    return nullptr;
}

vector<Accomodation> Service::findAccomodationsByRental(int rentalId) {
    std::vector<Accomodation> listaRezervari;

    struct RawRow {
        int accId;
        int clientId;
        int roomId;
        std::string dbStatus;
        std::string ciStr;
        std::string coStr;
    };
    std::vector<RawRow> rawRows;

    try {
        std::lock_guard<std::mutex> lock(mtx);
        nanodbc::statement stmt(conn);
        nanodbc::prepare(stmt,
            "SELECT a.id, a.clientId, a.rentedRoomId, a.status, a.checkIn, a.checkOut "
            "FROM Accomodation a "
            "JOIN Rooms r ON a.rentedRoomId = r.id "
            "WHERE r.rentalUnitID = ?");

        stmt.bind(0, &rentalId);
        auto res = nanodbc::execute(stmt);

        while (res.next()) {
            RawRow row;
            row.accId = res.get<int>("id");
            row.clientId = res.get<int>("clientId");
            row.roomId = res.get<int>("rentedRoomId");
            row.dbStatus = res.get<std::string>("status");
            row.ciStr = res.get<std::string>("checkIn");
            row.coStr = res.get<std::string>("checkOut");
            rawRows.push_back(row);
        }
    } 
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] findAccomodationsByRental a esuat: " << e.what() << std::endl;
        return listaRezervari;
    }


    for (const auto& row : rawRows) {
        status accStatus = confirmed;
        if (row.dbStatus == "cancelled") accStatus = cancelled;
        else if (row.dbStatus == "finished") accStatus = finished;

        IUsers* user = searchUserById(row.clientId);
        IRoom* room = findRoomById(row.roomId);


        int ziIn = 1, lunaIn = 1, anIn = 2026;
        if (row.ciStr.length() >= 10) {
            anIn = std::stoi(row.ciStr.substr(0, 4));
            lunaIn = std::stoi(row.ciStr.substr(5, 2));
            ziIn = std::stoi(row.ciStr.substr(8, 2));
        }
        Date* checkInDate = new Date(ziIn, lunaIn, anIn);

        int ziOut = 1, lunaOut = 1, anOut = 2026;
        if (row.coStr.length() >= 10) {
            anOut = std::stoi(row.coStr.substr(0, 4));
            lunaOut = std::stoi(row.coStr.substr(5, 2));
            ziOut = std::stoi(row.coStr.substr(8, 2));
        }
        Date* checkOutDate = new Date(ziOut, lunaOut, anOut);

        ReservationTime* timeObj = new ReservationTime(checkInDate, checkOutDate);
        Accomodation acc(row.accId, row.clientId, user, room, accStatus, timeObj);

        listaRezervari.push_back(acc);
    }

    return listaRezervari;
}

vector<Accomodation> Service::findAccomodationsByClient(int ClientId) {
    std::vector<Accomodation> listaRezervari;

    struct RawRow {
        int accId;
        int clientId;
        int roomId;
        std::string dbStatus;
        std::string ciStr;
        std::string coStr;
    };
    std::vector<RawRow> rawRows;

    try {
        std::lock_guard<std::mutex> lock(mtx);
        nanodbc::statement stmt(conn);

        nanodbc::prepare(stmt,
            "SELECT id, clientId, rentedRoomId, status, checkIn, checkOut "
            "FROM Accomodation WHERE clientId = ?");

        stmt.bind(0, &ClientId);
        auto res = nanodbc::execute(stmt);

        while (res.next()) {
            RawRow row;
            row.accId = res.get<int>("id");
            row.clientId = res.get<int>("clientId");
            row.roomId = res.get<int>("rentedRoomId");
            row.dbStatus = res.get<std::string>("status");
            row.ciStr = res.get<std::string>("checkIn");
            row.coStr = res.get<std::string>("checkOut");
            rawRows.push_back(row);
        }
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] findAccomodationsByClient a esuat: " << e.what() << std::endl;
        return listaRezervari;
    }

    for (const auto& row : rawRows) {
        status accStatus = confirmed;
        if (row.dbStatus == "cancelled") accStatus = cancelled;
        else if (row.dbStatus == "finished") accStatus = finished;

        // Conexiunea este liberă, deci căutările ssub-obiectelor vor rula perfect
        IUsers* user = searchUserById(row.clientId);
        IRoom* room = findRoomById(row.roomId);

        // Extragere dinamică dată Check-In
        int ziIn = 1, lunaIn = 1, anIn = 2026;
        if (row.ciStr.length() >= 10) {
            anIn = std::stoi(row.ciStr.substr(0, 4));
            lunaIn = std::stoi(row.ciStr.substr(5, 2));
            ziIn = std::stoi(row.ciStr.substr(8, 2));
        }
        Date* checkInDate = new Date(ziIn, lunaIn, anIn);

        // Extragere dinamică dată Check-Out
        int ziOut = 1, lunaOut = 1, anOut = 2026;
        if (row.coStr.length() >= 10) {
            anOut = std::stoi(row.coStr.substr(0, 4));
            lunaOut = std::stoi(row.coStr.substr(5, 2));
            ziOut = std::stoi(row.coStr.substr(8, 2));
        }
        Date* checkOutDate = new Date(ziOut, lunaOut, anOut);

        ReservationTime* timeObj = new ReservationTime(checkInDate, checkOutDate);
        Accomodation acc(row.accId, row.clientId, user, room, accStatus, timeObj);

        listaRezervari.push_back(acc);
    }

    return listaRezervari;
}
bool Service::cancelBookingInDb(int bookingId) {
    try {
        std::lock_guard<std::mutex> lock(mtx);
        nanodbc::statement stmt(conn);

        nanodbc::prepare(stmt, "UPDATE Accomodation SET status = 'cancelled' WHERE id = ?");
        stmt.bind(0, &bookingId);
        nanodbc::execute(stmt);

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] Anularea rezervarii a esuat in Service: " << e.what() << std::endl;
        return false;
    }
}
 int Service::calculateNights(const std::string& startStr, const std::string& endStr) {
    std::tm tm_start = {}, tm_end = {};
    std::istringstream ss_start(startStr), ss_end(endStr);
    ss_start >> std::get_time(&tm_start, "%Y-%m-%d");
    ss_end >> std::get_time(&tm_end, "%Y-%m-%d");

    if (ss_start.fail() || ss_end.fail()) return 1;

    auto tp_start = std::chrono::system_clock::from_time_t(std::mktime(&tm_start));
    auto tp_end = std::chrono::system_clock::from_time_t(std::mktime(&tm_end));

    auto duration = tp_end - tp_start;
    int days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
    return days <= 0 ? 1 : days;
}

bool Service::createReservation(int clientId, int roomId, const std::string& checkIn, const std::string& checkOut, double totalCost, double& outNewBalance)
{
    try {
        std::lock_guard<std::mutex> lock(mtx);

        nanodbc::statement stmtBalance(conn);
        nanodbc::prepare(stmtBalance, "SELECT Balance FROM Users WHERE Id = ?");
        stmtBalance.bind(0, &clientId);
        auto rowBalance = nanodbc::execute(stmtBalance);

        double currentBalance = 0.0;
        if (rowBalance.next()) {
            currentBalance = static_cast<double>(rowBalance.get<float>("Balance", 0.0f));
        }
        else {
            std::cout << "[SQL ERROR] Utilizatorul cu ID " << clientId << " nu exista." << std::endl;
            return false;
        }

        if (currentBalance < totalCost) {
            std::cout << "[SQL WARNING] Fonduri insuficiente pe server! Balanta: " << currentBalance << " € | Cerut: " << totalCost << " €" << std::endl;
            return false;
        }
        nanodbc::statement stmtUpdateUser(conn);
        nanodbc::prepare(stmtUpdateUser, "UPDATE Users SET Balance = Balance - ? WHERE Id = ?");
        stmtUpdateUser.bind(0, &totalCost);
        stmtUpdateUser.bind(1, &clientId);
        nanodbc::execute(stmtUpdateUser);

        nanodbc::statement stmtInsert(conn);
        nanodbc::prepare(stmtInsert,
            "INSERT INTO Accomodation (status, clientId, rentedRoomId, checkIn, checkOut) "
            "VALUES ('confirmed', ?, ?, ?, ?)");

        stmtInsert.bind(0, &clientId);
        stmtInsert.bind(1, &roomId);
        stmtInsert.bind(2, checkIn.c_str());
        stmtInsert.bind(3, checkOut.c_str());
        nanodbc::execute(stmtInsert);
        outNewBalance = currentBalance - totalCost;
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] Salvarea rezervarii a esuat in Service: " << e.what() << std::endl;
        return false;
    }
}


bool Service::cancelBookingByClient(std::string& mail, int bookingId, double newBalance) {
    try {
        std::lock_guard<std::mutex> lock(mtx);
        nanodbc::statement stmt(conn);


        nanodbc::statement stmtBalance(conn);
        nanodbc::prepare(stmtBalance, "UPDATE Users SET Balance = ? WHERE Mail = ?");
        stmtBalance.bind(0, &newBalance);
        stmtBalance.bind(1, mail.c_str());
        auto rowBalance = nanodbc::execute(stmtBalance);


        nanodbc::prepare(stmt, "UPDATE Accomodation SET status = 'cancelled' WHERE id = ?");
        stmt.bind(0, &bookingId);
        nanodbc::execute(stmt);

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR SQL] Anularea rezervarii a esuat in Service: " << e.what() << std::endl;
        return false;
    }
}