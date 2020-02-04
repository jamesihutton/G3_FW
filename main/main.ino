#include <SparkFunSi4703.h>
//#include <tpa2016.h>
#include <Wire.h>
#include "PCF8575.h"

#include "tpa2016.h"


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

//Si4703_Breakout radio(resetPin, SDIO, SCLK);
int channel = 947;
int volume = 10;
char rdsBuffer[10];

float mp3_gain = 3.0;
int8_t amp_gain = 0;

PCF8575 pcf8575(pcf_addr,SDIO,SCLK);
void displayInfo();


void setup()
{
  Serial.begin(9600);

  WiFi.mode(WIFI_OFF); 
  SD.begin(D0);
  Serial.printf("Sample MP3 playback begins...\n");

  audioLogger = &Serial;
  file = new AudioFileSourceSD("genesis.mp3");
  id3 = new AudioFileSourceID3(file);
  //id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  out->SetGain(mp3_gain);
  
/*
  radio.powerOn();
  radio.setVolume(volume);
  radio.setChannel(channel);
  displayInfo();
  updateLED();
  */
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

  delay(1);
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
          volume ++;
          if (volume == 16) volume = 15;
          //radio.setVolume(volume);
          displayInfo();
          updateLED();
        }
        if(pcf_byte[1] & SW_VDOWN_MASK){
          volume --;
          if (volume < 0) volume = 0;
          //radio.setVolume(volume);
          displayInfo();
          updateLED();
        }
        if(pcf_byte[1] & SW_RIGHT_MASK){
          //channel = radio.seekUp();
          displayInfo();
          updateLED();
        }
        if(pcf_byte[1] & SW_LEFT_MASK){
          //channel = radio.seekDown();
          displayInfo();
          updateLED();
        }
        if(pcf_byte[1] & SW_UP_MASK){
          tpa.gain += 5;
          if (tpa.gain > 30) tpa.gain = 30;
          tpa.setGain(tpa.gain);
          Serial.println(tpa.gain);
        }
        if(pcf_byte[1] & SW_DOWN_MASK){
          tpa.gain -= 5;
          if (tpa.gain < -28) tpa.gain = -28;
          tpa.setGain(tpa.gain);
          Serial.println(tpa.gain);
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
