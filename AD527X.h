 /******************************************************************************/
/*!
	
	@author		Andrew Shanaj (WPI)
    
	v1.0	2018 Jun 25, Start based on working Arduino .ino program

*/
/******************************************************************************/

/** --------  Description ------------------------------------------------------
 *
 * AD5274 is a digital potentiometer with 8-bits of resolution
 * AD5272 is 10-bit version
 *
 * Full scale resistance is 10K, 20K, or 100K

  When new and nothing has been programmed into eeprom,
  it is set at 50% = 10K . Since we have resistors above and below it to set the min and max
  range, the set point is not just a linear function of the pot value. For example
  with R1 = 75K and R7 = 120 ohms, the Rpot of zero gives 5 mA drive,
  50% Rpot gives 196 mA, and 100% gives 349 mA.
  This makes things more complicated than a simple DAC, but also lets us scale the output
  range in ways that would be harder with a DAC.

  With the ADDR input grounded our device 7-bit address is B0101111 and since Arduino Wire
  library expects a 7-bit address it should be happy with that.
  Commands are sent msb first, upper two bits are 00 then a 4-bit command, then 10 bits of data;
  16 bits total. For the 8-bit AD5274 the two lsb of data are don't care.

 Commands:
 00 0001 [10-bit data] writes the data to the RDAC. This can be done unlimited times.

The AD527[2|4] has one address pin which controls the two lsb of the address.
ADDR	I2C base
GND 	0x2F
Vdd 	0x2C
N/C 	0x2E (unipolar power only)

------------------------------------------------------------------------------*/

#include <Arduino.h>
#include "Wire.h"

#ifndef AD527X_h
#define AD527X_h

/**
 * I2C ADDRESS CONSTANS 
 * I2C addresses of AD5274 depend on ADDR pin connection 
 */

#define ADDRESS_GND 0x2F	/**< Default I2C address iff ADDR=GND, see notes above */
#define ADDRESS_VDD 0x2C	/**< I2C address iff ADDR=VDD, see notes above */
#define ADDRESS_FLOAT 0x2E	/**< I2C address iff ADDR=floating, see notes above */

/**
 * COMMAND CONSTANTS
 * See Table 12, page 21, in the Rev D datasheet
 * Commands are 16-bit writes: bits 15:14 are 0s
 * Bits 13:10 are the command value below
 * Bits 9:0 are data for the command, but not all bits are used with all commands
 */

// The NOP command is included for completeness. It is a valid I2C operation.
#define COMMAND_NOP 0x00	// Do nothing. Why you would want to do this I don't know

// write the 10 or 8 data bits to the RDAC wiper register (it must be unlocked first)
#define RDAC_WRITE 0x01

#define RDAC_READ 0x02	// read the RDAC wiper register

#define TP_WRITE 0x03	// store RDAC setting to 50-TP

// SW reset: refresh RDAC with last 50-TP stored value
// If not 50-TP value, reset to 50% I think???
// data bits are all dont cares
#define RDAC_REFRESH 0x04	// TODO refactor this to AD5274_SOFT_RESET

// read contents of 50-TP in next frame, at location in data bits 5:0,
// see Table 16 page 22 Rev D datasheet
// location 0x0 is reserved, 0x01 is first programmed wiper location, 0x32 is 50th programmed wiper location
#define TP_WIPER_READ 0x05

/**
 * Read contents of last-programmed 50-TP location
 * This is the location used in SW Reset command 4 or on POR
 */
#define TP_LAST_USED 0x06

#define CONTROL_WRITE 0x07// data bits 2:0 are the control bits

#define CONTROL_READ 0x08	// data bits all dont cares

#define SHUTDOWN 0x09	// data bit 0 set = shutdown, cleared = normal mode

/**
 * Control bits are three bits written with command 7
 */
// enable writing to the 50-TP memory by setting this control bit C0
// default is cleared so 50-TP writing is disabled
// only 50 total writes are possible!
#define TP_WRITE_ENABLE 0x01

// enable writing to volatile RADC wiper by setting this control bit C1
// otherwise it is frozen to the value in the 50-TP memory
// default is cleared, can't write to the wiper
#define RDAC_WIPER_WRITE_ENABLE 0x02

// enable high precision calibration by clearing this control bit C2
// set this bit to disable high accuracy mode (dunno why you would want to)
// default is 0 = emabled
#define RDAC_CALIB_DISABLE 0x04

// 50TP memory has been successfully programmed if this bit is set
#define TP_WRITE_SUCCESS 0x08

class AD527X
{   
    public:
        AD527X(uint8_t bytes, uint8_t resistance, uint8_t base); // constructor: base address

        void begin(boolean enableWiper); // checks that we have connected to the write IC
        boolean command_write(uint8_t command, uint16_t write_datum16);
        int16_t command_read(uint8_t command, uint8_t data);
        int8_t control_write_verified(uint8_t control);

        void enableWiperPosition();
        int getSetting();
        void shutdown();


        void setSettingValue(uint16_t inputValue);  
        uint16_t getMaximumSetting(); 
        void increment(); 
        void decrement(); 
        void setResistance(uint32_t targetResistance); 
        uint32_t getResistance(); 
        

        uint8_t BaseAddr;

    private:
    
    boolean getErrorCodes(uint8_t *i2c_error);
    
    uint32_t _resistance;
    uint32_t _MaxResistance;
    uint8_t _base;
    uint8_t _bits;
    uint8_t _bytes;
    uint16_t _value;
    uint16_t _minValue = 0;
    uint16_t _maxValue;
    uint16_t limit(uint16_t value);
};

#endif 