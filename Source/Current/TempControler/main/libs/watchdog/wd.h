/*
 * wdtdefs.h
 *
 * Created: 15.01.2016 19:06:15
 *  Author: HSO
 */ 


#ifndef WDTDEFS_H_
#define WDTDEFS_H_

#include <avr/wdt.h>

#define 	WDTO_15MS   0
#define 	WDTO_30MS   1
#define 	WDTO_60MS   2
#define 	WDTO_120MS   3
#define 	WDTO_250MS   4
#define 	WDTO_500MS   5
#define 	WDTO_1S   6
#define 	WDTO_2S   7
#define 	WDTO_4S   8
#define 	WDTO_8S   9




extern void WDT_init(uint8_t val);
extern void WDT_reset(void);
extern void WDT_disable(void);



#endif /* WDTDEFS_H_ */