 
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



static const int fm_addr = B1100011;   //i2c address for si4734
int SDIO = D2;
int SCLK = D1;

#define MUTE_PIN D0



int channel = 9470;
#define fm_max 10790
#define fm_min  6410
char rdsBuffer[10];

#define TRACK_MAX_GAIN    1.5   //
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

bool track_play = false;
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


  track_gain = mapf(nv.deviceVolume, 0, MAX_DEVICE_VOL, TRACK_MIN_GAIN, TRACK_MAX_GAIN);
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
  
  //RESET radio
  io.digitalWrite(USB_SD_RAD_RST, LOW);
  delay(10);
  io.digitalWrite(USB_SD_RAD_RST, HIGH);

  Wire.begin(SDIO, SCLK);  //SDA, SCL
  delay(200); //remove?

  //POINT TO ADDRESS TO READ FROM
  Wire.beginTransmission(fm_addr);
  Wire.write(B00000001);  //0x01  CMD: POWERUP
  Wire.write(B00010000);  //0x10 //external crystal
  Wire.write(0x05);  //0x05
  if(Wire.endTransmission()) return 0;  //NACK... failed to connect
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
  set_rad_chan(nv.radioChannel);

  radio_play = true;

  delay(MUTE_RADIO_MS);
  digitalWrite(MUTE_PIN, UNMUTE);

  return 1; //success

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
  digitalWrite(MUTE_PIN, MUTE);
  powerdown_radio();
  SD.begin(SD_CS, SPI_SPEED); 
  delay(100);
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
  
  // Set pinModes
  io.init();
  pinMode(D3, INPUT); //!IO_INT pin as input

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



  Serial.println("power - up jingle");
  //play power up jingle
  jingle(JINGLE_POWER_UP, DEFAULT_JINGLE_GAIN);       //takes ~2 seconds



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
  if(SPIFFS.begin())  Serial.println("SPIFFS Initialize....ok");
  else                Serial.println("SPIFFS Initialization...failed");
 
  //Format File System
  if(io.digitalRead(SW_Q)){
    if(SPIFFS.format()) Serial.println("File System Formated");
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
      if(nv.deviceMode == RADIO_MODE)  sleep_tick();
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
          nv.radioChannel += 20;
          if (nv.radioChannel > fm_max+1) nv.radioChannel = fm_min;
          set_rad_chan(nv.radioChannel);
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
          nv.radioChannel -= 20;
          if (nv.radioChannel < fm_min-1) nv.radioChannel = fm_max;
          set_rad_chan(nv.radioChannel);
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
        Serial.println("power down sequence...");
        
        //update track frame last second...
        if (nv.deviceMode == TRACK_MODE) nv.trackFrame = file->getPos();

        //set all the params in nonVol memory 
        nv.set_nonVols();

        //power down radio
        powerdown_radio();
        
        //SD.end();

        //play power down jingle
        jingle(JINGLE_POWER_DOWN, DEFAULT_JINGLE_GAIN);       //takes ~2 seconds

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
          SPIFFS.end();

          //power down board
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
    }
    out_progmem = new AudioOutputI2S();
    wav_progmem = new AudioGeneratorWAV();
    wav_progmem->begin(file_progmem, out_progmem);
    out_progmem->SetGain(gain);

    //mute for a few ms to avoid pop
    int start_time = millis();
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
#define ADC_CEILING_MV      972  //mv that corresponds to 1023 in the adc
void adc_set(int pin)
{
    

  (!!(pin & 0b0001)) ? (SDA_HIGH(SDA)) : (SDA_LOW(SDA));
  (!!(pin & 0b0010)) ? (SCL_HIGH(SCLK)) : (SCL_LOW(SCLK));
  
  //STILL NEED TO ADD 3RD CONTROL LINE...

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
      return(adc*11);    // 0.0909090 vdiv (100k->10k)
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
  int vcc;
  while(1){
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
      if(!digitalRead(D3)){ 
        io.update_pinData();    
        if(io.digitalRead(SW_POW)) {
          latchPower();
          for (i = 1; i<=4; i++) {io.pwm(i, 255);}  //enable all LEDs
          return;
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
      if(!digitalRead(D3)){ 
        io.update_pinData();    
        if(io.digitalRead(SW_POW)) {
          latchPower();
          return;  
        }
      }
      delay(1);
    }    
  }
}



uint8_t vccToPercent(int vcc)
{
  if (vcc > 3620) return 100;
  else if (vcc > 3400) return 90;
  else if (vcc > 3320) return 75;
  else if (vcc > 3290) return 50;
  else if (vcc > 3240) return 25;
  else if (vcc > 3185) return 10;
  else if (vcc > 3050) return 5;
  else if (vcc > 2700) return 1;
  else                 return 0;
}


//in certain modes (whenever not playing a track) device should go into light sleep periodically
void sleep_tick()
{
  
    Serial.printf("\nsleeping\n");
  

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
  wifi_fpm_do_sleep(120000 * 1000);
  delay (120001);
  
  // ~7ma for 15 seconds, then ~22ma for 15 seconds (75ma for 5 seconds without forcesleepbegin in callback)
  // because ticks slow dramatically during sleep, and delay uses ticks to know when its finished
  // (NOTE: inside the callback function, write and flush something to serial, and the delay() will be interrupted, thanks vlast3k)

  // works, but wipes memory
  //ESP.deepSleep(15000000);
}

void wakeup_cb() {
  wifi_fpm_close();
  //WiFi.forceSleepBegin();  // if you dont need wifi, ~22ma instead of ~75ma
  
  // required here, otherwise the delay after sleep will not be interrupted, thanks vlast3k
  // see: #6 from https://github.com/esp8266/Arduino/issues/1381#issuecomment-279117473
  Serial.printf("wakeup_cb\n");
  Serial.flush();
}
