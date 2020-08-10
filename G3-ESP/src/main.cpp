  //needed for sleeping...
 ////////////////////////////////////////////////////////////////////////////
extern "C" {
  #include "user_interface.h"
}
extern "C" {
  #include "gpio.h"
}
#include <ESP8266WiFi.h>

// only if you want wifi shut down completely
// if you dont need wifi, this has lowest wakeup power spike
RF_MODE(RF_DISABLED);   

#define SLEEP_TIME   15  //light sleep intervals are this many seconds
 ////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>





#include "main.h"
#include "compass.h" 
#include "compass_io.h" 
#include "compass_nonVols.h"
#include "compass_jingles.h"



#include <Wire.h>

#include <twi.h>
#include <SPI.h>
#include <SD.h>
#include "LittleFS.h"
// You may need a fast SD card. Set this as high as it will work (40MHz max).
#define SPI_SPEED   SD_SCK_MHZ(40)
#define SD_CS       17    //a non-existent pin 


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



static const int fm_addr = 0x11;   //i2c address for si4734 (0x63 for SEN = HIGH, 0x11 for SEN = LOW)
int SDIO = 4;
int SCLK = 5;

#define MUTE_PIN 16



int channel = 9470;
#define fm_max 10790
#define fm_min  8790
//#define fm_min  6410  //can go lower than general fm channels on fm mode...
char rdsBuffer[10];

#define TRACK_MAX_GAIN    1.5   //
#define TRACK_MIN_GAIN    0.01   //

#define RADIO_MAX_GAIN    63    
#define RADIO_MIN_GAIN    25

#define MAX_DEVICE_VOL    15    //used for both Radio and MP3 nv.deviceVolume!
#define MIN_DEVICE_VOL    1
float track_gain = 0.3;     //starting level





String track_name[100];
int track_count = 0;


String folder_name[70];
int folder_count = 0;


bool track_initialized = 0; //only happens first time

bool track_play = false;
bool radio_play = false;

bool LED_power_save = false;  //flag if LEDs have faded to save power

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

#define MUTE          1
#define UNMUTE        0
#define MUTE_MS       0   //the amount of ms to mute into each track (to avoid "click")
#define PRE_MUTE_MS   100 //the amount of ms to mute BEFORE each track
void init_track()
{
  digitalWrite(MUTE_PIN, MUTE); //mute amp
  delay(PRE_MUTE_MS);
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


  track_gain = track_gain_convert();
  out->SetGain(track_gain);
  Serial.print("\ngain = ");
  Serial.println(track_gain);
  
  //mute amp for "MUTE_MS" milliseconds into track (to avoid "click")
  int start_time = millis();
  while(1){
    if (millis() >= (start_time + MUTE_MS)){
      digitalWrite(MUTE_PIN, UNMUTE);
      break;
    }
    ESP.wdtFeed();
    track_tick();
  }
  
  //nv.set_nonVols(); //not sure if I want to update every time... only on power down?
}


