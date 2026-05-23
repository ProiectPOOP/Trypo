#pragma once
#include<string>
using namespace std;

class IUsers{
public:
    virtual int getId() const = 0;
    virtual void setId(int id) = 0;
    virtual std::string getName() const = 0;
    virtual std::string getPassword() const = 0;
    virtual std::string getMail() const = 0;
    virtual std::string getPhone() const = 0;
    virtual std::string getBirthDate() const = 0;
    virtual std::string getCountry() const = 0;
    virtual std::string getGender() const = 0;
    virtual std::string getAddress() const = 0;
    virtual float getBalance() const = 0;
	virtual ~IUsers(){}
};

