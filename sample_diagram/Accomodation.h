#pragma once
#include <vector>
#include "ReservationTime.h"
class IUsers;
class IRoom;
class ReservationTime;

typedef enum status {
	cancelled, confirmed, finished
} status;

class Accomodation {
private:
	int accomodationId;
	int clientId;
	IUsers* user;
	IRoom* rentedRoom;
	status accomodationStatus;
	ReservationTime* resTime;

public:
	Accomodation()
		: accomodationId(-1), clientId(-1), user(nullptr), rentedRoom(nullptr),
		accomodationStatus(confirmed), resTime(nullptr) {
	}

	Accomodation(int accId, int cId, IUsers* u, IRoom* room, status st, ReservationTime* time)
		: accomodationId(accId), clientId(cId), user(u), rentedRoom(room),
		accomodationStatus(st), resTime(time) {
	}
	~Accomodation() {
	}

	int getAccomodationId() const { return accomodationId; }
	int getClientId() const { return clientId; }
	IUsers* getUser() const { return user; }
	IRoom* getRentedRoom() const { return rentedRoom; }
	status getAccomodationStatus() const { return accomodationStatus; }
	ReservationTime* getReservationTime() const { return resTime; }

	void setAccomodationStatus(status newStatus) { accomodationStatus = newStatus; }
	void setReservationTime(ReservationTime* time) { resTime = time; }
};
