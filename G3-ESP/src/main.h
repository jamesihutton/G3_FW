/*
	WRITTEN BY JAMES HUTTON
	2020 - 04 - 01  (no joke...)
*/
#ifndef main_h
#define main_h

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
#endif
