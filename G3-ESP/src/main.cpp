

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include "SPIFFS.h"
#else
  #include <ESP8266WiFi.h>
#endif



#include "main.h"
#include "compass.h" 
#include "compass_io.h" 
#include "compass_nonVols.h"
#include "compass_jingles.h"



#include <Wire.h>

#include <twi.h>
#include <SPI.h>
#include <SD.h>
// You may need a fast SD card. Set this as high as it will work (40MHz max).
#define SPI_SPEED   SD_SCK_MHZ(40)
#define SD_CS       D0


#include "string.h"
File root;


//////////////////////////////////////////////////////////////////////////////
#include <Wire.h>
#include "compass_io.h" // Include SX1509 library
SX1509 io; // Create an SX1509 object to be used throughout
//////////////////////////////////////////////////////////////////////////////

nonVol nv;


#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorWAV *wav;
AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;



static const int fm_addr = B1100011;   //i2c address for si4734
int SDIO = D2;
int SCLK = D1;



int channel = 9470;
#define fm_max 10790
#define fm_min  6410
char rdsBuffer[10];

#define TRACK_MAX_GAIN    0.75   //
#define TRACK_MIN_GAIN    0.01   //

#define RADIO_MAX_GAIN    63    
#define RADIO_MIN_GAIN    30

#define MAX_DEVICE_VOL    15    //used for both Radio and MP3 nv.deviceVolume!
float track_gain = 0.3;     //starting level





String track_name[100];
int track_count = 0;


String folder_name[70];
int folder_count = 0;


bool track_initialized = 0; //only happens first time

bool track_play = true;
bool radio_play = false;



#define TRACK_MODE 0
#define RADIO_MODE 1



void listFolders()
{
  root = SD.open("/");

  int i = 0;
  while(1)
  {
    File entry = root.openNextFile();
     if (! entry) break;

    //check if folder
    if (entry.isDirectory())
    {
      String s = entry.name();
      if (!(s.equals("System Volume Information"))) {  //filter out "System Volume Information" file...
        folder_name[i] = "/" + s + "/";
        i++;
        if (i>=99) break;
      }
    }
    entry.close();
  }
  folder_count = i;
  Serial.print("FOLDER COUNT: ");
  Serial.println(i);

  //BUBBLE SORT
  //1. assign folders a number value
  int fold_dec[folder_count];

  for (int i = 0; i<folder_count; i++){
    //extract folder number into "fold_dec[i]"
    if (isdigit(folder_name[i][1])){
      fold_dec[i] = 0;
      for (int x = 1; isdigit(folder_name[i][x]); x++){
        fold_dec[i]*=10;
        fold_dec[i]+=folder_name[i][x] - '0';
      }
    } else {
      fold_dec[i] = 99; //stick any folder with no number at the end
    }
  }
  //2. sort
  String s_tmp;
  int d_tmp, a = 0, j = 0;
  for (a = 0; a<folder_count; a++){
    for (j = 0; j < folder_count-a-1; j++){
      if (fold_dec[j] > fold_dec[j+1]) {
        //swap dec
        d_tmp = fold_dec[j];
        fold_dec[j] = fold_dec[j+1];
        fold_dec[j+1] = d_tmp;
        //swap string
        s_tmp = folder_name[j];
        folder_name[j] = folder_name[j+1];
        folder_name[j+1] = s_tmp;
      }
    }
  }
}


void listFiles()
{
  root = SD.open(folder_name[nv.folderIndex
  ]);

  int i = 0;
  while(1)
  {
    File entry = root.openNextFile();
     if (! entry) break;
    String s = entry.name();
    //Serial.println(s);
    //check if mp3 file
    if (((s[s.length()-4] == '.') && (s[s.length()-3] == 'm') && (s[s.length()-2] == 'p') && (s[s.length()-1] == '3'))
    ||((s[s.length()-4] == '.') && (s[s.length()-3] == 'w') && (s[s.length()-2] == 'a') && (s[s.length()-1] == 'v')))
    {
      track_name[i] = s;
      i++;
      if (i>=99) break;
    }
    entry.close();
  }
  track_count = i;
}

