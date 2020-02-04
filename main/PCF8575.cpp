

#include "PCF8575.h"
#include "Wire.h"






/**
 * Constructor
 * @param address: i2c address
 */
PCF8575::PCF8575(uint8_t address){
	_wire = &Wire;

	_address = address;
};

/**
 * Construcor
 * @param address: i2c address
 * @param interruptPin: pin to set interrupt
 * @param interruptFunction: function to call when interrupt raised
 */
PCF8575::PCF8575(uint8_t address, uint8_t interruptPin,  void (*interruptFunction)() ){
	_wire = &Wire;

	_address = address;
	_interruptPin = interruptPin;
	_interruptFunction = interruptFunction;
	_usingInterrupt = true;
};

#if !defined(__AVR) && !defined(__STM32F1__)
	/**
	 * Constructor
	 * @param address: i2c address
	 * @param sda: sda pin
	 * @param scl: scl pin
	 */
	PCF8575::PCF8575(uint8_t address, uint8_t sda, uint8_t scl){
		_wire = &Wire;

		_address = address;
		_sda = sda;
		_scl = scl;
	};

	/**
	 * Constructor
	 * @param address: i2c address
	 * @param sda: sda pin
	 * @param scl: scl pin
	 * @param interruptPin: pin to set interrupt
 	 * @param interruptFunction: function to call when interrupt raised
	 */
	PCF8575::PCF8575(uint8_t address, uint8_t sda, uint8_t scl, uint8_t interruptPin,  void (*interruptFunction)() ){
		_wire = &Wire;

		_address = address;
		_sda = sda;
		_scl = scl;

		_interruptPin = interruptPin;
		_interruptFunction = interruptFunction;

		_usingInterrupt = true;
	};
#endif

#ifdef ESP32
	/**
	 * Constructor
	 * @param address: i2c address
	 */
	PCF8575::PCF8575(TwoWire *pWire, uint8_t address){
		_wire = pWire;

		_address = address;
	};

	/**
	 * Construcor
	 * @param address: i2c address
	 * @param interruptPin: pin to set interrupt
	 * @param interruptFunction: function to call when interrupt raised
	 */
	PCF8575::PCF8575(TwoWire *pWire, uint8_t address, uint8_t interruptPin,  void (*interruptFunction)() ){
		_wire = pWire;

		_address = address;
		_interruptPin = interruptPin;
		_interruptFunction = interruptFunction;
		_usingInterrupt = true;
	};

	/**
	 * Constructor
	 * @param address: i2c address
	 * @param sda: sda pin
	 * @param scl: scl pin
	 */
	PCF8575::PCF8575(TwoWire *pWire, uint8_t address, uint8_t sda, uint8_t scl){
		_wire = pWire;

		_address = address;
		_sda = sda;
		_scl = scl;
	};

	/**
	 * Constructor
	 * @param address: i2c address
	 * @param sda: sda pin
	 * @param scl: scl pin
	 * @param interruptPin: pin to set interrupt
	 * @param interruptFunction: function to call when interrupt raised
	 */
	PCF8575::PCF8575(TwoWire *pWire, uint8_t address, uint8_t sda, uint8_t scl, uint8_t interruptPin,  void (*interruptFunction)() ){
		_wire = pWire;

		_address = address;
		_sda = sda;
		_scl = scl;

		_interruptPin = interruptPin;
		_interruptFunction = interruptFunction;

		_usingInterrupt = true;
	};
#endif

/**
 * wake up i2c controller
 */
 
 
 
 
