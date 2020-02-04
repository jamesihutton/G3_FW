#include "Arduino.h"
#include "tpa2016.h"
#include "Wire.h"



uint8_t tpa::init()
{
	Wire.beginTransmission(tpa_addr);
	Wire.write(0x01); 
	Wire.write(B01000011); //turn off right speaker 
	return (Wire.endTransmission());  //return 0 if ACK, 2 if NACK
}

uint8_t tpa::setGain(int8_t x)
{

	if (x > 30) x = 30;
	if (x < -28) x = -28;
  
	Wire.beginTransmission(tpa_addr);
	Wire.write(0x05); //gain reg
	Wire.write(x); //set gain 
	return (Wire.endTransmission());  //return 0 if ACK, 2 if NACK
}