void init_track()
{
  if (track_initialized){
    if (mp3->isRunning()) {
      mp3->stop();
    } else if (wav->isRunning()) {
      wav->stop();
    }
  }
  char s[100] = "";

  int i;
  for (i = 0; i<100; i++)
  {
    s[i] = folder_name[nv.folderIndex][i];
    if (!(folder_name[nv.folderIndex][i])) break;
  }
  int j = 0;
  for (i = i; i<100; i++)
  {
    s[i] = track_name[nv.trackIndex][j];
    if (!(track_name[nv.trackIndex][j])) break;
    j++;
  }

  Serial.print("PLAYING FILE: ");
  Serial.print(s);
  
  if ((s[i-4] == '.') && (s[i-3] == 'm') && (s[i-2] == 'p') && (s[i-1] == '3')){
    Serial.println("\nLoading MP3 codec...");
    audioLogger = &Serial;
    file = new AudioFileSourceSD(s);
    id3 = new AudioFileSourceID3(file);
    out = new AudioOutputI2S();
    mp3 = new AudioGeneratorMP3();
    mp3->begin(id3,out);
    
  }
  else if ((s[i-4] == '.') && (s[i-3] == 'w') && (s[i-2] == 'a') && (s[i-1] == 'v')){
    Serial.println("\nLoading WAV codec...");
    audioLogger = &Serial;
    file = new AudioFileSourceSD(s);
    id3 = new AudioFileSourceID3(file);
    out = new AudioOutputI2S();
    wav = new AudioGeneratorWAV();
    wav->begin(id3, out);
  }


  file->seek(nv.trackFrame, SEEK_SET);
  Serial.println("@FRAME: "); Serial.println(nv.trackFrame);
  track_initialized = true; //raised forever after first init


  track_gain = mapf(nv.deviceVolume, 0, MAX_DEVICE_VOL, TRACK_MIN_GAIN, TRACK_MAX_GAIN);
  out->SetGain(track_gain);
  Serial.print("\ngain = ");
  Serial.println(track_gain);

  
  //nv.set_nonVols(); //not sure if I want to update every time... only on power down?
}

void init_radio()
{

  //RESET radio
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);

  Wire.begin(SDIO, SCLK);  //SDA, SCL
  delay(200); //remove?

  //POINT TO ADDRESS TO READ FROM
  Wire.beginTransmission(fm_addr);
  Wire.write(B00000001);  //0x01  CMD: POWERUP
  Wire.write(B00010000);  //0x10 //external crystal
  Wire.write(0x05);  //0x05
  Wire.endTransmission();
  delay(200);

  
  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  //0x12
  Wire.write(0x00);  //0x00
  Wire.write(0x13);  //0x13  // FM_SOFT_MUTE_MAX_ATTENUATION
  Wire.write(0x02);  //0x02
  Wire.write(0x00);  //0x00
  Wire.write(0x00);  //0x03   //3dB
  Wire.endTransmission();
  delay(200);

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  //0x12
  Wire.write(0x00);  //0x00
  Wire.write(0xFF);  //0xFF  // ***DISABLE DEBUG MODE!! PAGE 299!!!
  Wire.write(0x00);  //0x00
  Wire.write(0x00);  //0x00
  Wire.write(0x00);  //0x00
  Wire.endTransmission();
  delay(200);

  set_rad_vol(nv.deviceVolume);
  set_rad_chan(channel);

  radio_play = true;

}

int set_rad_vol(int vol)
{
  int vhex;
  if (vol < 0) vhex = 0;
  else vhex = map(vol,0,MAX_DEVICE_VOL,RADIO_MIN_GAIN,RADIO_MAX_GAIN); 
  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x40);  // RX volume
  Wire.write(0x00);  
  Wire.write(0x00);  
  Wire.write(vhex);  // 0-63
  Wire.endTransmission();
  delay(200);
  Serial.print("\nRadio volume set to ");
  Serial.println(vhex);
}

