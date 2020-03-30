 
#include <Wire.h> 
////////////////////////////////////////////////////////////////////////////// 
#include <SparkFunSX1509.h> // Include SX1509 library 
 
// SX1509 I2C address (set by ADDR1 and ADDR0 (00 by default): 
const byte SX1509_ADDRESS = 0x3E;  // SX1509 I2C address 
SX1509 io; // Create an SX1509 object to be used throughout 
////////////////////////////////////////////////////////////////////////////// 
#define SPI_SPEED   SD_SCK_MHZ(40) 
 
 
 
 
void setup() { 
  Serial.begin(9600); Serial.println(); Serial.println("boot"); 
  // Call io.begin(<address>) to initialize the SX1509. If it 
  // successfully communicates, it'll return 1. 
  if (!io.begin(SX1509_ADDRESS)) 
  { 
    while (1) { 
      Serial.println("can't connect..."); 
      delay(1000);    // If we fail to communicate, loop forever. 
    } 
  } 
  else { 
    Serial.println("IO initialized"); 
  } 
   
 
  
  latchPower();
 
   
  //init the rest of the IO 
  io.digitalWrite(MUX_SEL, LOW); io.pinMode(MUX_SEL, OUTPUT);     //ALWAYS ENSURE THAT THE DESIRED LEVEL IS WRITTEN BEFORE ENABLING AS OUTPUT!
  io.digitalWrite(USB_SD_RST, HIGH); io.pinMode(USB_SD_RST, OUTPUT);   
  io.pinMode(SW_MODE, INPUT); 
  io.pinMode(SW_Q, INPUT); 
  io.digitalWrite(LED1, LOW); io.pinMode(LED1, OUTPUT); 
  io.digitalWrite(LED2, LOW); io.pinMode(LED2, OUTPUT); 
  io.digitalWrite(LED3, LOW); io.pinMode(LED3, OUTPUT); 
  io.digitalWrite(LED4, LOW); io.pinMode(LED4, OUTPUT); 
  io.pinMode(SW_VDOWN, INPUT); 
  io.pinMode(SW_VUP, INPUT); 
  io.pinMode(SW_DOWN, INPUT); 
  io.pinMode(SW_UP, INPUT); 
  io.pinMode(SW_RIGHT, INPUT); 
  io.pinMode(SW_LEFT, INPUT); 
  io.pinMode(SW_PLAY, INPUT);  
  io.pinMode(SW_POW, INPUT); 
   
 
   
  //wait for power to be released 
  while(io.digitalRead(SW_POW))  
  {Serial.println("waiting to let go of POW"); 
    delay(100); 
  } 
  Serial.println("loop begins"); 
   
} 
 
int ms = 0; 
int last_ms = 0; 
void loop() {while(1){ 
 
  
 
   
  delay(1); 
  ms = millis(); 
  if (ms == last_ms) continue; 
  last_ms = ms; 
 
  if (!(ms%100)){  //check every 50ms 
     
    if (io.digitalRead(SW_POW)) { 
      powerDown(); 
    } 
 
  Serial.println("running..."); 
     
  } 
 
}} 
 
void latchPower() 
{ 
   //latch power 
  Wire.beginTransmission(SX1509_ADDRESS); 
  Wire.write(0x1E);  
  Wire.write(B01011111);         //set internal osc, set OSCIO to output, and HIGH    ///THIS KEEPS SUICIDE CIRCUIT ENABLED 
  Wire.endTransmission(); 
  Serial.println("power latched"); 
} 
 
void powerDown() 
{ 
  Serial.println("turning off power..."); 
      Wire.beginTransmission(SX1509_ADDRESS); 
      Wire.write(0x1E);  
      Wire.write(B01010000);         //set internal osc, set OSCIO to output, and HIGH    ///THIS KEEPS SUICIDE CIRCUIT ENABLED 
      Wire.endTransmission(); 
      io.digitalWrite(LED1, HIGH); io.pinMode(LED1, INPUT);  
      io.digitalWrite(LED2, HIGH); io.pinMode(LED2, INPUT);  
      io.digitalWrite(LED3, HIGH); io.pinMode(LED3, INPUT);  
      io.digitalWrite(LED4, HIGH); io.pinMode(LED4, INPUT);  
      delay(100000); 
      Serial.println("this should never be reached..."); 
} 
