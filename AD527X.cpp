/*
    AD527X.cpp

    Created on: 2019 June 25
        Author: andraws
        Contributor: bboyes

 */

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include "AD527X.h"
#include <inttypes.h>

/**
 * Constructor 
 * @param bits of the IC | 8 or 12 |
 * @parma Resistance value | 20, 50, 100 |
 * @param I2C base address 
 * 
 */

AD527X::AD527X(uint8_t bytes, uint8_t resistance, uint8_t base)
{
	_bytes = bytes;
    _base = base;
	_MaxResistance = resistance;
    BaseAddr = base;
}

/*
@return 0 if no I2c Errors, else the error count, also opens RDAC wiper
@param resistance: 20, 50, 100
 */
void AD527X::begin(boolean enableWiper){ 
	if(_bytes == 8){ // sets byte size 
		_maxValue = 255;
		_value = 255;
	}else if(_bytes == 12){
		_maxValue = 1023;
		_value = 1023;
	}else{
		error_count++;
	}
	if(_MaxResistance != 20000 || _MaxResistance != 50000 || _MaxResistance != 100000) // if resistance value not valid, increase error
		error_count++;

	_MaxResistance = _MaxResistance * 1000;
	command_write(CONTROL_WRITE, RDAC_WIPER_WRITE_ENABLE);
	setSettingValue(_value);
	return error_count;
}


/*
	Sets new value into RDAC register, and changes wiepr position.
	This command is supported only after enabling the wiper position change.
	@param inputValue 8 bit value to set wiper position
 */
void AD527X::setSettingValue(uint16_t inputValue){ // done
	_value = limit(inputValue);
	command_write( RDAC_WRITE,  _value); 
}

/*
	Increment wiper setting by 1, if at max or min, stays at those positions
 */
void AD527X::increment(){ // increment by 1
	_value = limit((_value+1));
	setSettingValue(_value);
}

/*
	Decrement wiper setting by 1, if at max or min, stays at those positions
 */
void AD527X::decrement() { // decrement by 1
	_value = limit((_value-1));
	setSettingValue(_value);
}

/*
	Gets max setting
 */
uint16_t AD527X::getMaximumSetting(){ 
	// TODO::go through the IC and try to increment until we get to max??
	return _maxValue;
}

// sets rheostat max resistance 
/* 
void AD527X::setMaxResistance(uint32_t setMaxResistance){
	_MaxResistance = setMaxResistance;
}
*/

// _value = (R/Rmax)*1024
/*
	Set resistance of the IC
	setting = (Resistance/Rmax)*1023
 */
void AD527X::setResistance(uint32_t targetResistance){
	_value = limit(float(targetResistance)/float(_MaxResistance)*1024);
	//Serial.println(_value);
	_resistance = (float(_value)/float(1023))*_MaxResistance;
	setSettingValue(_value);
}	

// return rounded targetResistance
uint32_t AD527X::getResistance(){
	_resistance = (float(_value)/float(1023)*_MaxResistance);
	return _resistance;
}

/*
	Helper function to set limit of setting
 */
uint16_t AD527X::limit(uint16_t value){
	if(value <= _minValue) {
		return _minValue;
	}else if(value >= _maxValue){
		return _maxValue;
	}else 
		return value;
}

/*
	Command number 7: This command unlocks the RDAC register to control potentiometer.
	Only call this function after initialisation phase. Enables wiper position change bit.
 */
void AD527X::enableWiperPosition(){
	control_write_verified(RDAC_WIPER_WRITE_ENABLE);
}

/*
	Command 1 from Datasheet
	Reads the current setting of the RDAC register.
	@return if positive it is read data; at most the 10 lsb are meaningful
   if negative, it's the count of errors where more negative is more errors
   if -100 the only error is "bad command", and no read was attempted
   if a smallish but negative number there were I2C errors on attempted read, and the absolute value is the error count
 */
int AD527X::getSetting(){
	return command_read(RDAC_READ, 0);
}

void AD527X::shutdown(){
	command_write( SHUTDOWN,  0);
}