int set_rad_chan(int chan) 
{
  if (chan < fm_min) return(0);
  if (chan > fm_max) return(0);
  Wire.beginTransmission(fm_addr);
  Wire.write(0x20);  
  Wire.write(0x00);  
  Wire.write(chan >> 8);  // channel
  Wire.write(chan & 0x00ff);  
  Wire.write(0x00);  
  Wire.write(0x00);  
  int resp = Wire.endTransmission();
  delay(200);
  Serial.print("Set to channel: ");
  Serial.println(chan);
  return (!resp);
}


void switch_mode_radio()
{
  nv.trackFrame = file->getPos();
  if (mp3->isRunning()) {
    mp3->stop();
  } else if (wav->isRunning()) {
    wav->stop();
  }
  SD.end();

  init_radio();
  track_play = false;
  nv.deviceMode = RADIO_MODE;
}

void switch_mode_track()
{
  //RESET radio to disable
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);
  delay(100); //remove?
  SD.begin(SD_CS, SPI_SPEED); 
  delay(100);
  nv.deviceMode = TRACK_MODE;
  nv.trackFrame = 0;
  track_play = true;
  init_track();
  
}



//LATCHES SUICIDE CIRCUIT BUY SETTING SX1509 OSCIO PIN HIGH
void latchPower()
{
   //latch power
  io.OSCIO_set(HIGH);
  io.OSCIO_set(HIGH);
  io.OSCIO_set(HIGH);   //5 times for redundancy... ¯\_(ツ)_/¯
  io.OSCIO_set(HIGH);
  io.OSCIO_set(HIGH); 
  Serial.println("power latched");
}

//DE-LATCHES SUICIDE CIRCUIT BUY SETTING SX1509 OSCIO PIN LOW
void powerDown()
{
  Serial.println("turning off power...");
  io.OSCIO_set(LOW);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600); Serial.println("\n\nboot\n\n");
  delay(100);
  
  // Set pinModes
  io.init();
  pinMode(D3, INPUT); //!IO_INT pin as input
  pinMode(D0, OUTPUT);
  digitalWrite(D0, 0);


  latchPower();
  Serial.println("power latchedx");
  delay(100);


  
  Serial.println("power - up jingle");
  //play power up jingle
  jingle(JINGLE_POWER_UP, DEFAULT_JINGLE_GAIN);       //takes ~2 seconds



  io.update_pinData();
  while(io.digitalRead(SW_POW)){  //wait until user releases SW_POW to proceed
    io.update_pinData();
    delay(100);
  }

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin(); //<-- saves like 100mA!

  //RESET SD card
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);


  int resp = SD.begin(SD_CS, SPI_SPEED); 
  //int resp = SD.begin(D0); 
  if (!resp) {
    while(1){
      Serial.println("\n\nCould not connect to SD card\n\n");
     io.digitalWrite(LED1, 1);io.digitalWrite(LED2, 1);io.digitalWrite(LED3, 1);io.digitalWrite(LED4, 1);
     delay(400);
     io.digitalWrite(LED1, 0);io.digitalWrite(LED2, 0);io.digitalWrite(LED3, 0);io.digitalWrite(LED4, 0);
     delay(400);
    }
  }

    

  //UPDATE NONVOLS
  //Initialize File System
  if(SPIFFS.begin())  Serial.println("SPIFFS Initialize....ok");
  else                Serial.println("SPIFFS Initialization...failed");
 
  //Format File System
  if(io.digitalRead(SW_Q)){
    if(SPIFFS.format()) Serial.println("File System Formated");
    else                Serial.println("File System Formatting Error");
  }

  
  

  nv.get_nonVols();
  
  nv.deviceMode = TRACK_MODE; //force track mode on startup for now...

  listFolders();
  listFiles();

   

  if (nv.deviceMode == TRACK_MODE) {
    init_track();
  } else if (nv.deviceMode == RADIO_MODE) {
    init_radio();
    Serial.println("radio inited");
  }

  updateLED();
}


