/*
	WRITTEN BY JAMES HUTTON 
	2020 - 04 - 01  (no joke...)
*/
#ifndef compass_io_h
#define compass_io_h

#include <Wire.h>
#include "compass.h"
#include <arduino.h>


#define			SX1509_ADDR		0X3E

#define			IO_OUTPUT			0
#define			IO_INPUT			1		//note, these are opposite to arduino's OUTPUT and INPUT.... be careful....

//----------------- PIN NAMES --------------------------------
#define			MUX_SEL				0
#define			USB_SD_RAD_RST		1
#define			SW_MODE				2
#define			SW_Q				3
#define			LED1				4
#define			LED2				5
#define			LED3				6
#define			LED4				7

#define			SW_VDOWN			8
#define			SW_VUP				9
#define			SW_DOWN				10
#define			SW_UP				11
#define			SW_RIGHT			12
#define			SW_LEFT				13
#define			SW_PLAY				14
#define			SW_POW				15
//-------------------------------------------------------------------



#define			DEFAULT_LEDMASK			0b0000000011110011			//LEDs on pins 4,5,6,7

#define			DEFAULT_INTMASK			0b0000000011110011			//0's enabled to interrupt

#define			DEFAULT_SENSE_B			0b0101010101010101			//sets the correct inputs to int on rising edge
#define			DEFAULT_SENSE_A			0b0000000001010000


#define			DEFAULT_PINDIR			0b1111111100001100			//set pins 0,1,4,5,6,7 as outputs
#define			DEFAULT_PINSTATE		0b0000000000000010			//LEDs all off (HIGH), !USB_SD_RAD_RST (HIGH), MUX_SEL (LOW)


class SX1509
{
private:


public:
	uint16_t pinDir = DEFAULT_PINDIR;					//direction of pins
	uint16_t pinState  = DEFAULT_PINSTATE;			//state of output pins
	uint16_t pinData;										//data of input pins
	
	bool init(void);
	bool OSCIO_set(bool state);								//the extra GPO pin used for the compass latch circuit
	void reset(void);
	
	bool pinMode(uint16_t pin, bool dir);
	bool get_pinMode(uint16_t pin);
	
	bool digitalWrite(uint16_t pin, bool dir);
	bool get_pinState(uint16_t pin);
	
	uint16_t get_pinState_raw();								//returns the whole 16bit integer
	
	uint16_t update_pinData();										//read all the input pins  to "pinData"
	
	bool digitalRead(uint16_t pin);							//simply returns the pinData for that pin...MUST UPDATE BEFORE READING!

	bool pwm(uint8_t led, uint8_t intensity);
};

typedef SX1509 sx1509Class;


#endif
