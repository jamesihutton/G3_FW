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

uint8_t tpa::setGain(int8_t gain)
{
	if (gain > 30) gain = 30;
	if (gain < -28) gain = -28;
  
	Wire.beginTransmission(tpa_addr);
	Wire.write(0x05); //gain reg
	Wire.write(gain); //set gain 
	return (Wire.endTransmission());  //return 0 if ACK, 2 if NACK
}