int ms = 0;
int last_ms = 0;
void loop()
{while(1){
  //feed WDT
  ESP.wdtFeed();

  //CONSTANT TICK HANDLES
  track_tick();

  //MS TICK HANDLES...
  ms = millis();
  if (ms == last_ms) continue;
  last_ms = ms;

  if (!(ms%5)){
      button_tick();
  }

  if (!(ms%100)){
    adc_set(ADC_PIN_USBVCC);
    Serial.println(adc_get(ADC_PIN_VCC));
  }
}}

void track_tick()
{
  //check if track has finished
  if (track_play){
    if (mp3->isRunning()) {
      if (!mp3->loop()) { //play next file
        nv.trackIndex++;
        nv.trackFrame=0;
        if (nv.trackIndex >= (track_count)) nv.trackIndex = 0;  //loop back to 0 after last song
        init_track();
      }
    } else if (wav->isRunning()){
        if (!wav->loop()) { //play next file
        nv.trackIndex++;
        nv.trackFrame=0;
        if (nv.trackIndex >= (track_count)) nv.trackIndex = 0;  //loop back to 0 after last song
        init_track();
      }
    }
    else {init_track();}
  }
}

void button_tick()
{
  if(!digitalRead(D3)){ //if !IO_INT is triggered... read buttons

      Serial.println("button pressed");
      io.update_pinData();    //this must be called before reading any pins! (reads them all at once in one command...)
      if(io.digitalRead(SW_VUP)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.deviceVolume ++;
          if (nv.deviceVolume > MAX_DEVICE_VOL) nv.deviceVolume = MAX_DEVICE_VOL;
          track_gain = mapf(nv.deviceVolume, 0, MAX_DEVICE_VOL, 0, TRACK_MAX_GAIN);
          /*
          nv.trackFrame = file->getPos();
          jingle(JINGLE_TICK, track_gain); //play the tick sound  
          init_track();
          */
          out->SetGain(track_gain);
          Serial.print(nv.deviceVolume); Serial.print(" ("); Serial.print(track_gain); Serial.println(")");
          updateLED();
        } else if (nv.deviceMode == RADIO_MODE) {
          nv.deviceVolume ++;
          if (nv.deviceVolume == 16) nv.deviceVolume = 15;

          set_rad_vol(nv.deviceVolume);
          displayInfo();
          updateLED();
        }


      }
      if(io.digitalRead(SW_VDOWN)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.deviceVolume --;
          if (nv.deviceVolume < 0) nv.deviceVolume = 0;
          track_gain = mapf(nv.deviceVolume, 0, MAX_DEVICE_VOL, 0, TRACK_MAX_GAIN);
          /*
          nv.trackFrame = file->getPos();
          jingle(JINGLE_TICK, track_gain); //play the tick sound
          init_track();
          */

          out->SetGain(track_gain);
          Serial.print(nv.deviceVolume); Serial.print(" ("); Serial.print(track_gain); Serial.println(")");
          updateLED();
        } else if (nv.deviceMode == RADIO_MODE) {
          nv.deviceVolume --;
          if (nv.deviceVolume < 0) nv.deviceVolume = 0;
          set_rad_vol(nv.deviceVolume);
          displayInfo();
          updateLED();
        }

      }
      if(io.digitalRead(SW_RIGHT)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.trackIndex++;
          if (nv.trackIndex >= (track_count)) nv.trackIndex = 0;  //loop back to 0 after last song
          nv.trackFrame = 0;
          init_track();
        } else if (nv.deviceMode == RADIO_MODE) {
          channel += 20;
          if (channel > fm_max+1) channel = fm_min;
          set_rad_chan(channel);
          displayInfo();
        }

      }
      if(io.digitalRead(SW_LEFT)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.trackIndex--; 
          if (nv.trackIndex < 0) nv.trackIndex = track_count-1;  //loop to last song
          nv.trackFrame = 0;
          init_track();
        } else if (nv.deviceMode == RADIO_MODE) {
          channel -= 20;
          if (channel < fm_min-1) channel = fm_max;
          set_rad_chan(channel);
          displayInfo();
        }

      }
      if(io.digitalRead(SW_UP)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.folderIndex
          --;
          if (nv.folderIndex
          < 0) nv.folderIndex
          = folder_count-1;  //loop back to 0 after last folder
          listFiles();
          nv.trackIndex = 0;
          nv.trackFrame = 0;
          init_track();
        } else if (nv.deviceMode == RADIO_MODE) {
          
          displayInfo();
          updateLED();
        }

      }
      if(io.digitalRead(SW_DOWN)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.folderIndex
          ++;
          if (nv.folderIndex
          >= (folder_count)) nv.folderIndex
          = 0;  //loop back to 0 after last folder
          listFiles();
          nv.trackIndex = 0;
          nv.trackFrame = 0;
          init_track();
        } else if (nv.deviceMode == RADIO_MODE) {
          
          displayInfo();
          updateLED();
        }
      }
      if(io.digitalRead(SW_MODE)){
        if (nv.deviceMode == TRACK_MODE) {
          switch_mode_radio();
        } else if (nv.deviceMode == RADIO_MODE) {
          switch_mode_track();
        }
        Serial.println(nv.deviceMode);
      }

      if(io.digitalRead(SW_PLAY)){
        if (nv.deviceMode == TRACK_MODE) {
            if (track_play) track_play = false;
            else track_play = true;
        }else if (nv.deviceMode == RADIO_MODE) {
          if (radio_play){
            set_rad_vol(-1);
            radio_play = false;
          } else {
            set_rad_vol(nv.deviceVolume);
            radio_play = true;
          }
        }

      }

      if(io.digitalRead(SW_Q)){
        
        //update track frame last second...
        nv.trackFrame = file->getPos();

        //set all the params in nonVol memory 
        nv.set_nonVols();

        //safely end SPIFFS
        SPIFFS.end();
        
        readSD();
        
        
      }

      if(io.digitalRead(SW_POW)){
        //update track frame last second...
        nv.trackFrame = file->getPos();

        //set all the params in nonVol memory 
        nv.set_nonVols();

        //safely end SPIFFS
        SPIFFS.end();

        //play power down jingle
        jingle(JINGLE_POWER_DOWN, DEFAULT_JINGLE_GAIN);       //takes ~2 seconds
        
        //power down the entire board
        while(1){
          powerDown();
          delay(100);
        }
      }
  }
}


