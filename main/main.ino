#include <SparkFunSi4703.h>
//#include <tpa2016.h>
#include <Wire.h>
#include "PCF8575.h"

#include "tpa2016.h"

#include <SPI.h>
#include <SD.h>
// You may need a fast SD card. Set this as high as it will work (40MHz max).
//#define SPI_SPEED   SD_SCK_MHZ(40)

#include "string.h"
File root;


#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include "SPIFFS.h"
#else
  #include <ESP8266WiFi.h>
#endif

const bool WiFiEnabled = false;


  WiFiServer server(80);
  // Variable to store the HTTP request
  String header;
  // Decode HTTP GET value
  String valueString = String(5);
  int pos1 = 0;
  int pos2 = 0;

//const char* ssid = "FBI Van 003";
//const char* password = "qwertyuiop";

//const char* ssid = "Hope-Bible";
//const char* password = "Th@nk.You1000!";

//const char* ssid = "Galcom Guests";
//const char* password = "GoodGoodTea";

const char* ssid = "Galcom Staff";
const char* password = "RadioActiveLifeChangingMedia";


#include "AudioFileSourceSD.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;



#define pcf_addr B0100000
#define tpa_addr B1011000
#define fm_addr  0x10 

int resetPin = D0;  //GPIO16 HOTWIRE
int SDIO = D2;
int SCLK = D1;

tpa tpa;

Si4703_Breakout radio(resetPin, SDIO, SCLK);
int channel = 947;
int volume = 5;
char rdsBuffer[10];

float mp3_gain = 0.5;
int8_t amp_gain = 0;

PCF8575 pcf8575(pcf_addr,SDIO,SCLK);
void displayInfo();

String mp3_name[100];
int mp3_count = 0;
int mp3_index = 0;

String folder_name[70];
int folder_count = 0;
int folder_index = 0;

bool mp3_initialized = 0; //only happens first time

bool mp3_play = true;



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
  Serial.println(s);
  audioLogger = &Serial;  //not needed
  file = new AudioFileSourceSD(s);
  id3 = new AudioFileSourceID3(file);
  out = new AudioOutputI2S();
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

void init_server()
{
  // Connect to Wi-Fi network with SSID and password
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
 
  WiFi.begin(ssid, password);
  int counter = 0;
  bool fail_connect = false;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    counter++;
    if(counter>80) {  //timeout after 8 seconds
      Serial.println();
      Serial.println("could not connect...");
      return;
    }
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
 
  // Start the server
  server.begin();
  Serial.println("Server started");
 
  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void server_tick()
{
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    //Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; margin-left:auto; margin-right:auto;}");
            client.println(".slider { width: 300px; }</style>");
            client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
                     
            // Web Page
            client.println("</head><body><h1>RADIO VOLUME CONTROL</h1>");
            client.println("<p>Volume: <span id=\"servoPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"15\" class=\"slider\" id=\"servoSlider\" onchange=\"servo(this.value)\" value=\""+valueString+"\"/>");
            
            client.println("<script>var slider = document.getElementById(\"servoSlider\");");
            client.println("var servoP = document.getElementById(\"servoPos\"); servoP.innerHTML = slider.value;");
            client.println("slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }");
            client.println("$.ajaxSetup({timeout:1000}); function servo(pos) { ");
            client.println("$.get(\"/?value=\" + pos + \"&\"); {Connection: close};}</script>");
           
            client.println("</body></html>");     
            
            //GET /?value=180& HTTP/1.1
            if(header.indexOf("GET /?value=")>=0) {
              pos1 = header.indexOf('=');
              pos2 = header.indexOf('&');
              valueString = header.substring(pos1+1, pos2);
              
              //SET VOLUME
              int val = valueString.toInt();
              volume = val;
              if (device_mode == MP3_MODE) { 
                tpa.setGain(map(volume,0,15,-28,30));
                Serial.println(volume);
                updateLED();
              } else if (device_mode == RADIO_MODE) {
                radio.setVolume(volume);
                Serial.println(volume);
                updateLED();
              }
              
              
            }         
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}



void setup()
{
  Serial.begin(9600);

                                                                       

  if (WiFiEnabled) {
    init_server();
  } else {
    WiFi.mode(WIFI_OFF); 
    WiFi.forceSleepBegin(); //<-- saves like 100mA!
  }
  
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

  if (mp3_play){
    if (mp3->isRunning()) {
      if (!mp3->loop()) { //play next file 
        mp3_index++; 
        if (mp3_index >= (mp3_count)) mp3_index = 0;  //loop back to 0 after last song 
        init_mp3();
      }
    }
  }
  
  ms = millis();
  if (ms == last_ms) continue;
  last_ms = ms;

  
  if (!(ms%20)){


    if (WiFiEnabled) server_tick();
    
  }

  if (!(ms%50)){

                                                         
    /*    
    Wire.requestFrom(pcf_addr,2);
    
    pcf_byte[0] = Wire.read(); pcf_byte[1] = Wire.read();
    
    
    if (pcf_byte[1] != last_pcf_byte[1]) {
        if(pcf_byte[1] & SW_VUP_MASK){
          if (device_mode == MP3_MODE) {
            volume ++;
            if (volume == 16) volume = 15;
            mp3_gain = ((volume*volume*volume)/1000.0)+(volume/30.0);
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
        if(pcf_byte[1] & SW_VDOWN_MASK){
          if (device_mode == MP3_MODE) {
            volume --;
            if (volume < 0) volume = 0;     
            mp3_gain = ((volume*volume*volume)/1000.0)+(volume/30.0);
            out->SetGain(mp3_gain);
            Serial.print(volume); Serial.print(" ("); Serial.print(mp3_gain); Serial.println(")");
            updateLED();
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
            channel+=2; 
            if (channel > 1079) channel = 879;
            radio.setChannel(channel);
            displayInfo();
          }
          
        }
        if(pcf_byte[1] & SW_LEFT_MASK){
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
        if(pcf_byte[1] & SW_UP_MASK){
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
        if(pcf_byte[1] & SW_DOWN_MASK){
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
              
              
            }else{
              mp3_play = true;
              
            }
          } else if (device_mode == RADIO_MODE) {
            radio.setVolume(0);
          }
          
        }
    }
    last_pcf_byte[1] = pcf_byte[1];
    
    
    
    
    delay(1); //removing this makes the watchdog trip in radio mode....idk...reduce?  
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
