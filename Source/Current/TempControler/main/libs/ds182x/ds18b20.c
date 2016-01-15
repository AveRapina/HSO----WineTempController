
#ifndef F_CPU
	#define F_CPU 16000000UL
#endif




#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "ds18b20.h"
#include "../usart/usart.h"


//#define DEBUG_DS18

/*
 * ds18b20 init
 */
uint8_t DS_reset() {
	uint8_t i;

	//low for 480us
	DS18B20_PORT &= ~ (1<<DS18B20_DQ); //low
	DS18B20_DDR |= (1<<DS18B20_DQ); //output
	_delay_us(480);

	//release line and wait for 60uS
	DS18B20_DDR &= ~(1<<DS18B20_DQ); //input
	_delay_us(60);

	//get value and wait 420us
	i = (DS18B20_PIN & (1<<DS18B20_DQ));
	_delay_us(420);

	//return the read value, 0=ok, 1=error
	return i;
}

/*
 * write one bit
 */
void DS_writeBit(uint8_t bit){
	//low for 1uS
	DS18B20_PORT &= ~ (1<<DS18B20_DQ); //low
	DS18B20_DDR |= (1<<DS18B20_DQ); //output
	_delay_us(1);

	//if we want to write 1, release the line (if not will keep low)
	if(bit)
		DS18B20_DDR &= ~(1<<DS18B20_DQ); //input

	//wait 60uS and release the line
	_delay_us(60);
	DS18B20_DDR &= ~(1<<DS18B20_DQ); //input
}

/*
 * read one bit
 */
uint8_t DS_readBit(void){
	uint8_t bit=0;

	//low for 1uS
	DS18B20_PORT &= ~ (1<<DS18B20_DQ); //low
	DS18B20_DDR |= (1<<DS18B20_DQ); //output
	_delay_us(1);

	//release line and wait for 14uS
	DS18B20_DDR &= ~(1<<DS18B20_DQ); //input
	_delay_us(14);

	//read the value
	if(DS18B20_PIN & (1<<DS18B20_DQ))
		bit=1;

	//wait 45uS and return read value
	_delay_us(45);
	return bit;
}

/*
 * write one byte
 */
void DS_writeByte(uint8_t byte){
	uint8_t i=8;
	while(i--){
		DS_writeBit(byte&1);
		byte >>= 1;
	}
}

/*
 * read one byte
 */
uint8_t DS_readByte(void){
	uint8_t i=8, n=0;
	while(i--){
		n >>= 1;
		n |= (DS_readBit()<<7);
	}
	return n;
}



void DS18B20_startConv(void){
	DS_reset(); //reset
	DS_writeByte(DS18B20_CMD_SKIPROM); //skip ROM
	DS_writeByte(DS18B20_CMD_CONVERTTEMP); //start temperature conversion
}

/************************************************************************/
/* @check if ds as finished                                                                     */
/************************************************************************/
uint8_t DS18B20_convComplete(void){
	return DS_readBit(); //wait until conversion is complete
	
}

double DS18B20_getTemp(void){
	uint8_t temperature_l;
	uint8_t temperature_h;
	double retd = 0;
	
	DS_reset(); //reset
	DS_writeByte(DS18B20_CMD_SKIPROM); //skip ROM
	DS_writeByte(DS18B20_CMD_RSCRATCHPAD); //read scratchpad

	//read 2 byte from scratchpad
	temperature_l = DS_readByte();
	temperature_h = DS_readByte();
	
	
	
	#ifdef DEBUG_DS18
	char buffer[10];
	sprintf(buffer,"%x:%x\n\r",temperature_h,temperature_l);
	USART1_sendStr(buffer);
	#endif
	
	//convert the 12 bit value obtained
	retd = ( ( temperature_h << 8 ) + temperature_l ) * 0.0625 ;

	return retd;
}

/*
 * get temperature
 */
double ds18b20_getTempAndWait() {
	uint8_t temperature_l;
	uint8_t temperature_h;
	double retd = 0;

	#if DS18B20_STOPINTERRUPTONREAD == 1
	cli();
	#endif

	DS_reset(); //reset
	DS_writeByte(DS18B20_CMD_SKIPROM); //skip ROM
	DS_writeByte(DS18B20_CMD_CONVERTTEMP); //start temperature conversion

	while(!DS_readBit()); //wait until conversion is complete

	DS_reset(); //reset
	DS_writeByte(DS18B20_CMD_SKIPROM); //skip ROM
	DS_writeByte(DS18B20_CMD_RSCRATCHPAD); //read scratchpad

	//read 2 byte from scratchpad
	temperature_l = DS_readByte();
	temperature_h = DS_readByte();

	#if DS18B20_STOPINTERRUPTONREAD == 1
	sei();
	#endif
	
	#ifdef DEBUG_DS18
	char buffer[10];
	sprintf(buffer,"%x:%x\n\r",temperature_h,temperature_l);
	USART1_sendStr(buffer);
	#endif
	//convert the 12 bit value obtained
	retd = ( ( temperature_h << 8 ) + temperature_l ) * 0.0625 ;

	return retd;
}

