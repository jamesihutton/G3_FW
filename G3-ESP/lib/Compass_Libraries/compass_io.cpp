#include "compass_io.h"


bool SX1509::init(void)
{
	Wire.begin(SDA_PIN,SCL_PIN);
	
	
	//First check if able to comm with SX1509
	Wire.beginTransmission(SX1509_ADDR);
	if(Wire.endTransmission() != 0)		return 0;		//fail if returns anything other than 0
	
	
	//SOFTWARE RESET DEVICE (page 25)
	reset();	
	
	
	//SET  RegClock
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x1E); 
	Wire.write(0x40);         //set OSC to 0, set OSC AS input, internal 2MHz oscillator
	Wire.endTransmission();
	
	
	//SET  RegMisc
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x1F); 
	Wire.write(0x10);         //Sets CLK = fOSC, (needed for PWM), (set to 0x00 if PWM not needed)
	Wire.endTransmission();

	//SET RegInterruptMask
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x12); 
	Wire.write(DEFAULT_INTMASK >> 8);    	//sets all the inputs as interrupts
	Wire.write(DEFAULT_INTMASK & 0xFF); 

	Wire.write(DEFAULT_SENSE_B >> 8);    	//sets the correct inputs to int on rising edge
	Wire.write(DEFAULT_SENSE_B & 0xFF);  
	Wire.write(DEFAULT_SENSE_A >> 8);    	
	Wire.write(DEFAULT_SENSE_A & 0xFF);  
	Wire.endTransmission();

	//SET RegDebounce
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x23);
	Wire.write(0xFF);		//enable debounce for all inputs
	Wire.write(0xFF);
	Wire.endTransmission();
	
	
	//SET RegDirB and RegDirA (pinModes)
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x0E); 
	Wire.write(pinDir >> 8);         
	Wire.write(pinDir & 0xFF);         
	Wire.endTransmission();
	

	//enable pulldowns on inputs
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x08); 
	Wire.write(pinDir >> 8);         
	Wire.write(pinDir & 0xFF);         
	Wire.endTransmission();
	
	
	//SET  RegDataB and RegDataA 
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x10); 
	Wire.write(pinState >> 8);         
	Wire.write(pinState & 0xFF);        
	Wire.endTransmission();	

	//ENABLE LED DRIVERS 
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x20); 
	Wire.write(DEFAULT_LEDMASK >> 8);    	
	Wire.write(DEFAULT_LEDMASK & 0xFF);  
	Wire.endTransmission();	
	
	
	
	return 1;		//success
}

bool SX1509::OSCIO_set(bool state)
{
	int resp = 0;
	if (state == 1){
		Wire.beginTransmission(SX1509_ADDR); 
		Wire.write(0x1E);  
		Wire.write(B01011111);         //set internal osc, set OSCIO to output, and HIGH    ///THIS KEEPS SUICIDE CIRCUIT ENABLED 
		if(!Wire.endTransmission()) resp = 1;   //success
	} else {
		
		Wire.beginTransmission(SX1509_ADDR); 
		Wire.write(0x1E);  
		Wire.write(B01010000);         //kill power to everything (ESP suicide)
		if(!Wire.endTransmission()) resp = 1;   //success
	}
	return resp;

}

void SX1509::reset(void)
{
	//SOFTWARE RESET DEVICE (page 25)
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x7D); 
	Wire.write(0x12);       
	Wire.endTransmission();
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x7D); 
	Wire.write(0x34);      
	Wire.endTransmission();
	
}	
	
	
bool SX1509::pinMode(uint16_t pin, bool dir)
{
	//note! dir for SX1509 is opposite to arduino's OUTPUT and INPUT defines! 
	// 0 = SX1509 OUTPUT, 1 = SX1509 INPUT (PULL DOWN)
	int resp = 0;
	pinDir = (pinDir & ~(1<<pin)) | dir<<pin;	//update bit in pinState
	
	//SET RegDirB and RegDirA (pinModes)
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x0E); 
	Wire.write(pinDir >> 8);         
	Wire.write(pinDir & 0xFF);         
	if(!Wire.endTransmission()) resp = 1;   //success

 

  //If input, set to pull-downs to avoid need for resistors on PCB 
  if (dir == 1) {
    Wire.beginTransmission(SX1509_ADDR);
    Wire.write(0x08); 
    Wire.write(pinDir >> 8);         //these regs should match pinDir reg, since INPUT = 1, and pulldown = 1
    Wire.write(pinDir & 0xFF);         
    if(!Wire.endTransmission()) resp = 1;   //success

  }
	
	return resp;
}

bool SX1509::get_pinMode(uint16_t pin)
{
	return ((pinDir>>pin) & 1);
}


bool SX1509::digitalWrite(uint16_t pin, bool state)
{

	pinState = (pinState & ~(1<<pin)) | state<<pin;	//update bit in pinState
	
	//SET  RegDataB and RegDataA 
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x10); 
	Wire.write(pinState >> 8);         
	Wire.write(pinState & 0xFF);       
	if(!Wire.endTransmission())	return 1;		//success
	else return 0;
}

bool SX1509::get_pinState(uint16_t pin)
{
	return ((pinState>>pin) & 1);
}
	
uint16_t SX1509::get_pinState_raw()
{
	return pinState;
}


/*
NOTE:
This retrieves the current pinData as it is when read, not as it was when trigger happened!
Trigger from SX is simply meant as a "hey, read me" message to ESP
*/
uint16_t SX1509::update_pinData()
{
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x10);	//point to RegDataB
	if(Wire.endTransmission() != 0)		return 0;
	
	Wire.requestFrom(SX1509_ADDR, (byte) 2);
	byte b = Wire.read();
	byte a = Wire.read();
	pinData  = (b<<8) | a;
	return pinData;
}

bool SX1509::digitalRead(uint16_t pin)
{
	return ((pinData>>pin) & 1);
}


bool SX1509::pwm(uint8_t led, uint8_t intensity)
{
	if ((led < 0) || (led > 4)) return 0;
	
	Wire.beginTransmission(SX1509_ADDR);
	switch(led)
	{
		case 4:
			Wire.write(0x45);
			break;
		case 3:
			Wire.write(0x40);
			break;
		case 2:
			Wire.write(0x3B);
			break;
		case 1:
			Wire.write(0x36);
			break;
	}

	Wire.write(intensity);         
	if(!Wire.endTransmission())	return 1;		//success
	else return 0;
}


void SX1509::setAllLEDs(uint8_t value)
{
	SX1509::digitalWrite(LED1, value);
	SX1509::digitalWrite(LED2, value);
	SX1509::digitalWrite(LED3, value);
	SX1509::digitalWrite(LED4, value);
}