//never leaves this function
void readSD()
{
  io.update_pinData();
  while(io.digitalRead(SW_Q)){  //wait until user releases SW_Q to proceed
    io.update_pinData();
    delay(100);
  }

  Serial.println("switching to SD card...");

  //switch ESP SD pins to high impedance
  pinMode(D0, INPUT);
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);

  //orient USB MUX from serial to SD
  io.digitalWrite(MUX_SEL, HIGH);
  //RESET SD card and SD reader
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);

  //loop until SW_POW is pressed (should reset ESP though when MUX_SEL goes low and triggers FTDI...)
  while(1){
    delay(100);
    io.update_pinData();
    if (io.digitalRead(SW_Q)){
      io.digitalWrite(MUX_SEL, LOW);    //this will reset the ESP...
      //RESET SD card and SD reader
      io.digitalWrite(USB_SD_RAD_RST, LOW);   //not reached...
      delay(100);
      io.digitalWrite(USB_SD_RAD_RST, HIGH);
      return;
    }
  }
}


//remember, LEDs are reversed (0 = on)
void updateLED(){
  io.digitalWrite(LED1, 1);io.digitalWrite(LED2, 1);io.digitalWrite(LED3, 1);io.digitalWrite(LED4, 1);
  if (nv.deviceVolume>=1) {
    io.digitalWrite(LED1, 0);
  }
  if (nv.deviceVolume>=4) {
    io.digitalWrite(LED2, 0);
  }
  if (nv.deviceVolume>=8) {
    io.digitalWrite(LED3, 0);
  }
  if (nv.deviceVolume>=12) {
    io.digitalWrite(LED4, 0);
  }
}

