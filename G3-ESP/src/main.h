/*
	WRITTEN BY JAMES HUTTON
	2020 - 04 - 01  (no joke...)
*/
#ifndef main_h
#define main_h

#include <Arduino.h>

#define 	ADC_PIN_USBVCC		0	
#define 	ADC_PIN_CHRG		1		
#define 	ADC_PIN_HPDET		2		
#define 	ADC_PIN_VCC			3	

#define 	ADC_PIN_DCHRG		4		//these aren't checked during normal opp
#define 	ADC_PIN_BRDID		5		
#define 	ADC_PIN_SOLAR		6			
#define 	ADC_PIN_NC			7		//not connected...		



//CONSTANTS:
#define 		LV_THRESH				2700	//under 2700mv, the device will power down
#define			LV_CHECK_INTERVAL		10000	//check for low voltage every 10 seconds
#define			LV_WARN_THRESH			3000	//Should have ~10 mins runtime left at this threshold...
#define			LV_WARN_INTERVAL		120000	//LV warning beep every 2 mins	(ensure this is a multiple of the check interval!)

#define			RADIO_SLEEP_INTERVAL	120000	//2 miniute sleep interval for radio	








bool latchPower();
void button_tick();

bool init_radio();
int rad_seek(bool dir);
void track_tick();
int set_rad_chan(int chan); 
int set_rad_vol(int vol);
void print_rad_info();
int powerdown_radio();
void displayInfo();
void updateLED();
float mapf(float x, float in_min, float in_max, float out_min, float out_max);
void readSD();
void updateTrackPos();
void jingle(int id, float gain);

uint32_t adc_get(int pin);
void adc_set(int pin);
void adc_print_all();
void handle_wakeup();
void charging_loop();
uint8_t vccToPercent(int vcc);

void radio_sleep_tick();
void wakeup_cb();

bool LV_check();
void LV_handle();
void adc_settle();
#endif
