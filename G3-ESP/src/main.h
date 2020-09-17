/*
	WRITTEN BY JAMES HUTTON
	2020 - 04 - 01  (no joke...)
*/
#ifndef main_h
#define main_h

#include <Arduino.h>

/////////////////////////////////
#define 	FW_REV				"2020-09-17"

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
#define			LV_WARN_INTERVAL		60000	//LV warning beep every 2 mins	(ensure this is a multiple of the check interval!)

#define			RADIO_SLEEP_INTERVAL	60000	//1 miniute sleep interval for radio	

#define 		LED_FADEOUT_TIME		60000	//to save power. Only affects track mode (for radio, this will just = sleep interval)	

#define			PAUSED_POWER_DOWN_TIMER	240000	//turn off after this much time when paused

#define 		initial_press_interval	500
#define 		press_interval			200		//skip forward every 200ms held down
#define			skip_bytes				120000	//bytes in file to skip each skip

bool latchPower();
void button_tick();
void vup();
void vdown();


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
float track_gain_convert();
int radio_gain_convert();
void pause_powerdown_check();
void quiet_power_down();


#endif
