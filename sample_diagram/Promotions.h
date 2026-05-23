#pragma once
class Promotions{
private:
	float discount;

public:
	Promotions(float discount) : discount(discount) {
	}
	float getDiscount() { return discount; }
	~Promotions(){}
};