/*
	Write commands to the AD527X
	COMMANDS: 
	RDAC_WRITE write lsb 10- or 8- bit data to RDAC (Must unlock first!)
	TP_WRITE store RDAC value to next 50-TP location (it must be unlocked to allow this)
	RDAC_REFRESH (software rest) 
	CONTROL_WRITE write data bits 2:0 to the control register
	SHUTDOWN set data bit 0 to shutdown, clear it for normal operation

	@param command - the type of write command 

	@param write_datum16 - the data we will use if the command supports. 10 lsb are used, we clip off any data in bits 15..10

	@return boolean if command and data were written
 */
boolean AD527X::command_write(uint8_t command, uint16_t write_datum16){
	uint8_t error_count = 0;
	uint8_t data = 0;
	uint8_t multiplier = 0; 
	uint16_t step = 0;
	int8_t return_val = 0;
	int8_t count = 0;
	uint16_t data_to_write = 0;
	boolean flag = true;

	if (write_datum16 > 0x3FF)
	{
		// data in bits 13:10 will clobber the command when we OR in write_datum16
		write_datum16 &= 0x3FF;	// clip off any bad high bits even though we are going to error out and not use them
		// this is an error in the code using this function, we should not proceed with the write
		error_count |= 0x10;
	}

	if ( (RDAC_WRITE == command) || (CONTROL_WRITE == command) || (SHUTDOWN == command) )
	{
		// these commands need to use data we send over
		// shift the command over into bits 13:10
		data_to_write = command<<10;

		// also need to send 10-bit or 8-bit wiper value, or 3 control bits, or shutdown bit
		data_to_write |= write_datum16;
	}
	else if ( (TP_WRITE == command) || (RDAC_REFRESH == command) )
	{
		// shift the command over into bits 13:10
		data_to_write = command<<10;
		// no data needed
	}
	else
	{
		// It's either a bad command (out of range, > SHUTDOWN), or its not a writeable command
		// Bad command, we can't reasonably proceed
		error_count |= 0x20;
	}

	// if no errors so far, command is valid and datum is too
	if (0 == error_count)
	{
		Wire.beginTransmission(_base);
		data = (data_to_write >> 8);		// ms byte to write
		count = Wire.write(data);
		data = data_to_write & 0x0FF;		// ls byte to write
		count += Wire.write(data);
		if (count!=2) error_count += (2-count);	// add number of bad writes

		error_count+= Wire.endTransmission(true);
	}

	if(RDAC_WRITE == command){
		_value = readRDAC();
	}

	// if errors, return the count, make it negative first
	if (error_count > 0)
		{
		flag = false;
		}

	return flag;
}

/**
 * Send a command read and request data from the AD5274
 * To read, we send a  read command and (possibly) needed location data
 *
 * @param command valid values are
 * RDAC_READ - reads current wiper value
 * TP_WIPER_READ also needs six bits of location data (which of 50, 0x01 to 0x32)
 * TP_LAST_READ - read the most recently programmed 50-TP location
 * CONTROL_READ - read the value of the control register, in 4 lsbs
 *
 * @param write_datum is only used with read of 50-TP wiper memory, and only six lsb are used
 *
 * @return if positive it is read data; at most the 10 lsb are meaningful
 * if negative, it's the count of errors where more negative is more errors
 * if -100 the only error is "bad command", and no read was attempted
 * if a smallish but negative number there were I2C errors on attempted read, and the absolute value is the error count
 *
 *
 */
