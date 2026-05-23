#pragma once
#include<string>
#include<vector>
#include "Promotions.h"
using namespace std;
class IRoom;
class Promotions;

class RentalUnit {
private:
	int id; // static
	string name;
	string address;
	string location;
	int capacity;
	vector<IRoom*> rooms;
	Promotions* promotion;
	string mailAdmin;

public:
	RentalUnit(string name, string location, string address, int capacity, float promotion,string idAdmin) : name(name), location(location),address(address), capacity(capacity), mailAdmin(idAdmin) {
		this->promotion = new Promotions(promotion);
	}
	RentalUnit(int i,string name, string location, string address, int capacity, float promotion,string idAdmin) :id(i), name(name),location(location), address(address), capacity(capacity), mailAdmin(idAdmin) {
		this->promotion = new Promotions(promotion);
	}
	~RentalUnit() {
		rooms.clear();
	}
	void addRoom(IRoom* r) {
		this->rooms.push_back(r);
	}

	int getId()const { return id; }
	string getName() const { return name;  }
	string getAddress()const { return address; }
	string getLocation()const { return location; }
	string getAdmin()const { return mailAdmin; }
	Promotions* getPromotion()const { return promotion; }
	vector<IRoom*> getRooms()const { return this->rooms; }
	float getDiscount() const {
		if (promotion != nullptr) {
			return promotion->getDiscount();
		}
		return 0.0f;
	}
};

