





#ifndef tpa2016
#define tpa2016


#include "Arduino.h"


class tpa
{
	public:
		uint8_t init();
		uint8_t setGain(int8_t gain);
	
	
	private:
		
		
		static const int tpa_addr = B1011000;
		
};



#endif