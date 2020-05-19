
#include "main.h"
#include "compass.h" 
#include "compass_io.h" 
#include "compass_nonVols.h"

#include "SparkFunSi4703.h"




#include <SPI.h>
#include <SD.h>
// You may need a fast SD card. Set this as high as it will work (40MHz max).
//#define SPI_SPEED   SD_SCK_MHZ(40)

#include "string.h"
File root;


//////////////////////////////////////////////////////////////////////////////
#include <Wire.h>
#include "compass_io.h" // Include SX1509 library
SX1509 io; // Create an SX1509 object to be used throughout
//////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include "SPIFFS.h"
#else
  #include <ESP8266WiFi.h>
#endif



#include "AudioFileSourceSD.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;


#define fm_addr  0x10

int resetPin = D0;  //GPIO16 HOTWIRE
int SDIO = D2;
int SCLK = D1;



Si4703_Breakout radio(resetPin, SDIO, SCLK);
int channel = 947;
int volume = 5;
char rdsBuffer[10];

#define MP3_MAX_GAIN    1.0
#define MAX_VOLUME      15    //used for both Radio and MP3 volume!
float mp3_gain = 0.3;     //starting level





String mp3_name[100];
int mp3_count = 0;
int mp3_index = 0;

String folder_name[70];
int folder_count = 0;
int folder_index = 0;

bool mp3_initialized = 0; //only happens first time

bool mp3_play = true;
bool radio_play = false;



#define MP3_MODE 0
#define RADIO_MODE 1
bool device_mode = MP3_MODE;

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
  root = SD.open(folder_name[folder_index]);

  int i = 0;
  while(1)
  {
    File entry = root.openNextFile();
     if (! entry) break;
    String s = entry.name();
    //Serial.println(s);
    //check if mp3 file
    if ((s[s.length()-4] == '.') && (s[s.length()-3] == 'm') && (s[s.length()-2] == 'p') && (s[s.length()-1] == '3'))
    {
      mp3_name[i] = s;
      i++;
      if (i>=99) break;
    }
    entry.close();
  }
  mp3_count = i;
}

void init_mp3()
{
  if (mp3_initialized){
    if (mp3->isRunning()) {
      mp3->stop();
    }
  }
  char s[100] = "";

  int i;
  for (i = 0; i<100; i++)
  {
    s[i] = folder_name[folder_index][i];
    if (!(folder_name[folder_index][i])) break;
  }
  int j = 0;
  for (i = i; i<100; i++)
  {
    s[i] = mp3_name[mp3_index][j];
    if (!(mp3_name[mp3_index][j])) break;
    j++;
  }

  Serial.print("PLAYING FILE: ");
  Serial.print(s);
  audioLogger = &Serial;  //not needed
  file = new AudioFileSourceSD(s);
  id3 = new AudioFileSourceID3(file);
  out = new AudioOutputI2S();
  mp3 = new AudioGeneratorMP3();
  mp3_initialized = true; //raised forever after first init
  mp3->begin(id3,out);
  Serial.print("\t\t@"); Serial.println(nonVol[TRACKPOS]);
  file->seek(nonVol[TRACKPOS], SEEK_SET);
  out->SetGain(mp3_gain);
  Serial.print("gain = ");
  Serial.println(mp3_gain);
}

void init_radio()
{
  radio.powerOn();
  radio.setVolume(volume);
  radio.setChannel(channel);
  displayInfo();
  updateLED();
}

void switch_mode_radio()
{
  if (mp3_initialized){
    if (mp3->isRunning()) {
      mp3->stop();
    }
  }

  init_radio();
  mp3_play = false;
  device_mode = RADIO_MODE;

}

void switch_mode_mp3()
{
  digitalWrite(resetPin,LOW); //put radio into reset mode (disable it)
  init_mp3();
  device_mode = MP3_MODE;
}



//LATCHES SUICIDE CIRCUIT BUY SETTING SX1509 OSCIO PIN HIGH
void latchPower()
{
   //latch power
  io.OSCIO_set(HIGH);
  Serial.println("power latched");
}

//DE-LATCHES SUICIDE CIRCUIT BUY SETTING SX1509 OSCIO PIN LOW
void powerDown()
{
  Serial.println("turning off power...");

  io.OSCIO_set(LOW);
}



void setup()
{
  Serial.begin(9600); Serial.println(); Serial.println("boot");
  delay(100);
  // Set pinModes
  io.init();

  latchPower();


  io.update_pinData();
  while(io.digitalRead(SW_POW)){  //wait until user releases SW_POW to proceed
    io.update_pinData();
    delay(100);
  }

  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin(); //<-- saves like 100mA!

  //RESET SD card
  io.digitalWrite(USB_SD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RST, HIGH);

  SD.begin(D0);

  //UPDATE NONVOLS
  SPIFFS.begin();
  get_nonVols();

  listFolders();
  listFiles();

  if (device_mode == MP3_MODE) {
    init_mp3();
  } else if (device_mode == RADIO_MODE) {
    init_radio();
    Serial.println("radio inited");
  }



  updateLED();
}

