#pragma once
#include "Date.h"
class Date;

class ReservationTime {
private:
	Date* checkIn;
	Date* checkOut;

public:
	ReservationTime(Date* checkIn, Date* checkOut)
		: checkIn(checkIn), checkOut(checkOut) {
	}
	Date* getCheckIn() const { return checkIn; }
	Date* getCheckOut() const { return checkOut; }
};