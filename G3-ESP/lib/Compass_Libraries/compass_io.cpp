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
	
	
	//SET RegDirB and RegDirA (pinModes)
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x0E); 
	Wire.write(pinDir >> 8);         
	Wire.write(pinDir & 0xFF);         
	Wire.endTransmission();
  Serial.print(pinDir>>8, BIN); Serial.print('\t'); Serial.println(pinDir & 0xFF, BIN);

  //enable pulldowns on inputs
  Wire.beginTransmission(SX1509_ADDR);
  Wire.write(0x08); 
  Wire.write(pinDir >> 8);         
  Wire.write(pinDir & 0xFF);         
  Wire.endTransmission();
  Serial.print(pinDir>>8, BIN); Serial.print('\t'); Serial.println(pinDir & 0xFF, BIN);
	
	//SET  RegDataB and RegDataA 
	Wire.beginTransmission(SX1509_ADDR);
	Wire.write(0x10); 
	Wire.write(pinState >> 8);         
	Wire.write(pinState & 0xFF);        
	Wire.endTransmission();	
	
	
	return 1;		//success
}

bool SX1509::OSCIO_set(bool state)
{
	if (state == 1){
		Wire.beginTransmission(SX1509_ADDR); 
		Wire.write(0x1E);  
		Wire.write(B01011111);         //set internal osc, set OSCIO to output, and HIGH    ///THIS KEEPS SUICIDE CIRCUIT ENABLED 
		Wire.endTransmission(); 
	} else {
		//power down LEDs first... (temporary need for proto3 they are hotwired to vbat...)
		/*digitalWrite(LED1, HIGH); pinMode(LED1, INPUT);  
		digitalWrite(LED2, HIGH); pinMode(LED2, INPUT);  
		digitalWrite(LED3, HIGH); pinMode(LED3, INPUT);  
		digitalWrite(LED4, HIGH); pinMode(LED4, INPUT);  */
		
		Wire.beginTransmission(SX1509_ADDR); 
		Wire.write(0x1E);  
		Wire.write(B01010000);         //kill power to everything (ESP suicide)
		Wire.endTransmission(); 
		delay(5000); 	//wait for suicide circuit cap to drain
		
		return (0); //<--- this should never be reached... if it returns, the power down failed...
	}

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

 Serial.print(pinDir>>8, BIN); Serial.print('\t'); Serial.print(pinDir & 0xFF, BIN);

  //If input, set to pull-downs to avoid need for resistors on PCB 
  if (dir == 1) {
    Wire.beginTransmission(SX1509_ADDR);
    Wire.write(0x08); 
    Wire.write(pinDir >> 8);         //these regs should match pinDir reg, since INPUT = 1, and pulldown = 1
    Wire.write(pinDir & 0xFF);         
    if(!Wire.endTransmission()) resp = 1;   //success
    Serial.print(pinDir>>8, BIN); Serial.print('\t'); Serial.print(pinDir & 0xFF, BIN);
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
