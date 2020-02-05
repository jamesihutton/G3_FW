#include <SparkFunSi4703.h>
//#include <tpa2016.h>
#include <Wire.h>
#include "PCF8575.h"

#include "tpa2016.h"

#include <SPI.h>
#include <SD.h>

#include "string.h"
File root;


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
#include "AudioOutputI2SNoDAC.h"

AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2SNoDAC *out;
AudioFileSourceID3 *id3;



#define pcf_addr B0100000
#define tpa_addr B1011000

int resetPin = D8;  //GPIO15 HOTWIRE
int SDIO = D2;
int SCLK = D1;

tpa tpa;

Si4703_Breakout radio(resetPin, SDIO, SCLK);
int channel = 947;
int volume = 5;
char rdsBuffer[10];

float mp3_gain = 2.0;
int8_t amp_gain = 0;

PCF8575 pcf8575(pcf_addr,SDIO,SCLK);
void displayInfo();

String mp3_name[100];
int mp3_count = 0;
int mp3_index = 0;

String folder_name[70];
int folder_count = 0;
int folder_index = 0;

bool mp3_initialized = 0;

bool mp3_play = true;


bool device_mode = 0; //0 = mp3, 1 = radio
#define MP3_MODE 0
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
  Serial.println();Serial.println();
  Serial.print("FOLDER COUNT: ");
  Serial.println(i);
  Serial.println();Serial.println();
  for (int j = 0; j<i; j++)
  {
    Serial.println(folder_name[j]);
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
    Serial.println(s);
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
  Serial.println();Serial.println();
  Serial.print("MP3 COUNT: ");
  Serial.println(i);
  Serial.println();Serial.println();
  for (int j = 0; j<i; j++)
  {
    Serial.println(mp3_name[j]);
  }
  

}