#define   MUTE_RADIO_MS   1  //amount of ms to mute amp when switching to radio mode (to avoid pop)
bool init_radio()
{

  digitalWrite(MUTE_PIN, MUTE);   //mute during init to avoid pops
  
  //RESET radio (and SD card...)
  SD.end();
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(10);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);

  int resp = SD.begin(SD_CS, SPI_SPEED); //SD card must now be restarted, since doing this toggled it's power
  if (!resp) {
    while(1){
      Serial.println("\n\nCould not connect to SD card\n\n");
     io.digitalWrite(LED1, 1);io.digitalWrite(LED2, 1);io.digitalWrite(LED3, 1);io.digitalWrite(LED4, 1);
     delay(400);
     io.digitalWrite(LED1, 0);io.digitalWrite(LED2, 0);io.digitalWrite(LED3, 0);io.digitalWrite(LED4, 0);
     delay(400);
    }
  } 

  Wire.begin(SDIO, SCLK);  //SDA, SCL

  //POINT TO ADDRESS TO READ FROM
  Wire.beginTransmission(fm_addr);
  Wire.write(B00000001);  //0x01  CMD: POWERUP
  Wire.write(B00010000);  //0x10 //external crystal
  Wire.write(0x05);  //0x05
  if(Wire.endTransmission()) return 0;  //NACK... failed to connect

  
  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  //0x12
  Wire.write(0x00);  //0x00
  Wire.write(0x13);  //0x13  // FM_SOFT_MUTE_MAX_ATTENUATION
  Wire.write(0x02);  //0x02
  Wire.write(0x00);  //0x00
  Wire.write(0x00);  //0x03   //3dB
  Wire.endTransmission();

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  //0x12
  Wire.write(0x00);  //0x00
  Wire.write(0xFF);  //0xFF  // ***DISABLE DEBUG MODE!! PAGE 299!!!
  Wire.write(0x00);  //0x00
  Wire.write(0x00);  //0x00
  Wire.write(0x00);  //0x00
  Wire.endTransmission();


  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x11);  
  Wire.write(0x08);  
  Wire.write(0x00);
  Wire.write(0x14);  //tune_error to 20kHz (best seek performance)
  Wire.endTransmission();
  delay(100);
  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x14);  
  Wire.write(0x03);  
  Wire.write(0x00);  
  Wire.write(0x03);  //set seek SNR thresh in dB (default was 3 dB)
  Wire.endTransmission();
  delay(100);
  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x14);  
  Wire.write(0x04);  
  Wire.write(0x00);  
  Wire.write(0x15);  //set seek RSSI thresh in dBuV (default was 20 dBuV)
  Wire.endTransmission();
  delay(100);

  ///////////////////////////////////
  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x00);
  Wire.write(0x8F);  //FM_RSQ_INT_SOURCE
  Wire.endTransmission();
  delay(100);

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x12);  
  Wire.write(0x01);  
  Wire.write(0x00);
  Wire.write(0x1E);  //FM_RSQ_SNR_HI_THRESHOLD
  delay(100);

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x12);  
  Wire.write(0x02);  
  Wire.write(0x00);
  Wire.write(0x06);  //FM_RSQ_SNR_LO_THRESHOLD
  delay(100);

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x12);  
  Wire.write(0x03);  
  Wire.write(0x00);
  Wire.write(0x32);  //FM_RSQ_RSSI_HI_THRESHOLD
  delay(100);

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x12);  
  Wire.write(0x04);  
  Wire.write(0x00);
  Wire.write(0x18);  //FM_RSQ_RSSI_LO_THRESHOLD
  delay(100);

  Wire.beginTransmission(fm_addr);
  Wire.write(0x12);  
  Wire.write(0x00);  
  Wire.write(0x14);  
  Wire.write(0x00);  
  Wire.write(0x22);
  Wire.write(0x56);  //FM_SEEK_BAND_BOTTOM (8790)
  delay(100);
  ///////////////////////////////////

  /*
  Wire.beginTransmission(fm_addr);
  Wire.write(0x14);  
  Wire.write(0x02);  
  Wire.write(0x00);  
  Wire.write(0x14);  //set seek spacing to 200kHz (default was 100kHz)
  Wire.endTransmission();
  delay(200);
  */

  set_rad_vol(nv.deviceVolume);
  set_rad_chan(nv.radioChannel);

  radio_play = true;

  delay(MUTE_RADIO_MS);
  digitalWrite(MUTE_PIN, UNMUTE);

  return 1; //success

}

int rad_seek(bool dir)
{
  //seek
  Wire.beginTransmission(fm_addr);
  Wire.write(0x21);  
  if (dir)  Wire.write(B00001100); //seek up
  else      Wire.write(B00000100); //seek down
  Wire.endTransmission();
  delay(100);

  int resp[8];
  int timeout = 0;
  while(1){
    Wire.beginTransmission(fm_addr);
    Wire.write(0x22); //get fm tune status
    Wire.write(0x01);
    Wire.endTransmission();
    Wire.requestFrom(fm_addr, (byte) 7);
    for (int i = 0; i<8; i++) {
      resp[i] = Wire.read();
    }
    
    //once valid station is found
    if (resp[1] > 0) {
      delay(1000); //settle channel
      //poll one last time
      Wire.beginTransmission(fm_addr);
      Wire.write(0x22); //get fm tune status
      Wire.write(0x01);
      Wire.endTransmission();
      Wire.requestFrom(fm_addr, (byte) 7);
      for (int i = 0; i<8; i++) {
        resp[i] = Wire.read();
      }
      int freq = (resp[2] <<  8) | resp[3];    
      nv.radioChannel = freq;
      Serial.printf("\nRadio seeked to: %i", freq);
      Serial.printf("\nRSSI: \t\t%i dB", resp[4]);
      Serial.printf("\nSNR: \t\t%i dB", resp[5]);
      return freq;
    }
    delay(100);
    timeout += 100;
    if (timeout > 5000) return 0; //timeout after 5 seconds
  }

}

void print_rad_info()
{
  int resp[8];
  Wire.beginTransmission(fm_addr);
  Wire.write(0x22); //get fm tune status
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(fm_addr, (byte) 7);
  for (int i = 0; i<8; i++) {
    resp[i] = Wire.read();
  }
  int freq = (resp[2] <<  8) | resp[3];    
  nv.radioChannel = freq;
  Serial.printf("\nChannel: %i", freq);
  Serial.printf("\nRSSI: \t\t%i dB", resp[4]);
  Serial.printf("\nSNR: \t\t%i dB", resp[5]);
}

