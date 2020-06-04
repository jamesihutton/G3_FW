/*
	WRITTEN BY JAMES HUTTON
	2020 - 04 - 01  (no joke...)
*/
#ifndef main_h
#define main_h


#define 	ADC_PIN_USBVCC		0	
#define 	ADC_PIN_CHRG		1		
#define 	ADC_PIN_HPDET		2		
#define 	ADC_PIN_VCC			3	

#define 	ADC_PIN_DCHRG		4		//these aren't checked during normal opp
#define 	ADC_PIN_BRDID		5		
#define 	ADC_PIN_SOLAR		6			
#define 	ADC_PIN_NC			7		//not connected...		




void button_tick();
void track_tick();
int set_rad_chan(int chan); 
int set_rad_vol(int vol);
void displayInfo();
void updateLED();
float mapf(float x, float in_min, float in_max, float out_min, float out_max);
void readSD();
void updateTrackPos();
void jingle(int id, float gain);

uint32_t adc_get(int pin);
void adc_set(int pin);
#endif