void PCF8575::begin(){
	#if !defined(__AVR) && !defined(__STM32F1__)
		_wire->begin(_sda, _scl);
	#else
	//			Default pin for AVR some problem on software emulation
	//			#define SCL_PIN _scl
	// 			#define SDA_PIN _sda
		_wire->begin();
	#endif

//		Serial.println( writeMode, BIN);
//		Serial.println( readMode, BIN);

	// Check if there are pins to set low
	if (writeMode>0 || readMode>0){
		DEBUG_PRINTLN("Set write mode");
		_wire->beginTransmission(_address);
		DEBUG_PRINT(" ");
		DEBUG_PRINT("usedPin pin ");


		uint16_t usedPin = writeMode | readMode;
		DEBUG_PRINTLN( ~usedPin, BIN);
//		Serial.println( ~usedPin, BIN);

		_wire->write((uint8_t) ~usedPin);
		_wire->write((uint8_t) (~(usedPin >> 8)));

		DEBUG_PRINTLN("Start end trasmission if stop here check pullup resistor.");
		_wire->endTransmission();
	}

	// If using interrupt set interrupt value to pin
	if (_usingInterrupt){
		DEBUG_PRINTLN("Using interrupt pin (not all pin is interrupted)");
		::pinMode(_interruptPin, INPUT_PULLUP);
		attachInterrupt(digitalPinToInterrupt(_interruptPin), (*_interruptFunction), FALLING );
	}

	// inizialize last read
	lastReadMillis = millis();
}

/**
 * Set if fin is OUTPUT or INPUT
 * @param pin: pin to set
 * @param mode: mode, supported only INPUT or OUTPUT (to semplify)
 */
void PCF8575::pinMode(uint8_t pin, uint8_t mode){
	DEBUG_PRINT("Set pin ");
	DEBUG_PRINT(pin);
	DEBUG_PRINT(" as ");
	DEBUG_PRINTLN(mode);

	if (mode == OUTPUT){
		writeMode = writeMode | bit(pin);
		readMode =  readMode & ~bit(pin);
		DEBUG_PRINT("writeMode: ");
		DEBUG_PRINT(writeMode, BIN);
		DEBUG_PRINT("readMode: ");
		DEBUG_PRINTLN(readMode, BIN);

	}else if (mode == INPUT){
		writeMode = writeMode & ~bit(pin);
		readMode =   readMode | bit(pin);
		DEBUG_PRINT("writeMode: ");
		DEBUG_PRINT(writeMode, BIN);
		DEBUG_PRINT("readMode: ");
		DEBUG_PRINTLN(readMode, BIN);
	}
	else{
		DEBUG_PRINTLN("Mode non supported by PCF8575")
	}
	DEBUG_PRINT("Write mode: ");
	DEBUG_PRINTLN(writeMode, BIN);

};

/**
 * Read value from i2c and bufferize it
 * @param force
 */
void PCF8575::readBuffer(bool force){
	if (millis() > PCF8575::lastReadMillis+READ_ELAPSED_TIME || _usingInterrupt || force){
		_wire->requestFrom(_address,(uint8_t)2);// Begin transmission to PCF8575 with the buttons
		lastReadMillis = millis();
		if(_wire->available())   // If uint16_ts are available to be recieved
		{
			uint16_t iInput = _wire->read();// Read a uint16_t
			iInput |= _wire->read() << 8;// Read a uint16_t
			if ((iInput & readMode)>0){
				byteBuffered = byteBuffered | (uint16_t)iInput;
			}
		}
	}
}


	/**
	 * Read value of all INPUT pin in byte format for low memory usage
	 * Debounce read more fast than 10millis, non managed for interrupt mode
	 * @return
	 */
	uint16_t PCF8575::digitalReadAll(void){
		DEBUG_PRINTLN("Read from buffer");
		_wire->requestFrom(_address,(uint8_t)2);// Begin transmission to PCF8575 with the buttons
		lastReadMillis = millis();
		if(_wire->available())   // If uint16_ts are available to be recieved
		{
			  DEBUG_PRINTLN("Data ready");
			  uint16_t iInput = _wire->read();// Read a uint16_t
				iInput |= _wire->read() << 8;// Read a uint16_t

			  if ((iInput & readMode)>0){
				  DEBUG_PRINT("Input ");
				  DEBUG_PRINTLN((uint16_t)iInput, BIN);

				  byteBuffered = byteBuffered | (uint16_t)iInput;
				  DEBUG_PRINT("byteBuffered ");
				  DEBUG_PRINTLN(byteBuffered, BIN);
			  }
		}

		DEBUG_PRINT("Buffer value ");
		DEBUG_PRINTLN(byteBuffered, BIN);

		uint16_t byteRead = byteBuffered;

		if ((readMode & byteBuffered)>0){
			byteBuffered = ~readMode & byteBuffered;
			DEBUG_PRINT("Buffer hight value readed set readed ");
			DEBUG_PRINTLN(byteBuffered, BIN);
		}
		DEBUG_PRINT("Return value ");
		return byteRead;
	};


