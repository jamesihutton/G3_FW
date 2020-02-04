/*
 * PCF8575 GPIO Port Expand
 * https://www.mischianti.org/2019/07/22/pcf8575-i2c-16-bit-digital-i-o-expander/
 *

 */

#ifndef PCF8575_h
#define PCF8575_h

#include "Wire.h"

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// Uncomment to enable printing out nice debug messages.
// #define PCF8575_DEBUG

// Uncomment for low memory usage this prevent use of complex DigitalInput structure and free 7byte of memory
// #define PCF8575_LOW_MEMORY

// Define where debug output will be printed.
#define DEBUG_PRINTER Serial

// Setup debug printing macros.
#ifdef PCF8575_DEBUG
	#define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
	#define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
	#define DEBUG_PRINT(...) {}
	#define DEBUG_PRINTLN(...) {}
#endif

#define READ_ELAPSED_TIME 10

//#define P0  	B00000001
//#define P1  	B00000010
//#define P2  	B00000100
//#define P3  	B00001000
//#define P4  	B00010000
//#define P5  	B00100000
//#define P6  	B01000000
//#define P7  	B10000000
//
#define P0  	0
#define P1  	1
#define P2  	2
#define P3  	3
#define P4  	4
#define P5  	5
#define P6  	6
#define P7  	7
#define P8  	8
#define P9  	9
#define P10  	10
#define P11  	11
#define P12  	12
#define P13  	13
#define P14  	14
#define P15  	15

#include <math.h>



//GALCOM DEFINES!
#define MUX_SEL       			P0
#define AMP_SHUTOFF   		P1
#define RADIO_SHUTOFF 		P2
#define RADIO_RST     			P3

#define LED1          P4
#define LED2          P5
#define LED3          P6
#define LED4          P7


#define SW_POW        	P10
#define SW_PLAY       	P11
#define SW_LEFT       	P12
#define SW_RIGHT      	P13
#define SW_UP         		P14
#define SW_DOWN       	P15
#define SW_VUP        	P16
#define SW_VDOWN      P17



//MASKS
#define SW_POW_MASK       	B00000001
#define SW_PLAY_MASK      	B00000010
#define SW_LEFT_MASK      	B00000100
#define SW_RIGHT_MASK     	B00001000
#define SW_UP_MASK        	B00010000
#define SW_DOWN_MASK      B00100000
#define SW_VUP_MASK       	B01000000
#define SW_VDOWN_MASK    B10000000
//////////////////////////////////////////


class PCF8575 {
public:

	PCF8575(uint8_t address);
	PCF8575(uint8_t address, uint8_t interruptPin,  void (*interruptFunction)() );

#if !defined(__AVR) && !defined(__STM32F1__)
	PCF8575(uint8_t address, uint8_t sda, uint8_t scl);
	PCF8575(uint8_t address, uint8_t sda, uint8_t scl, uint8_t interruptPin,  void (*interruptFunction)());
#endif

#ifdef ESP32
	///// changes for second i2c bus
	PCF8575(TwoWire *pWire, uint8_t address);
	PCF8575(TwoWire *pWire, uint8_t address, uint8_t sda, uint8_t scl);

	PCF8575(TwoWire *pWire, uint8_t address, uint8_t interruptPin,  void (*interruptFunction)() );
	PCF8575(TwoWire *pWire, uint8_t address, uint8_t sda, uint8_t scl, uint8_t interruptPin,  void (*interruptFunction)());
#endif

	void begin();
	void pinMode(uint8_t pin, uint8_t mode);

	void readBuffer(bool force = true);
	uint8_t digitalRead(uint8_t pin);

		uint16_t digitalReadAll(void);

	void digitalWrite(uint8_t pin, uint8_t value);

private:
	uint8_t _address;

	#if defined(__AVR) || defined(__STM32F1__)
		uint8_t _sda;
		uint8_t _scl;
	#else
		uint8_t _sda = SDA;
		uint8_t _scl = SCL;
	#endif

	TwoWire *_wire;

	bool _usingInterrupt = false;
	uint8_t _interruptPin = 2;
	void (*_interruptFunction)(){};

	uint16_t writeMode 	= 	0;
	uint16_t readMode 	= 	0;
	uint16_t byteBuffered = 0;
	unsigned long lastReadMillis = 0;

	uint16_t writeByteBuffered = 0;

};

#endif