void displayInfo()
{
   Serial.print("Channel:"); Serial.print(channel);
   Serial.print("Volume:"); Serial.println(nv.deviceVolume);
}


float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


AudioGeneratorWAV *wav_progmem;
AudioFileSourcePROGMEM *file_progmem;
AudioOutputI2S *out_progmem;

void jingle(int id, float gain)
{
    audioLogger = &Serial;
    switch(id)
    {
        case JINGLE_POWER_UP:
            file_progmem = new AudioFileSourcePROGMEM(power_up, sizeof(power_up));
            delay(300); //engine needs to warm up a bit...
            break;

        case JINGLE_POWER_DOWN:
            file_progmem = new AudioFileSourcePROGMEM(power_down, sizeof(power_down));
            break;
        
        case JINGLE_TICK:
            file_progmem = new AudioFileSourcePROGMEM(tick, sizeof(tick));
            break;
    }
    
    out_progmem = new AudioOutputI2S();
    wav_progmem = new AudioGeneratorWAV();
    wav_progmem->begin(file_progmem, out_progmem);
    out_progmem->SetGain(gain);
    while(1){
        if (wav_progmem->isRunning()){
            if (!wav_progmem->loop()){
            wav_progmem->stop();
            return;
            } 
        }
        /*
        if (track_play && (id == JINGLE_TICK)){
          if (wav->isRunning()) wav->loop();
          if (mp3->isRunning()) mp3->loop();
        }*/


        ESP.wdtFeed();
    }
}


/*
    NOTES:
    -add delay between set and get to let line settle
    -Never read from 4-7 when expecting a button press

*/
#include <core_esp8266_si2c.cpp>

ADC_MODE(ADC_TOUT);
#define ADC_SAMPLE_COUNT    10  //read and average this many samples each time
#define ADC_CEILING_MV      972  //mv that corresponds to 1023 in the adc
void adc_set(int pin)
{
    
  SCL_LOW(SCLK);
  (!!(pin & 0b0001)) ? (SDA_HIGH(SCLK)) : (SDA_LOW(SCLK));
  (!!(pin & 0b0010)) ? (SCL_HIGH(SCLK)) : (SCL_LOW(SCLK));

  //digitalWrite(SDA, !!(pin & 0b0001));
  //digitalWrite(SCL, !!(pin & 0b0010));
  //digitalWrite(D0,  !!(pin & 0b0100));
  //digitalWrite(D0, 0);
  //Wire.begin(SDA, SCL);
}

//must always first be set
//returns mapped value in mv depending on pin being read
uint32_t adc_get(int pin)
{
  uint32_t adc = 0;

  //average 1000 readings
  for (int i = 0; i<ADC_SAMPLE_COUNT; i++){
    adc += analogRead(A0);
  } 
  adc /= ADC_SAMPLE_COUNT;

  //convert to mv
  adc = map(adc, 0, 1023, 0, ADC_CEILING_MV); 

  //correct for vdivs for whatever pin being read
  switch(pin)
  {
    case ADC_PIN_USBVCC:

      break;
    
    case ADC_PIN_CHRG:

      break;
    
    case ADC_PIN_HPDET:

      break;
    
    case ADC_PIN_VCC:
      return(adc*4);    // 1/4vdiv
      break;
  
  }
}

