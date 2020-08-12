#ifndef CPSETTINGS_H
#define CPSETTINGS_H


#define SPI_SPEED   SD_SCK_MHZ(40)
#define SD_CS       17    //a non-existent pin 

#define fm_addr 0x11   //i2c address for si4734 (0x63 for SEN = HIGH, 0x11 for SEN = LOW)
#define SDIO 4
#define SCLK 5

#define MUTE_PIN 16
#define MUTE          1
#define UNMUTE        0
#define MUTE_MS       0   //the amount of ms to mute into each track (to avoid "click")
#define PRE_MUTE_MS   100 //the amount of ms to mute BEFORE each track

#define fm_max 10790
#define fm_min  8790
//#define fm_min  6410  //can go lower than general fm channels on fm mode...

#define TRACK_MAX_GAIN    1.5   //
#define TRACK_MIN_GAIN    0.01   //

#define RADIO_MAX_GAIN    63    
#define RADIO_MIN_GAIN    25

#define MAX_DEVICE_VOL    15    //used for both Radio and MP3 nv.deviceVolume!
#define MIN_DEVICE_VOL    1

#define TRACK_MODE 0
#define RADIO_MODE 1

#define   MUTE_RADIO_MS   1  //amount of ms to mute amp when switching to radio mode (to avoid pop)


#endif