/**
 * Read value of specified pin
 * Debounce read more fast than 10millis, non managed for interrupt mode
 * @param pin
 * @return
 */
uint8_t PCF8575::digitalRead(uint8_t pin){
	uint8_t value = LOW;
	DEBUG_PRINT("Read pin ");
	DEBUG_PRINTLN(pin);
	// Check if pin already HIGH than read and prevent reread of i2c
	if ((bit(pin) & byteBuffered)>0){
		DEBUG_PRINTLN("Pin already up");
		value = HIGH;
	 }else if ((/*(bit(pin) & byteBuffered)<=0 && */millis() > PCF8575::lastReadMillis+READ_ELAPSED_TIME) /*|| _usingInterrupt*/){
		 DEBUG_PRINTLN("Read from buffer");
		  _wire->requestFrom(_address,(uint8_t)2);// Begin transmission to PCF8575 with the buttons
		  lastReadMillis = millis();
		  if(_wire->available())   // If bytes are available to be recieved
		  {
			  DEBUG_PRINTLN("Data ready");
			  uint16_t iInput = _wire->read();// Read a uint16_t
				iInput |= _wire->read() << 8;// Read a uint16_t

//				Serial.println(iInput, BIN);

			  if ((iInput & readMode)>0){
				  DEBUG_PRINT("Input ");
				  DEBUG_PRINTLN((uint16_t)iInput, BIN);

				  byteBuffered = byteBuffered | (uint16_t)iInput;
				  DEBUG_PRINT("byteBuffered ");
				  DEBUG_PRINTLN(byteBuffered, BIN);

				  if ((bit(pin) & byteBuffered)>0){
					  value = HIGH;
				  }
			  }
		  }
	}
	DEBUG_PRINT("Buffer value ");
	DEBUG_PRINTLN(byteBuffered, BIN);
	// If HIGH set to low to read buffer only one time
	if (value==HIGH){
		byteBuffered = ~bit(pin) & byteBuffered;
		DEBUG_PRINT("Buffer hight value readed set readed ");
		DEBUG_PRINTLN(byteBuffered, BIN);
	}
	DEBUG_PRINT("Return value ");
	DEBUG_PRINTLN(value);
	return value;
};

/**
 * Write on pin
 * @param pin
 * @param value
 */
void PCF8575::digitalWrite(uint8_t pin, uint8_t value){
	DEBUG_PRINTLN("Begin trasmission");
	_wire->beginTransmission(_address);     //Begin the transmission to PCF8575
	if (value==HIGH){
		writeByteBuffered = writeByteBuffered | bit(pin);
	}else{
		writeByteBuffered = writeByteBuffered & ~bit(pin);
	}
	DEBUG_PRINT("Write data ");
	DEBUG_PRINT(writeByteBuffered, BIN);
	DEBUG_PRINT(" for pin ");
	DEBUG_PRINT(pin);
	DEBUG_PRINT(" bin value ");
	DEBUG_PRINT(bit(pin), BIN);
	DEBUG_PRINT(" value ");
	DEBUG_PRINTLN(value);

//	Serial.print(" --> ");
//	Serial.println(writeByteBuffered);
//	Serial.println((uint8_t) writeByteBuffered);
//	Serial.println((uint8_t) (writeByteBuffered >> 8));

	writeByteBuffered = writeByteBuffered & writeMode;
	_wire->write((uint8_t) writeByteBuffered);
	_wire->write((uint8_t) (writeByteBuffered >> 8));
	DEBUG_PRINTLN("Start end trasmission if stop here check pullup resistor.");

	_wire->endTransmission();
};