void init_mp3()
{
  Serial.print("init mp3 :");
  Serial.println(mp3_name[mp3_index]);
  if (mp3_initialized){
    if (mp3->isRunning()) {
      Serial.println("isRunning");
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
  Serial.println(s);
  audioLogger = &Serial;
  file = new AudioFileSourceSD(s);
  id3 = new AudioFileSourceID3(file);
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3_initialized = true; //raised forever after first init
  mp3->begin(id3,out);
  out->SetGain(mp3_gain);
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
  
  device_mode = RADIO_MODE;
}

void switch_mode_mp3()
{
  digitalWrite(resetPin,LOW); //put radio into reset mode (disable it)
  init_mp3();
  device_mode = MP3_MODE;
}

void setup()
{
  Serial.begin(9600);

  WiFi.mode(WIFI_OFF); 
  SD.begin(D0);
  listFolders();
  listFiles();

  if (device_mode == MP3_MODE) {
    init_mp3();
  } else if (device_mode == RADIO_MODE) {
    init_radio();
  }
  

    
  
  
  // Set pinModes
  
  pcf8575.pinMode(MUX_SEL, OUTPUT);       pcf8575.digitalWrite(MUX_SEL, LOW);
  pcf8575.pinMode(AMP_SHUTOFF, OUTPUT);   pcf8575.digitalWrite(AMP_SHUTOFF, LOW);
  pcf8575.pinMode(RADIO_SHUTOFF, OUTPUT); pcf8575.digitalWrite(RADIO_SHUTOFF, LOW);
  pcf8575.pinMode(RADIO_RST, OUTPUT);   pcf8575.digitalWrite(RADIO_RST, LOW);
  
  pcf8575.pinMode(LED1, OUTPUT);   pcf8575.digitalWrite(LED1, LOW);
  pcf8575.pinMode(LED2, OUTPUT);   pcf8575.digitalWrite(LED2, LOW);
  pcf8575.pinMode(LED3, OUTPUT);   pcf8575.digitalWrite(LED3, LOW);
  pcf8575.pinMode(LED4, OUTPUT);   pcf8575.digitalWrite(LED4, LOW);
  
  pcf8575.begin();



  //SET UP TPA AMP
  tpa.setGain(tpa.gain);
  
}

int ms = 0;
int last_ms = 0;
byte pcf_byte[2];
byte last_pcf_byte[2];
void loop()
{while(1){

  delayMicroseconds(10);
  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  }
  
  ms = millis();
  if (ms == last_ms) continue;
  last_ms = ms;

  

  if (!(ms%50)){
    
    
    Wire.requestFrom(pcf_addr,2);
    
    pcf_byte[0] = Wire.read(); pcf_byte[1] = Wire.read();
    
    
    if (pcf_byte[1] != last_pcf_byte[1]) {
        if(pcf_byte[1] & SW_VUP_MASK){
          if (device_mode == MP3_MODE) {
            volume ++;
            if (volume == 16) volume = 15;
            tpa.setGain(map(volume,0,15,-28,30));
            Serial.println(map(volume,0,15,-28,30));
            updateLED();
          } else if (device_mode == RADIO_MODE) {
            volume ++;
            if (volume == 16) volume = 15;
            radio.setVolume(volume);
            displayInfo();
            updateLED();
          }
          
          
        }
        if(pcf_byte[1] & SW_VDOWN_MASK){
          if (device_mode == MP3_MODE) {
            volume --;
            if (volume < 0) volume = 0;     
            tpa.setGain(map(volume,0,15,-28,30));
            Serial.println(map(volume,0,15,-28,30));
            updateLED();
          } else if (device_mode == RADIO_MODE) {
            volume --;
            if (volume < 0) volume = 0;
            radio.setVolume(volume);
            displayInfo();
            updateLED();
          }
          
        }
        if(pcf_byte[1] & SW_RIGHT_MASK){
          if (device_mode == MP3_MODE) {
            mp3_index++;
            if (mp3_index >= (mp3_count)) mp3_index = 0;  //loop back to 0 after last song
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            channel = radio.seekUp();
            displayInfo();
            updateLED();
          }
          
        }
        if(pcf_byte[1] & SW_LEFT_MASK){
          if (device_mode == MP3_MODE) {
            mp3_index--;
            if (mp3_index < 0) mp3_index = mp3_count-1;  //loop to last song
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            channel = radio.seekDown();
            displayInfo();
            updateLED();
          }
          
        }
        if(pcf_byte[1] & SW_UP_MASK){
          if (device_mode == MP3_MODE) {
            folder_index --;
            if (folder_index < 0) folder_index = folder_count-1;  //loop back to 0 after last folder
            listFiles();
            mp3_index = 0;
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            
          }
          
        }
        if(pcf_byte[1] & SW_DOWN_MASK){
          if (device_mode == MP3_MODE) {
            folder_index ++;
            if (folder_index >= (folder_count)) folder_index = 0;  //loop back to 0 after last folder
            listFiles();
            mp3_index = 0;
            init_mp3();
          } else if (device_mode == RADIO_MODE) {
            
          }
        }
        if(pcf_byte[1] & SW_POW_MASK){
          if (device_mode == MP3_MODE) {
            switch_mode_radio();
          } else if (device_mode == RADIO_MODE) {
            switch_mode_mp3();
          } 
        }
        if(pcf_byte[1] & SW_PLAY_MASK){
          if (device_mode == MP3_MODE) {
            if (mp3_play){
              mp3_play = false;
              mp3->stop();
            }else{
              mp3_play = true;
              //mp3->play();  ///does not work
            }
          } else if (device_mode == RADIO_MODE) {
            radio.setVolume(0);
          }
          
        }
    }
    last_pcf_byte[1] = pcf_byte[1];
    
    
    
    
    delay(1);
  }  
}}

//remember, LEDs are reversed (0 = on)
void updateLED(){
  pcf8575.digitalWrite(LED1, 1);pcf8575.digitalWrite(LED2, 1);pcf8575.digitalWrite(LED3, 1);pcf8575.digitalWrite(LED4, 1);
  if (volume>=1) {
    pcf8575.digitalWrite(LED1, 0);
  }
  if (volume>=4) {
    pcf8575.digitalWrite(LED2, 0);
  }
  if (volume>=8) {
    pcf8575.digitalWrite(LED3, 0);
  }
  if (volume>=12) {
    pcf8575.digitalWrite(LED4, 0);
  }
}

void displayInfo()
{
   Serial.print("Channel:"); Serial.print(channel); 
   Serial.print(" Volume:"); Serial.println(volume); 
}
