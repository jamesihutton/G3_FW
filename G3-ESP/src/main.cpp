

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

#include "SparkFunSi4703.h"




#include <SPI.h>
#include <SD.h>
// You may need a fast SD card. Set this as high as it will work (40MHz max).
#define SPI_SPEED   SD_SCK_MHZ(40)

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




#define fm_addr  0x10

int resetPin = D0;  //GPIO16 HOTWIRE
int SDIO = D2;
int SCLK = D1;



Si4703_Breakout radio(resetPin, SDIO, SCLK);
int channel = 947;
char rdsBuffer[10];

#define TRACK_MAX_GAIN    2.6   //with a fixed 1/2vDiv on PAM8019 volume input!
#define MAX_VOLUME      15    //used for both Radio and MP3 nv.deviceVolume!
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


  track_gain = mapf(nv.deviceVolume, 0, MAX_VOLUME, 0, TRACK_MAX_GAIN);
  out->SetGain(track_gain);
  Serial.print("\ngain = ");
  Serial.println(track_gain);

  
  //nv.set_nonVols(); //not sure if I want to update every time... only on power down?
}

void init_radio()
{
  radio.powerOn();
  radio.setVolume(nv.deviceVolume);
  radio.setChannel(channel);
  displayInfo();
  updateLED();
}

void switch_mode_radio()
{
  if (track_initialized){
    if (mp3->isRunning()) {
      mp3->stop();
    } else if (wav->isRunning()) {
      wav->stop();
    }
  }

  init_radio();
  track_play = false;
  nv.deviceMode = RADIO_MODE;

}

void switch_mode_track()
{
  digitalWrite(resetPin,LOW); //put radio into reset mode (disable it)
  init_track();
  nv.deviceMode = TRACK_MODE;
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
  io.digitalWrite(USB_SD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RST, HIGH);

   

  int resp = SD.begin(D0, SPI_SPEED); //sometimes throws error in IDE... but works fine...
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
  /*
  if(SPIFFS.format()) Serial.println("File System Formated");
  else                Serial.println("File System Formatting Error");
  //while(1){Serial.println("done"); delay(1000);}
  */

  nv.get_nonVols();

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
uint16_t switch_states = 0;
uint16_t switch_states_last = 1;
void loop()
{while(1){
  //feed WDT
  ESP.wdtFeed();

  //check if track has finished
  if (track_play){
    if (mp3->isRunning()) {
      if (!mp3->loop()) { //play next file
        nv.trackIndex++;
        if (nv.trackIndex >= (track_count)) nv.trackIndex = 0;  //loop back to 0 after last song
        init_track();
      }
    } else if (wav->isRunning()){
        if (!wav->loop()) { //play next file
        nv.trackIndex++;
        if (nv.trackIndex >= (track_count)) nv.trackIndex = 0;  //loop back to 0 after last song
        init_track();
      }
    }
    else {init_track();}
  }

  ms = millis();
  if (ms == last_ms) continue;
  last_ms = ms;

  //NEEDS TO CHANGE TO >50... WORK ON THIS LATER...
  if (!(ms%50)){

    switch_states = io.update_pinData();    //this must be called before reading any pins! (reads them all at once in one command...)
    if (switch_states != switch_states_last) {
        
        
        if(io.digitalRead(SW_VUP)){
          if (nv.deviceMode == TRACK_MODE) {
            nv.deviceVolume ++;
            if (nv.deviceVolume > MAX_VOLUME) nv.deviceVolume = MAX_VOLUME;
            track_gain = mapf(nv.deviceVolume, 0, MAX_VOLUME, 0, TRACK_MAX_GAIN);
            jingle(JINGLE_TICK, track_gain); //play the tick sound  

            out->SetGain(track_gain);
            Serial.print(nv.deviceVolume); Serial.print(" ("); Serial.print(track_gain); Serial.println(")");
            updateLED();
          } else if (nv.deviceMode == RADIO_MODE) {
            nv.deviceVolume ++;
            if (nv.deviceVolume == 16) nv.deviceVolume = 15;
            radio.setVolume(nv.deviceVolume);
            displayInfo();
            updateLED();
          }


        }
        if(io.digitalRead(SW_VDOWN)){
          if (nv.deviceMode == TRACK_MODE) {
            nv.deviceVolume --;
            if (nv.deviceVolume < 0) nv.deviceVolume = 0;
            track_gain = mapf(nv.deviceVolume, 0, MAX_VOLUME, 0, TRACK_MAX_GAIN);
            jingle(JINGLE_TICK, track_gain); //play the tick sound
            out->SetGain(track_gain);
            Serial.print(nv.deviceVolume); Serial.print(" ("); Serial.print(track_gain); Serial.println(")");
            updateLED();
          } else if (nv.deviceMode == RADIO_MODE) {
            nv.deviceVolume --;
            if (nv.deviceVolume < 0) nv.deviceVolume = 0;
            radio.setVolume(nv.deviceVolume);
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
            channel+=2;
            if (channel > 1079) channel = 879;
            radio.setChannel(channel);
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
            channel-=2;
            if (channel < 879) channel = 1079;
            radio.setChannel(channel);
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
            channel = radio.seekUp();
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
            channel = radio.seekDown();
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
            if (track_play){
              track_play = false;


            }else{
              track_play = true;

            }
          } else if (nv.deviceMode == RADIO_MODE) {
            if (radio_play){
              radio.setVolume(0);
              radio_play = false;
            } else {
              radio.setVolume(nv.deviceVolume);
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
   Serial.print(" Volume:"); Serial.println(nv.deviceVolume);
}


float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