int ms = 0;
int last_ms = 0;
uint16_t switch_states = 0;
uint16_t switch_states_last = 1;
void loop()
{while(1){
  ESP.wdtFeed();
  //delay(2);

  if (mp3_play){
    if (mp3->isRunning()) {
      if (!mp3->loop()) { //play next file
        mp3_index++;
        if (mp3_index >= (mp3_count)) mp3_index = 0;  //loop back to 0 after last song
        init_mp3();
      }
    } else {init_mp3();}
  }

  ms = millis();
  if (ms == last_ms) continue;
  last_ms = ms;


  if (!(ms%50)){

    switch_states = io.update_pinData();    //this must be called before reading any pins! (reads them all at once in one command...)
    if (switch_states != switch_states_last) {
        if(io.digitalRead(SW_VUP)){
          if (device_mode == MP3_MODE) {
            volume ++;
            if (volume > MAX_VOLUME) volume = MAX_VOLUME;
            mp3_gain = mapf(volume, 0, MAX_VOLUME, 0, MP3_MAX_GAIN);
            out->SetGain(mp3_gain);
            Serial.print(volume); Serial.print(" ("); Serial.print(mp3_gain); Serial.println(")");
            updateLED();
          } else if (device_mode == RADIO_MODE) {
            volume ++;
            if (volume == 16) volume = 15;
            radio.setVolume(volume);
            displayInfo();
            updateLED();
          }


        }
        if(io.digitalRead(SW_VDOWN)){
          if (device_mode == MP3_MODE) {
            volume --;
            if (volume < 0) volume = 0;
            mp3_gain = mapf(volume, 0, MAX_VOLUME, 0, MP3_MAX_GAIN);
            out->SetGain(mp3_gain);
            Serial.print(volume); Serial.print(" ("); Serial.print(mp3_gain); Serial.println(")");
            updateLED();
          } else if (device_mode == RADIO_MODE) {
            volume --;
            if (volume < 0) volume = 0;
            radio.setVolume(volume);
            displayInfo();
            updateLED();
          }

        }
        if(io.digitalRead(SW_RIGHT)){
          if (device_mode == MP3_MODE) {
            mp3_index++;
            if (mp3_index >= (mp3_count)) mp3_index = 0;  //loop back to 0 after last song
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            channel+=2;
            if (channel > 1079) channel = 879;
            radio.setChannel(channel);
            displayInfo();
          }

        }
        if(io.digitalRead(SW_LEFT)){
          if (device_mode == MP3_MODE) {
            mp3_index--;
            if (mp3_index < 0) mp3_index = mp3_count-1;  //loop to last song
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            channel-=2;
            if (channel < 879) channel = 1079;
            radio.setChannel(channel);
            displayInfo();
          }

        }
        if(io.digitalRead(SW_UP)){
          if (device_mode == MP3_MODE) {
            folder_index --;
            if (folder_index < 0) folder_index = folder_count-1;  //loop back to 0 after last folder
            listFiles();
            mp3_index = 0;
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            channel = radio.seekUp();
            displayInfo();
            updateLED();
          }

        }
        if(io.digitalRead(SW_DOWN)){
          if (device_mode == MP3_MODE) {
            folder_index ++;
            if (folder_index >= (folder_count)) folder_index = 0;  //loop back to 0 after last folder
            listFiles();
            mp3_index = 0;
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            channel = radio.seekDown();
            displayInfo();
            updateLED();
          }
        }
        if(io.digitalRead(SW_MODE)){
          if (device_mode == MP3_MODE) {
            switch_mode_radio();
          } else if (device_mode == RADIO_MODE) {
            switch_mode_mp3();
          }
          Serial.println(device_mode);
        }
        if(io.digitalRead(SW_PLAY)){
          if (device_mode == MP3_MODE) {
            if (mp3_play){
              mp3_play = false;


            }else{
              mp3_play = true;

            }
          } else if (device_mode == RADIO_MODE) {
            if (radio_play){
              radio.setVolume(0);
              radio_play = false;
            } else {
              radio.setVolume(volume);
              radio_play = true;
            }
          }

        }

        if(io.digitalRead(SW_Q)){
          readSD();
          //Serial.println(file ->getPos());
          
        }

        if(io.digitalRead(SW_POW)){
          updateTrackPos();
          set_nonVols();
          powerDown();
        }



    switch_states_last = switch_states;

    //delay(1); //removing this makes the watchdog trip in radio mode....idk...reduce?
  }}
}}

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
  io.digitalWrite(USB_SD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RST, HIGH);

  //loop until SW_POW is pressed (should reset ESP though when MUX_SEL goes low and triggers FTDI...)
  while(1){
    delay(100);
    io.update_pinData();
    if (io.digitalRead(SW_Q)){
      io.digitalWrite(MUX_SEL, LOW);    //this will reset the ESP...
      //RESET SD card and SD reader
      io.digitalWrite(USB_SD_RST, LOW);   //not reached...
      delay(100);
      io.digitalWrite(USB_SD_RST, HIGH);
      return;
    }
  }
}


//remember, LEDs are reversed (0 = on)
void updateLED(){
  io.digitalWrite(LED1, 1);io.digitalWrite(LED2, 1);io.digitalWrite(LED3, 1);io.digitalWrite(LED4, 1);
  if (volume>=1) {
    io.digitalWrite(LED1, 0);
  }
  if (volume>=4) {
    io.digitalWrite(LED2, 0);
  }
  if (volume>=8) {
    io.digitalWrite(LED3, 0);
  }
  if (volume>=12) {
    io.digitalWrite(LED4, 0);
  }
}

void displayInfo()
{
   Serial.print("Channel:"); Serial.print(channel);
   Serial.print(" Volume:"); Serial.println(volume);
}


float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void updateTrackPos()
{
  nonVol[TRACKPOS] = file->getPos();
}