int set_rad_vol(int vol)
{
  int vhex;
  if (vol < 0) vhex = 0;
  else vhex = radio_gain_convert(); 
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

//sets the radio into low power state
//NOTE: only responds to power_up command after this (or rst)
//All GPO's are low when in this state
int powerdown_radio()
{
  Wire.beginTransmission(fm_addr);
  Wire.write(0x11);  
  int resp = Wire.endTransmission();
  return (!resp);
}

void switch_mode_radio()
{
  Serial.println("switching to radio mode");

  if(init_radio()) {
    Serial.println("Radio initted...");
    track_play = false;
    radio_play = true;
    nv.deviceMode = RADIO_MODE;
    nv.trackFrame = file->getPos();
    if (mp3->isRunning()) {
      mp3->stop();
    } else if (wav->isRunning()) {
      wav->stop();
    }
    SD.end();
    track_initialized = 0;
  } else {
    Serial.println("could not connect to radio...");
  }
}

void switch_mode_track()
{
  //avoid "click" on radio power down
  digitalWrite(MUTE_PIN, MUTE); 
  delay(150);  
  powerdown_radio();

  SD.begin(SD_CS, SPI_SPEED); 
  nv.deviceMode = TRACK_MODE;
  radio_play = false;
  track_play = true;
  init_track();
  Serial.println("track initted");
}



//LATCHES SUICIDE CIRCUIT BUY SETTING SX1509 OSCIO PIN HIGH
bool latchPower()
{
  int resp = 0;

  //latch power
  for (int i = 0; i<10; i++){
    resp = io.OSCIO_set(HIGH);
    if (resp) {
      Serial.println("power latched");
      return 1;
    }
    delay(1);
  }
  return 0; //failure after 10 tries
  
}

//DE-LATCHES SUICIDE CIRCUIT BUY SETTING SX1509 OSCIO PIN LOW
void powerDown_device()
{
  Serial.println("turning off power...");
  io.reset();   //remove LED drivers
  io.OSCIO_set(LOW);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600); Serial.println("\n\nboot\n\n");
  delay(100);
  Serial.print("Firmware Rev: ");
  Serial.println(FW_REV);
  
  // Set pinModes
  io.init();
  for(int i = 1; i<= 4; i++) io.pwm(i, 0);
  io.digitalWrite(LED1, 0);io.digitalWrite(LED2, 0);io.digitalWrite(LED3, 0);io.digitalWrite(LED4, 0);
  pinMode(0, INPUT); //!IO_INT pin as input

  pinMode(MUTE_PIN, OUTPUT);
  digitalWrite(MUTE_PIN, 0);
 
  


  if(latchPower()){
    Serial.println("power latched...");
  } else {
    Serial.println("could not latch power...");
    delay(1000);
  }

  powerdown_radio();  //can remove if not debugging...
  
  //what woke the device up?
  handle_wakeup();

  //check voltage:
  if (!LV_check()) {
    Serial.println("Battery voltage to low... Powering down");
    jingle(JINGLE_LOWBATT, DEFAULT_JINGLE_GAIN);
    //power down board
    io.reset();
    io.OSCIO_set(LOW);
    delay(5000);  //wait for latch circuit to die

    //if still on at this point, USB is keeping on, 
    //so reset the device and go into charging animation...
    Serial.println("reseting device...");
    ESP.restart();
    
    //should never reach this
    while(1){}
  }

  Serial.println("power - up jingle");
  //play power up jingle
  jingle(JINGLE_POWER_UP, 0.10);       //takes ~2 seconds



  io.update_pinData();
  while(io.digitalRead(SW_POW)){  //wait until user releases SW_POW to proceed
    io.update_pinData();
    delay(100);
  }

  WiFi.mode(WIFI_OFF);
  wifi_fpm_set_sleep_type (LIGHT_SLEEP_T);
  WiFi.forceSleepBegin(); //<-- saves like 100mA!

  //RESET SD card
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(100);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);


  int resp = SD.begin(SD_CS, SPI_SPEED); 
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
  if(LittleFS.begin())  Serial.println("SPIFFS Initialize....ok");
  else                Serial.println("SPIFFS Initialization...failed");
 
  //Format File System
  if(io.digitalRead(SW_Q)){
    if(LittleFS.format()) Serial.println("File System Formated");
    else                Serial.println("File System Formatting Error");
  }

  nv.get_nonVols();
  


  //init either radio or player...
  if (nv.deviceMode == TRACK_MODE) {
    Serial.println("INIT TRACK");
    listFolders();
    listFiles();
    init_track();
    track_play = true;
  } else if (nv.deviceMode == RADIO_MODE) {
    Serial.println("INIT RADIO");
    init_radio();
    listFolders();
    listFiles();
    radio_play = true;
  }

  updateLED();
}



void device_init()
{
  Serial.println("power - up jingle");
  //play power up jingle
  jingle(JINGLE_POWER_UP, 0.1);       //takes ~2 seconds



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
  if(LittleFS.begin())  Serial.println("SPIFFS Initialize....ok");
  else                Serial.println("SPIFFS Initialization...failed");
 
  //Format File System
  if(io.digitalRead(SW_Q)){
    if(LittleFS.format()) Serial.println("File System Formated");
    else                Serial.println("File System Formatting Error");
  }

  nv.get_nonVols();
  


  //init either radio or player...
  if (nv.deviceMode == TRACK_MODE) {
    Serial.println("INIT TRACK");
    listFolders();
    listFiles();
    init_track();
    track_play = true;
  } else if (nv.deviceMode == RADIO_MODE) {
    Serial.println("INIT RADIO");
    init_radio();
    listFolders();
    listFiles();
    radio_play = true;
  }

  updateLED();
}