int16_t AD527X::command_read(uint8_t command, uint8_t write_datum)
{
	uint8_t error_count = 0;
	uint8_t data = 0;
	uint16_t data_to_write = 0;
	int16_t read_datum = 0;

	// this will hold two data bytes as they are read
	uint8_t recvd = 0xFF;	// init with impossible value
	int8_t count = 0;

	if (TP_WIPER_READ == command)
	{
		// shift the command over into bits 13:10
		data_to_write = command<<10;
		// also need to send 6-bit 50-TP location
		data_to_write |= write_datum;
	}
	else if ( (RDAC_READ == command) || (TP_LAST_USED == command) || (CONTROL_READ == command) )
	{
		// command is in range and something possible to read
		// shift the command over into bits 13:10
		data_to_write = command<<10;
	}
	else
	{
		// It's either a bad command (out of range, > SHUTDOWN), or its not a readable command
		// Bad command, we can't reasonably proceed
		error_count = 100;
		read_datum = error_count * -1;
	}

	/**
	 * At this point, if error_count == 0 we have a valid read command
	 */
	if (0 == error_count)
	{
//		Serial.println(" No errors so far");
		Wire.beginTransmission(_base);		// I2C slave address
		data = (data_to_write >> 8);		// ms byte to write
		count = Wire.write(data);
		data = data_to_write & 0x0FF;		// ls byte to write
		count += Wire.write(data);
		if (count!=2) error_count += (2-count);	// add number of bad writes
		error_count+= Wire.endTransmission(true);

//		Serial.print(" Error count after command write=");
//		Serial.println(error_count);

		// now request the read data from the control register
		count = Wire.requestFrom(_base, (uint8_t)2, (uint8_t) true);
		if (2 != count)
		{
			Serial.print(" Error: I2C command ");
			Serial.print(command);
			Serial.print(" read didn't return 2 but ");
			Serial.println(count);
			if (count!=2) error_count += (2-count);	// add number of bad reads
		}
		recvd = Wire.read();	// ms byte
		read_datum = recvd << 8;

		recvd = Wire.read();	// ls byte
		read_datum |= recvd;

		// if errors, return the count, make it negative first
		if (error_count > 0)
			{
			//Serial.print(" Error count after command_read =");
			//Serial.println(error_count);
			read_datum = -1 * error_count;
			}
	}

	return read_datum;
}

/**
 * Write to the control register, then read it back and verify
 * the read data matches the write data.
 *
 * @param control only 3 lsb are used, others ignored but if nonzero increments error count.
 * This can be a value or a combination of manifest constants. The default, POR value for
 * these bits is 000
 *
 * @see TP_WRITE_ENABLE
 * @see RDAC_WIPER_WRITE_ENABLE
 * @see RDAC_CALIB_DISABLE
 *
 * @return 0 if successful, nonzero for total number of errors which occurred
 *
 * @TODO this seems redundant. Instead reuse the more generic command_write function.
 */
int8_t AD527X::control_write_verified(uint8_t control)
{
	uint8_t error_count = 0;
	uint8_t data = 0;

	uint8_t recvd = 0xFF;	// init with impossible value
	int8_t count = 0;
	uint16_t data_to_write = 0;

	if (control > 7)	// bad value, what is caller's intent?
	{
		// AD5274 will ignore all bits except 2:0 so we can proceed
		// but this is still not good, so bump the error counter
		error_count++;
	}
	data_to_write = (CONTROL_WRITE<<10) | control;
	Wire.beginTransmission(_base);
	data = (data_to_write >> 8);		// ms byte to write
	count = Wire.write(data);
	data = data_to_write & 0x0FF;		// ls byte to write
	count += Wire.write(data);
	if (count!=2) error_count += (2-count);	// add number of bad writes

	error_count+= Wire.endTransmission(true);


	// now see if control reg was correctly written by reading it

	// write the control read command, data except the command value are don't cares
	data_to_write = CONTROL_READ<<10;
	Wire.beginTransmission(_base);
	data = (data_to_write >> 8);		// ms byte to write
	count = Wire.write(data);
	data = data_to_write & 0x0FF;		// ls byte to write
	count += Wire.write(data);
	if (count!=2) error_count += (2-count);	// add number of bad writes

	error_count+= Wire.endTransmission(true);

	// now request the read data from the control register
	count = Wire.requestFrom(_base, (uint8_t)2, (uint8_t) true);

	if (2 != count)
	{
		Serial.print(" Error: I2C control read didn't return 2 but ");
		Serial.println(count);
		error_count += (2-count);	// add number of bad reads
	}
	recvd = Wire.read();	// ms byte is don't care, discard this
	recvd = Wire.read();	// ls byte bits 2:0 holds the control reg value

	if (recvd != control)
		{
			Serial.print(" Error: Control reg write and read don't match: ");
			Serial.print(control, HEX);
			Serial.print("/");
			Serial.println(recvd, HEX);
		}

	Serial.print("Control register value now ");
	Serial.println(recvd, HEX);

	return error_count;
}