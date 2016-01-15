/*
 * wd.c
 *
 * Created: 15.01.2016 19:12:18
 *  Author: HSO
 */ 


#ifndef F_CPU
#define F_CPU 16000000UL
#endif


#include <avr/wdt.h>

void WDT_init(uint8_t val){
	wdt_enable(val);
}



void WDT_reset(void){
	wdt_reset(); 
}

void WDT_disable(void){
	wdt_disable();
	
}