int mute = 0;
int ms = 0;
int last_ms = 0;
int next_LV_check = LV_CHECK_INTERVAL;
int next_LV_warn = LV_WARN_INTERVAL;
uint32_t pause_time = -1;
uint32_t rad_pause_power_down_time = -1;
uint32_t rad_pause_timer = 0;
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
      if(nv.deviceMode == RADIO_MODE)  radio_sleep_tick();

      //pause checker
      pause_powerdown_check();
  }

  if ((ms >= next_LV_check) && (nv.deviceMode == TRACK_MODE)){
    adc_set(ADC_PIN_VCC);
    delay(10);
    int mv = adc_get(ADC_PIN_VCC);
    if (mv < LV_THRESH) {
      LV_handle();
    }
    else if (mv < LV_WARN_THRESH) {
      if(ms >= next_LV_warn){
        //make sure not charging
        adc_set(ADC_PIN_USBVCC);
        delay(10);
        if (adc_get(ADC_PIN_USBVCC) < 200) {
          //stop current track
          int resume_playing = track_play;
          nv.trackFrame = file->getPos();
          if (mp3->isRunning()) {
            mp3->stop();
          } else if (wav->isRunning()) {
            wav->stop();
          }
          track_initialized = 0;
          //play jingle
          jingle(JINGLE_LOWBATT, 1);
          //resume track
          init_track();
          track_play = resume_playing;
          next_LV_warn = ms + LV_WARN_INTERVAL;
        }
      }        
    }
    next_LV_check = ms + LV_CHECK_INTERVAL;
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

int LED_fade_timer = LED_FADEOUT_TIME;
void button_tick()
{
  if(!digitalRead(0)){ //if !IO_INT is triggered... read buttons
      //bring back LEDs if they faded out
      updateLED();
      LED_power_save = false;
      LED_fade_timer = millis() + LED_FADEOUT_TIME;

      Serial.println("button pressed");
      io.update_pinData();    //this must be called before reading any pins! (reads them all at once in one command...)
      if(io.digitalRead(SW_VUP)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.deviceVolume ++;
          if (nv.deviceVolume > MAX_DEVICE_VOL) nv.deviceVolume = MAX_DEVICE_VOL;
          track_gain = track_gain_convert();
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
          if (nv.deviceVolume > MAX_DEVICE_VOL) nv.deviceVolume = MAX_DEVICE_VOL;

          set_rad_vol(nv.deviceVolume);
          displayInfo();
          updateLED();
        }


      }
      if(io.digitalRead(SW_VDOWN)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.deviceVolume --;
          if (nv.deviceVolume < MIN_DEVICE_VOL) nv.deviceVolume = MIN_DEVICE_VOL;
          track_gain = track_gain_convert();
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
          if (nv.deviceVolume < MIN_DEVICE_VOL) nv.deviceVolume = MIN_DEVICE_VOL;
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
          //delay(1000);  //wait for i2c line to settle 
          rad_seek(1);
        }

      }
      if(io.digitalRead(SW_LEFT)){
        if (nv.deviceMode == TRACK_MODE) {
          nv.trackIndex--; 
          if (nv.trackIndex < 0) nv.trackIndex = track_count-1;  //loop to last song
          nv.trackFrame = 0;
          init_track();
        } else if (nv.deviceMode == RADIO_MODE) {
          //delay(1000);  //wait for i2c line to settle 
          rad_seek(0);
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
          nv.radioChannel += 10;
          if (nv.radioChannel > fm_max+1) nv.radioChannel = fm_min;
          set_rad_chan(nv.radioChannel);
          print_rad_info();
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
          nv.radioChannel -= 10;
          if (nv.radioChannel < fm_min-1) nv.radioChannel = fm_max;
          set_rad_chan(nv.radioChannel);
          print_rad_info();
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
            if (track_play) {
              track_play = false;    
              pause_time = millis();     
            } else {
              track_play = true;
              pause_time = -1; //redundant... just in case...
            }
        }else if (nv.deviceMode == RADIO_MODE) {
          print_rad_info();
          rad_pause_timer = 0;
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
        //feature not yet enabled...


        /*
        //update track frame last second...
        nv.trackFrame = file->getPos();

        //set all the params in nonVol memory 
        nv.set_nonVols();

        //safely end SPIFFS
        LittleFS.end();
        
        readSD();
        */
        
      }

      if(io.digitalRead(SW_POW)){
        Serial.println("power down sequence...");
        
        //update track frame last second...
        if (nv.deviceMode == TRACK_MODE) nv.trackFrame = file->getPos();

        //set all the params in nonVol memory 
        nv.set_nonVols();


        //avoid "click" on radio power down
        digitalWrite(MUTE_PIN, MUTE); 
        delay(150);  
        //power down radio
        powerdown_radio();
        
        //SD.end();

        //play power down jingle
        jingle(JINGLE_POWER_DOWN, 0.1);       //takes ~2 seconds

        //Check if USB is plugged in
        adc_set(ADC_PIN_USBVCC);
        delay(100);
        if (adc_get(ADC_PIN_USBVCC) > 4500) {
          
          Serial.println("charging loop");
          charging_loop();

          
          Serial.println("resuming playback");
          //when returned, resume...
          //init either radio or player...
          device_init();


        } else {

          //safely end SPIFFS
          LittleFS.end();

          //power down board
          io.reset();
          io.OSCIO_set(LOW);
          delay(5000);  //wait for latch circuit to die

          //if still on at this point, USB is keeping on, 
          //so reset the device and go into charging animation...
          Serial.println("reseting device...");
          ESP.restart();
          
          //should never reach this
          while(1){}
        }

      }
  } else {  //if button hasn't been pressed
    if ((millis() >= LED_fade_timer) && !LED_power_save) {
      Serial.println("fade leds");
      io.pwm(1, 0); io.pwm(2, 0); io.pwm(3, 0); io.pwm(4, 0); 
      for (int i = 254; i>=0; i--){ 
        if (nv.deviceVolume>=0)   io.pwm(1, i);
        if (nv.deviceVolume>=4)   io.pwm(2, i);
        if (nv.deviceVolume>=8)   io.pwm(3, i);
        if (nv.deviceVolume>=12)  io.pwm(4, i);
        int m_last = millis();
        while((m_last+4) >= millis()){
          track_tick(); //keep ticking audio file while fading out
        }
      }
      LED_power_save = true;
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
  pinMode(14, INPUT);
  pinMode(12, INPUT);
  pinMode(13, INPUT);

  //orient USB MUX from serial to SD
  io.digitalWrite(MUX_SEL, HIGH);
  //RESET SD card and SD reader
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(1000);
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
  io.pwm(1, 0); io.pwm(2, 0); io.pwm(3, 0); io.pwm(4, 0); //switched to using PWM, but check power consumption...
  if (nv.deviceVolume>=0) {
    io.pwm(1, 255);
  }
  if (nv.deviceVolume>=4) {
    io.pwm(2, 255);
  }
  if (nv.deviceVolume>=8) {
    io.pwm(3, 255);
  }
  if (nv.deviceVolume>=12) {
    io.pwm(4, 255);
  }
}

void displayInfo()
{
   Serial.print("Channel:"); Serial.print(nv.radioChannel);
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
    digitalWrite(MUTE_PIN, MUTE);
    delay(PRE_MUTE_MS);
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

        case JINGLE_CHARGING:
            delay(300);
            file_progmem = new AudioFileSourcePROGMEM(charging, sizeof(charging));
            break;

        case JINGLE_LOWBATT:  
            delay(300);       
            file_progmem = new AudioFileSourcePROGMEM(lowBatt, sizeof(lowBatt));          
            delay(300);
            break;

        default:
          return;
    }
    out_progmem = new AudioOutputI2S();
    wav_progmem = new AudioGeneratorWAV();
    wav_progmem->begin(file_progmem, out_progmem);
    out_progmem->SetGain(gain);

    //mute for a few ms to avoid pop
    int start_time = millis();
    int anim_last_time = 0;
    #define anim_interval 1
    int anim[5] = {0,0,0,0,0};
    int mapped_ceiling = 200;
    while(1){
        if (wav_progmem->isRunning()){
            if (millis() >= (start_time + MUTE_MS)){
              digitalWrite(MUTE_PIN, UNMUTE);
            }
            if (!wav_progmem->loop()){
            digitalWrite(MUTE_PIN, MUTE); //mute amp
            wav_progmem->stop();
            return;
            }
          ESP.wdtFeed();
        }

        //LED animations
        if ((id == JINGLE_POWER_UP) && (millis() >= (anim_last_time + anim_interval))) {
          anim_last_time = millis();
          if (anim[0] < 90) anim[0]++; //this one is just for initial delay
          else if (anim[1] < 185) anim[1]++;
          else if (anim[2] < 185) anim[2]++;
          else if (anim[3] < 185) anim[3]++;
          else if (anim[4] < 185) anim[4]++;

          for (int i = 1; i<=4; i++) io.pwm(i, map(anim[i], 0, 185, 0, 254));
        }

        if ((id == JINGLE_POWER_DOWN) && (millis() >= (anim_last_time + anim_interval))) {
          anim_last_time = millis();
          if (anim[0] < 100) anim[0]++; //this one is just for initial delay
          else if (anim[4] < 185) anim[4]++;
          else if (anim[3] < 185) anim[3]++;
          else if (anim[2] < 185) anim[2]++;
          else if (anim[1] < 185) anim[1]++;

          for (int i = 4; i>0; i--) io.pwm(i, map(185-anim[i], 0, 185, 0, 254));
        }

        

    }


    //mute amp for "MUTE_MS" milliseconds into track (to avoid "click")
  
}


/*
    NOTES:
    -add delay between set and get to let line settle
    -Never read from 4-7 when expecting a button press

*/
#include <core_esp8266_si2c.cpp>

ADC_MODE(ADC_TOUT);
#define ADC_SAMPLE_COUNT    100  //read and average this many samples each time
#define ADC_CEILING_MV      1000  //mv that corresponds to 1023 in the adc
void adc_set(int pin)
{
    

  (!!(pin & 0b0001)) ? (SDA_HIGH(SDA)) : (SDA_LOW(SDA));
  (!!(pin & 0b0010)) ? (SCL_HIGH(SCLK)) : (SCL_LOW(SCLK));
  (!!(pin & 0b0100)) ? (digitalWrite(16, HIGH)) : (digitalWrite(16, LOW));
  
  //STILL NEED TO ADD 3RD CONTROL LINE...

}

//must always first be set
//returns mapped value in mv depending on pin being read
uint32_t adc_get(int pin)
{
  float adc = 0;

  //average "ADC_SAMPLE_COUNT" x readings
  for (int i = 0; i<ADC_SAMPLE_COUNT; i++){
    adc += analogRead(A0);
  } 
  adc /= (ADC_SAMPLE_COUNT/1.0);

  //convert to mv
  adc = mapf(adc, 0, 1023, 0, ADC_CEILING_MV); 

  //correct for vdivs for whatever pin being read
  switch(pin)
  {
    case ADC_PIN_USBVCC:
      adc*=11;    // 0.0909090 vdiv (100k->10k)
      break;
    
    case ADC_PIN_CHRG:
      break;
    
    case ADC_PIN_HPDET:
      break;
    
    case ADC_PIN_VCC:
      adc*=4;    // 1/4vdiv
      break;

    case ADC_PIN_DCHRG:
      break;

    case ADC_PIN_BRDID:
      break;

    case ADC_PIN_SOLAR:
      adc/=(1.0/(1.0+4.7)); 
      break;
  
  }
  return(adc);
}


//DO CERTAIN THINGS BASED ON WHAT WOKE THE DEVICE UP
void handle_wakeup()
{
  
  //Check if USB was plugged in
  adc_set(ADC_PIN_USBVCC);
  delay(10);
  if (adc_get(ADC_PIN_USBVCC) > 4500) {
    jingle(JINGLE_CHARGING, 0.99);
    charging_loop();
  }


}


//an endless charging animation for the LEDS when plugged in
//(unless power button is pressed)
void charging_loop()
{
  io.init();
  io.digitalWrite(LED1, 0);io.digitalWrite(LED2, 0);io.digitalWrite(LED3, 0);io.digitalWrite(LED4, 0);
  int vcc;
  while(1){
    adc_print_all(); //for debugging

    //read battery voltage
    adc_set(ADC_PIN_VCC);
    delay(10);
    vcc = adc_get(ADC_PIN_VCC);
    int percent = vccToPercent(vcc);
    Serial.printf("vcc = %i \t(%i percent)\n", vcc, percent);
    //animate LEDs accordingly (switch this to builtin fade eventually...)
    int breathing_LED = 1;
    if (percent == 100) breathing_LED = 5;    //doesn't exist... so wont do anything ;)
    else if (percent >= 75) breathing_LED = 4;
    else if (percent >= 50) breathing_LED = 3;
    else if (percent >= 25) breathing_LED = 2;
    else                    breathing_LED = 1;

    int i;
    for (i = 1; i<5; i++) {io.pwm(i,0);} //turn all the LEDs off
    for (i = 1; i<breathing_LED; i++) {io.pwm(i, 255);}  //turn on the ones below breathing
    for (i=0; i<255; i++) {
      //adjust pwm
      io.pwm(breathing_LED, i); 
      //check if POW was pressed
      if(!digitalRead(0)){ 
        io.update_pinData();    
        if(io.digitalRead(SW_POW)) {
          latchPower();
          for (i = 1; i<=4; i++) {io.pwm(i, 0);}  //enable all LEDs
          return;
        } else if (io.digitalRead(SW_VDOWN) && io.digitalRead(SW_VUP)) {
          readSD();
        }
          
      }
      delay(1);
    }
    adc_set(ADC_PIN_USBVCC);
    delay(10);
    if (adc_get(ADC_PIN_USBVCC) < 4500) {
      while(1){
        delay(10);
        powerDown_device();
      }
    }
    
    for (i=254; i>=0; i--) {
      //adjust pwm
      io.pwm(breathing_LED, i); 
      //check if POW was pressed
      if(!digitalRead(0)){ 
        io.update_pinData();    
        if(io.digitalRead(SW_POW)) {
          latchPower();
          return;  
        } else if (io.digitalRead(SW_VDOWN) && io.digitalRead(SW_VUP)) {
          readSD();
        }
      }
      delay(1);
    }    
  }
}



uint8_t vccToPercent(int vcc)
{
  if (vcc > 3620) return 100;
  else if (vcc > 3500) return 90;
  else if (vcc > 3400) return 75;
  else if (vcc > 3300) return 50;
  else if (vcc > 3200) return 25;
  else if (vcc > 3185) return 10;
  else if (vcc > 3050) return 5;
  else if (vcc > 2700) return 1;
  else                 return 0;
}


//in certain modes (whenever not playing a track) device should go into light sleep periodically
void radio_sleep_tick()
{
  
    Serial.printf("\nsleeping\n");

    //set pinmodes accordingly


  

  // For some reason, moving timer_list pointer to the end of the list lets us achieve light sleep
  //Serial.printf("--timers----\n");
  extern os_timer_t *timer_list;
  while(timer_list != 0) {
    /*
    Serial.printf("address_current= %p\n", timer_list);
    Serial.printf("timer_expire= %u\n", timer_list->timer_expire);
    Serial.printf("timer_period= %u\n", timer_list->timer_period);
    Serial.printf("address_next= %p\n", timer_list->timer_next);
    Serial.printf("------------\n");
    */

    // doing the actual disarm doesn't seem to be necessary, and causes stuff to not work
    //  os_timer_disarm(timer_list);
    timer_list = timer_list->timer_next;
  }
  Serial.flush();
  
    
  wifi_set_opmode_current(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  //gpio_pin_wakeup_enable(GPIO_ID_PIN(0), GPIO_PIN_INTR_HILEVEL); // or LO/ANYLEVEL, no change
  gpio_pin_wakeup_enable(GPIO_ID_PIN(0), GPIO_PIN_INTR_LOLEVEL);
  wifi_fpm_open();
  wifi_fpm_set_wakeup_cb(wakeup_cb);
  //wifi_fpm_do_sleep(0xFFFFFF);  // works but requires interupt to wake up
  wifi_fpm_do_sleep(RADIO_SLEEP_INTERVAL * 1000);
  delay (RADIO_SLEEP_INTERVAL + 1);
  
  // ~7ma for 15 seconds, then ~22ma for 15 seconds (75ma for 5 seconds without forcesleepbegin in callback)
  // because ticks slow dramatically during sleep, and delay uses ticks to know when its finished
  // (NOTE: inside the callback function, write and flush something to serial, and the delay() will be interrupted, thanks vlast3k)

  // works, but wipes memory
  //ESP.deepSleep(15000000);

  if (!radio_play) {
    rad_pause_timer += RADIO_SLEEP_INTERVAL;
    if (rad_pause_timer >= PAUSED_POWER_DOWN_TIMER) quiet_power_down();
  }

  //Check voltage
  if(digitalRead(0)){    //if wokeup from sleep timer (not a button press)
    adc_set(ADC_PIN_VCC);
    delay(10);
    int mv = adc_get(ADC_PIN_VCC);
    if (mv < LV_THRESH) {
      LV_handle();
    }
    else if (mv < LV_WARN_THRESH) {
      //make sure not charging
      adc_set(ADC_PIN_USBVCC);
      delay(10);
      if (adc_get(ADC_PIN_USBVCC) < 200) {
        //Mute Radio
        digitalWrite(MUTE_PIN, MUTE);
        delay(400);
        powerdown_radio();

        //play jingle
        jingle(JINGLE_LOWBATT, 1);

        //resume radio
        bool resume_play = radio_play;
        init_radio();
        radio_play = resume_play;
        if(!radio_play) set_rad_vol(-1);
        digitalWrite(MUTE_PIN, UNMUTE);
      }
    }

    //fade out LEDs to save power 
    if (!LED_power_save){
      Serial.println("fade leds");
      io.pwm(1, 0); io.pwm(2, 0); io.pwm(3, 0); io.pwm(4, 0); 
      for (int i = 254; i>=0; i--){ 
        if (nv.deviceVolume>=0)   io.pwm(1, i);
        if (nv.deviceVolume>=4)   io.pwm(2, i);
        if (nv.deviceVolume>=8)   io.pwm(3, i);
        if (nv.deviceVolume>=12)  io.pwm(4, i);
        delay(5);   
      }
      LED_power_save = true;
    }
  } else { //if button press woke up the device
    updateLED();
    LED_power_save = false;
  }
}

void wakeup_cb() {
  wifi_fpm_close();
  //WiFi.forceSleepBegin();  // if you dont need wifi, ~22ma instead of ~75ma
  
  // required here, otherwise the delay after sleep will not be interrupted, thanks vlast3k
  // see: #6 from https://github.com/esp8266/Arduino/issues/1381#issuecomment-279117473
  Serial.printf("wakeup_cb\n");
  Serial.flush();

}


void adc_print_all()
{
  const int settle_time = 10;
  const int high_thresh = 900;
  const int low_thresh = 100;
  int x;
  
  adc_set(ADC_PIN_USBVCC);
  delay(settle_time);
  Serial.printf("\nUSB: %imv\t", adc_get(ADC_PIN_USBVCC));

  adc_set(ADC_PIN_CHRG);
  delay(settle_time);
  x = adc_get(ADC_PIN_CHRG);
  if (x < low_thresh) Serial.printf("CHRG: 1\t\t");
  else if (x > high_thresh) Serial.printf("CHRG: 0\t\t");
  else Serial.printf("CHRG: error\t");

  adc_set(ADC_PIN_HPDET);
  delay(settle_time);
  x = adc_get(ADC_PIN_HPDET);
  if (x < low_thresh) Serial.printf("HP_DET: 0\t");
  else if (x > high_thresh) Serial.printf("HP_DET: 1\t");
  else Serial.printf("HP_DET: error\t");

  adc_set(ADC_PIN_VCC);
  delay(settle_time);
  Serial.printf("VCC: %imv\t", adc_get(ADC_PIN_VCC));

  adc_set(ADC_PIN_DCHRG);
  delay(settle_time);
  x = adc_get(ADC_PIN_DCHRG);
  if (x < low_thresh) Serial.printf("DONE_CHRG: 1\t");
  else if (x > high_thresh) Serial.printf("DONE_CHRG: 0\t");
  else Serial.printf("DONE_CHRG: error\t");

  adc_set(ADC_PIN_BRDID);
  delay(settle_time);
  Serial.printf("BRD_ID: %imv\t", adc_get(ADC_PIN_BRDID));

  adc_set(ADC_PIN_SOLAR);
  delay(settle_time);
  Serial.printf("SOLAR: %imv\t", adc_get(ADC_PIN_SOLAR));

}

bool LV_check()
{
  adc_set(ADC_PIN_VCC);
  delay(10);
  if (adc_get(ADC_PIN_VCC) < LV_THRESH) return 0;
  else return 1;
}

void LV_handle()
{
  //power down radio
  powerdown_radio();
  delay(100);

  jingle(JINGLE_LOWBATT, 1.0);       //takes ~2 seconds
  delay(500);
  Serial.println("BATTERY TO LOW, POWERING DOWN...");
    
  //update track frame last second...
  if (nv.deviceMode == TRACK_MODE) nv.trackFrame = file->getPos();

  //set all the params in nonVol memory 
  nv.set_nonVols();
  
  //SD.end();

  //play power down jingle
  jingle(JINGLE_POWER_DOWN, 0.1);       //takes ~2 seconds

  //safely end SPIFFS
  LittleFS.end();

  //power down board
  io.reset();
  io.OSCIO_set(LOW);
  delay(5000);  //wait for latch circuit to die

  //if still on at this point, USB is keeping on, 
  //so reset the device and go into charging animation...
  Serial.println("reseting device...");
  ESP.restart();
  
  //should never reach this
  while(1){}
}

float track_gain_convert()
{
  return (((nv.deviceVolume/100.0) * (nv.deviceVolume*0.5))) + 0.015; //~0.02 - 1.14
}

int radio_gain_convert()
{
  //~10 - 63
  return map(nv.deviceVolume, MIN_DEVICE_VOL, MAX_DEVICE_VOL, RADIO_MIN_GAIN, RADIO_MAX_GAIN);    
}

void pause_powerdown_check()
{
  //only for track mode...
  if (nv.deviceMode == RADIO_MODE) return;
  if (track_play) return;
  if (millis() < (pause_time + PAUSED_POWER_DOWN_TIMER)) return;

  Serial.println("paused for to long... powering down");
  quiet_power_down();
}

void quiet_power_down()
{
    
  //update track frame last second...
  if (nv.deviceMode == TRACK_MODE) nv.trackFrame = file->getPos();

  //set all the params in nonVol memory 
  nv.set_nonVols();


  //avoid "click" on radio power down
  digitalWrite(MUTE_PIN, MUTE); 
  delay(150);  
  //power down radio
  powerdown_radio();
  
  //SD.end();

  //don't play jingle....
  //jingle(JINGLE_POWER_DOWN, 0.1);       //takes ~2 seconds

  //Check if USB is plugged in
  adc_set(ADC_PIN_USBVCC);
  delay(100);
  if (adc_get(ADC_PIN_USBVCC) > 4500) {
    
    Serial.println("charging loop");
    charging_loop();

    
    Serial.println("resuming playback");
    //when returned, resume...
    //init either radio or player...
    device_init();


  } else {

    //safely end SPIFFS
    LittleFS.end();

    //power down board
    io.reset();
    io.OSCIO_set(LOW);
    delay(5000);  //wait for latch circuit to die

    //if still on at this point, USB is keeping on, 
    //so reset the device and go into charging animation...
    Serial.println("reseting device...");
    ESP.restart();
    
    //should never reach this
    while(1){}
  }
}
