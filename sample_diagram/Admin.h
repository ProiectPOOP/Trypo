#pragma once
#include "CUsers.h"
class Admin : public CUsers{
public:
	Admin(string name, string password, string mail, string phoneNumber): CUsers(name,password,mail,phoneNumber){}

	virtual std::string getBirthDate() const { return ""; }
	virtual std::string getCountry() const  { return ""; }
	virtual std::string getGender() const { return ""; }
	virtual std::string getAddress() const { return ""; }
	virtual float getBalance() const { return 0; }

};