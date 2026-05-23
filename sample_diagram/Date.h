#pragma once

class Date {
private:
	int day;
	int month;
	int year;

public:
	Date(int day, int month, int year)
		: day(day), month(month), year(year) {
	}

	int getDay() const { return day; }
	int getMonth() const { return month; }
	int getYear() const { return year; }
};