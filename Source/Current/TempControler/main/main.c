/*
 * main.c
 *
 * Created: 01.01.2016 01:44:08
 *  Author: HSO
 */ 


#define F_CPU 16000000UL


#include <avr/io.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "libs/watchdog/wd.h"

#include "libs/utils/bit_tools.h"
#include "libs/utils/my_utils.h"
#include "libs/lcds/hd774x.h"
#include "libs/interrupts/interruptvectors.h"
#include "libs/timer/timer_utils.h"
#include "libs/ds182x/ds18b20.h"
#include "libs/usart/usart.h"

/************************************************************************/
/* Local Prototipes                                                                     */
/************************************************************************/
void setTempSetPoint(uint8_t up);
void setHistSetPoint(uint8_t up);
uint8_t checkTempError(double currentTemp, double setPoint, double hist);

void stateMachine(uint8_t inCode);

uint8_t decodeButton(uint8_t button);
uint8_t debounceKey(uint8_t codeNew);

void updateLcd(void);
void setLcdInitialFields(void);
void showLcdSavedMessage(void);
void showLcdSplash(void);

void initGPIO(void);
uint8_t readButtons(void);
void setOutputRelay(uint8_t val);

void paramLoadDefaultParameters(void);
void eepromSetDefaultParameters(void);
void paramLoadFromEeprom(void);
void paramSavetoEeprom(void);

//#define  USE_WDT

// Debug stuff
//#define MAIN_DEBUG
char debugBuffer[10];


// LEDS state
#define LED_RUN_ON bit_clear(PORTB,2)
#define LED_RUN_OFF bit_set(PORTB,2)

#define LED_PROG_ON bit_clear(PORTB,3)
#define LED_PROG_OFF bit_set(PORTB,3)

#define LED_OUT_ON bit_clear(PORTB,4)
#define LED_OUT_OFF bit_set(PORTB,4)


// eeproms strucst
typedef struct{
	uint8_t initEeprom;
	
	double setPointTemp;
	double setPointHist;
	
}eestruct_t;


eestruct_t EEMEM eestruct_eemem;
eestruct_t eestruct_var;


/////////////////////////////////////////////////////////////
////////////////// SCHED ZONE ////////////////////////////////
/////////////////////////////////////////////////////////////

#define SCHEDULER_PRESCALER TIMER0_PRESC128
#define SCHEDULER_RELOAD 31*4 // 1ms tick

// base at 1ms
// task periods
volatile uint16_t taskReadButtonsPeriod =100;
volatile uint16_t taskUpdateLCDPeriod = 500;
volatile uint16_t taskControlPeriod =1000;

//task flags
volatile uint8_t flagTaskReadButtons =0;
volatile uint8_t flagTaskUpdateLcd =0;
volatile uint8_t flagTaskControl =0;

uint8_t flagSaveParametersEeprom=0;

// @ setuo scheulder
void schedulerInit(void){
	TCCR0 |= SCHEDULER_PRESCALER;
	TCCR0 |= TIMER0_WAVEFORM_MODE_CTC;
	OCR0  = SCHEDULER_RELOAD; // timer count reaload
	TIMSK |= (1<< OCIE0); // Enable timer compare interrupt
	
	
	
}




/////////////////////////////////////////////////////////////
////////////////// STATE ZONE ////////////////////////////////
/////////////////////////////////////////////////////////////


// STATE MACHINE
#define STATE_IDLE 0
#define STATE_PROGRAM_SETPOINT 1
#define STATE_PROGRAM_HIST 2
#define STATE_SAVE 3


#define BUTTON_ESC 1
#define BUTTON_ENTER 2
#define BUTTON_UP 3
#define BUTTON_DOWN 4



/************************************************************************/
/* @decode Button                                                                     */
/************************************************************************/
uint8_t decodeButton(uint8_t button){
	button &=0x0F; // ensure
	switch (button){
		
		case 1: return BUTTON_ENTER;
		break;
		
		case 2: return BUTTON_UP;
		break;
		
		case 4: return BUTTON_DOWN;
		break;
		
		case 8: return BUTTON_ESC;
		
		
		default: return 0;
		break;
	}
}



#define N_DEBOUNCE 3
/************************************************************************/
/* @debounce function                                                                     */
/************************************************************************/
uint8_t debounceKey(uint8_t codeNew){
	uint8_t key =0; // by default
	static uint8_t codeOld;
	static uint8_t keyCount;
	
	// ALREADY SOMETHIN PRESSED
	if(keyCount != 0){
		
		// IF SAME KEY and inside debounce times save
		if(codeNew == codeOld && keyCount <N_DEBOUNCE){ // ONLY IF EQUAL AND DEBOUNCE AVAILABLE
			codeOld =codeNew;
			keyCount++;
			// Reached debounce value and valid key
			if (keyCount == N_DEBOUNCE){
				key = codeNew; // ONLY HERE key is changed;
				
			}
		}
		
	}

	
	// INITIAL CONDITION
	if (keyCount == 0){
		codeOld = codeNew;
		keyCount++;
	}
	
	// if pressed key different reset (user must release the key for new run)
	if(codeNew != codeOld){
		codeOld =codeNew;
		keyCount =1;
	}
	return key;
}



/************************************************************************/
/* @sate machine                                                                     */
/************************************************************************/
void stateMachine(uint8_t inCode){
	
	static uint8_t state;

	//inCode &= 0x07; // ensure clean
	
	
	
	// Switch to the state
	switch (state){
		
		case STATE_IDLE:
			#ifdef MAIN_DEBUG
			USART1_sendStr("IDLE\n\r");
			#endif
			LED_PROG_OFF;
			if(inCode == BUTTON_ENTER) state = STATE_PROGRAM_SETPOINT;
		break;
		
		case STATE_PROGRAM_SETPOINT:
			#ifdef MAIN_DEBUG
			USART1_sendStr("SETPOINT\n\r");
			#endif
			LED_PROG_ON;
			if(inCode==BUTTON_UP)setTempSetPoint(1);
			if(inCode==BUTTON_DOWN)setTempSetPoint(0);
			if(inCode==BUTTON_ENTER)state=STATE_PROGRAM_HIST;
			if(inCode== BUTTON_ESC)state=STATE_SAVE;
		break;
		
		case STATE_PROGRAM_HIST:
			#ifdef MAIN_DEBUG
			USART1_sendStr("HIST\n\r");
			#endif
			if(inCode==BUTTON_UP)setHistSetPoint(1);
			if(inCode==BUTTON_DOWN)setHistSetPoint(0);
			if(inCode==BUTTON_ENTER)state=STATE_SAVE;
			if(inCode== BUTTON_ESC)state=STATE_SAVE;
		
		break;
		case STATE_SAVE:
			#ifdef MAIN_DEBUG
			USART1_sendStr("SAVE\n\r");
			#endif
			flagSaveParametersEeprom++;
			//wait to be saved and exit
			state=STATE_IDLE;
		break;
		
		default:
			state=STATE_IDLE;
		break;
	}
}



/////////////////////////////////////////////////////////////
////////////////// CTRL ZONE ////////////////////////////////
/////////////////////////////////////////////////////////////
#define SETPOINT_TEMP_DEFAULT 20.0
#define SETPOINT_HISTERESYS_DEFAULT 3.0



// run variables
double currentTemp=30.0;
double currentTempSetPoint=20.0;
double currentHistSetPoint=5.0;
uint8_t currentStatus=0;

#define TEMP_UP_LIMIT 200.0
#define TEMP_LOW_LIMIT -200.0
#define HIST_STEP 1.0
#define SETPOINT_STEP 1.0


#define RELAY_STATE_OFF 0
#define RELAY_STATE_ON 1
uint8_t checkTempError(double currentTemp, double setPoint, double hist){
	static uint8_t relayLastState;
	
	if (currentTemp > (setPoint+hist/2.0)){
		
		relayLastState=RELAY_STATE_ON;
		return RELAY_STATE_ON;
	}
	
	// low temp set to off
	if(currentTemp < (setPoint - hist/2.0)){
		relayLastState=RELAY_STATE_OFF;
		return RELAY_STATE_OFF;
	}
	
	// by defaut return last state //
	return relayLastState;
	
	
}

void setTempSetPoint(uint8_t up){
	if(up)currentTempSetPoint+=SETPOINT_STEP;
	else currentTempSetPoint-=SETPOINT_STEP;
	if(currentTempSetPoint >TEMP_UP_LIMIT) currentTempSetPoint=TEMP_UP_LIMIT;
	if(currentTempSetPoint<TEMP_LOW_LIMIT)currentTempSetPoint=TEMP_LOW_LIMIT;
}


void setHistSetPoint(uint8_t up){
	if(up)currentHistSetPoint+=SETPOINT_STEP;
	else currentHistSetPoint-=SETPOINT_STEP;
	if(currentHistSetPoint<0.5) currentHistSetPoint=SETPOINT_STEP; // ensure always positive
}


// read temteprature
double readTemperatures(void){
	return ds18b20_getTempAndWait();
	//return ds18b20_gettemp();
	
}


/////////////////////////////////////////////////////////////
////////////////// GIO ZONE /////////////////////////////////
/////////////////////////////////////////////////////////////
#define  RELAY_PORT PORTB
#define RELAY_BIT 1
/************************************************************************/
/* @read the buttons and complement and clean                                                                     */
/************************************************************************/
uint8_t readButtons(void){

	return (~PINA) &0x0F; // read the low nibble
}


#define RELAY_OUT_ON bit_set(RELAY_PORT,RELAY_BIT)
#define RELAY_OUT_OFF bit_clear(RELAY_PORT,RELAY_BIT)
/************************************************************************/
/* @set the output relay                                                                     */
/************************************************************************/
void setOutputRelay(uint8_t val){
	if(val >0){
		RELAY_OUT_ON;
		LED_OUT_ON;
		
	}else{
		RELAY_OUT_OFF;
		LED_OUT_OFF;
	}
	
}


/************************************************************************/
/* @init the gpio                                                                     */
/************************************************************************/
void initGPIO(void){
	DDRA = 0x00;
	PORTA = 0x0F; // enable pulls ups
	
	DDRB = 0b00011110;
	PORTB = 0x01;

}



/////////////////////////////////////////////////////////////
////////////////// EEPROM ZONE //////////////////////////////
/////////////////////////////////////////////////////////////

/************************************************************************/
/* @load hardcodded default values                                                                     */
/************************************************************************/
void paramLoadDefaultParameters(void){
	currentTempSetPoint = SETPOINT_TEMP_DEFAULT;
	currentHistSetPoint = SETPOINT_HISTERESYS_DEFAULT;
}


/************************************************************************/
/* @ set initial values to eeprom  if nothin there yet                                                                   */
/************************************************************************/
void eepromSetDefaultParameters(void){
	eestruct_var.initEeprom=1; // emprom init
	
	eestruct_var.setPointTemp = SETPOINT_TEMP_DEFAULT;
	eestruct_var.setPointHist = SETPOINT_HISTERESYS_DEFAULT;
	
	eeprom_write_block((const void*)&eestruct_var,(void*)&eestruct_eemem,sizeof(eestruct_t));
	
}


/************************************************************************/
/* @ load eeprom saved values                                                                     */
/************************************************************************/
void paramLoadFromEeprom(void){
	//uint8_t temp=0;
	// read from emprom
	eeprom_read_block((void*)&eestruct_var, (const void*)&eestruct_eemem,sizeof(eestruct_t));
	
	// test the fits field to check if it was written else use default and load
	if((eestruct_var.initEeprom &0xFF) ==0xFF){
		eepromSetDefaultParameters();
		paramLoadDefaultParameters();
		
	}
	else{
		// write to the global variables
		currentTempSetPoint = eestruct_var.setPointTemp;
		currentHistSetPoint = eestruct_var.setPointHist;
		
	}
	
	
	
}


/************************************************************************/
/* @save current values to eerprom                                                                     */
/************************************************************************/
void paramSavetoEeprom(void){
	// save paramenetrs on the run
	eestruct_var.initEeprom=1; // emprom init
	eestruct_var.setPointTemp = currentTempSetPoint;
	eestruct_var.setPointHist = currentHistSetPoint;
	
	// save block
	eeprom_write_block((const void*)&eestruct_var,(void*)&eestruct_eemem,sizeof(eestruct_t));
	
}


/////////////////////////////////////////////////////////////
////////////////// LCD ZONE /////////////////////////////////
/////////////////////////////////////////////////////////////

/************************************************************************/
/* @show initial splash                                                                     */
/************************************************************************/
void showLcdSplash(void){
	
	LCD_gotoXY(2,0);
	LCD_sendString("Wine Control");
	LCD_gotoXY(1,1);
	LCD_sendString("Serafim Cunha");
	_delay_ms(2000);
	LCD_clr();
	
	
	
}

// Current: 20.0º State: OFF
// SetPoint: 30.0 Hist: 10.5
//

/************************************************************************/
/* @set lcd initail fields                                                                     */
/************************************************************************/
void setLcdInitialFields(void){

	LCD_clr();
	LCD_gotoXY(0,0);
	LCD_sendString("T:");
	LCD_gotoXY(9,0);
	LCD_sendString("ST:");
	
	// sendond filds	
	LCD_gotoXY(0,1);
	LCD_sendString("S:");
	LCD_gotoXY(9,1);
	LCD_sendString("D:");	

}

/************************************************************************/
/* @show saved messsage on lcd                                                                      */
/************************************************************************/
void showLcdSavedMessage(void){
	
	LCD_clr();
	LCD_gotoXY(7,0);
	LCD_sendString(" SAVED");

	_delay_ms(500);
	
}

/************************************************************************/
/* @ convert double two parts to print                                                                     */
/************************************************************************/
void splitDouble(double *d, int precision, int *intPart, int *decPart ){
	*intPart = (int)*d;
	*decPart = (int)( (*d - *intPart) *pow(10, precision) + 0.5);
	
}

/************************************************************************/
/* @update lcd with current vars                                                                     */
/************************************************************************/ 
void updateLcd(void){
	char buffer[4];
	int integerPart;
	int decimalPart;
	// RELAY
	LCD_gotoXY(13,0);
	if(currentStatus)LCD_sendString("ON ");
	else LCD_sendString("OFF");
	
	
	// current temp
	LCD_gotoXY(2,0);
	splitDouble(&currentTemp,1,&integerPart,&decimalPart);
	sprintf(buffer,"%3d.%1d",integerPart, decimalPart);
	LCD_sendString(buffer);
	
	// current setpoint
	LCD_gotoXY(2,1);
	splitDouble(&currentTempSetPoint,1,&integerPart,&decimalPart);
	sprintf(buffer,"%3d.%1d",integerPart, decimalPart);
	LCD_sendString(buffer);
	
	// current hist
	LCD_gotoXY(11,1);
	splitDouble(&currentHistSetPoint,1,&integerPart,&decimalPart);
	sprintf(buffer,"%3d.%1d",integerPart, decimalPart);
	LCD_sendString(buffer);
	

}



#define DS18B20_STATUS_READY 0
#define DS18B20_STATUS_CONV 1
#define DS18B20_STATUS_END_CONV 2

// main loop
int main(void){
	
	uint8_t stateDS18=DS18B20_STATUS_READY;
	
	#ifdef USE_WDT
	WDT_init(WDTO_8S); 
	#endif
	
	
	initGPIO();
	LCD_init();
	
	showLcdSplash();
		
	setLcdInitialFields();
		
	paramLoadFromEeprom();
		
	USART1_config(USART1_MY_UBBRN,USART_DATA_FORMAT_8BITS|USART_STOP_BITS_1,USART_TRANSMIT_ENABLE|USART_RECEIVE_ENABLE| USART_INTERRUPT_ENABLE);
	
	USART1_sendStr("Hello");
	
	schedulerInit();
	
	// check for the default values
	#ifdef USE_WDT
	WDT_init(WDTO_8S);
	#endif
	
	
	sei(); //enable interrups
	
	// loop while	
    while(1){
		
		
		// cintrol zone
		if(flagTaskControl){
			#ifdef USE_WDT
			WDT_reset();
			#endif
			
			
			LED_RUN_OFF;
			// fire the DS
			if(stateDS18==DS18B20_STATUS_READY){
				DS18B20_startConv();
				stateDS18=DS18B20_STATUS_CONV;
			
			// check if convertion ended	
			}else if(stateDS18==DS18B20_STATUS_CONV){
				if(DS18B20_convComplete()){
					stateDS18=DS18B20_STATUS_END_CONV;
				}
			
			// convertion ready	
			}else if (stateDS18==DS18B20_STATUS_END_CONV){
				LED_RUN_ON;
				currentTemp=DS18B20_getTemp();
				stateDS18=DS18B20_STATUS_READY;
				
				currentStatus = checkTempError(currentTemp,currentTempSetPoint,currentHistSetPoint); // chec the out
				setOutputRelay(currentStatus);
				_delay_ms(50);
				LED_RUN_OFF;
					
				
			}
			
			flagTaskControl=0;
		}
		
		// user bottons area
		if(flagTaskReadButtons){
			#ifdef USE_WDT
			WDT_reset();
			#endif
			
			uint8_t portVal = readButtons();
			uint8_t code = decodeButton(portVal);
			code = debounceKey(code);
			
			#ifdef MAIN_DEBUG
			sprintf(debugBuffer,"Key %d",code);
			USART1_sendStr(debugBuffer);
			#endif
			
			stateMachine(code); // go to machine
			
			flagTaskReadButtons=0;
		}
		
		 
		 // lcd update area
		 if(flagTaskUpdateLcd){
			 #ifdef USE_WDT
			 WDT_reset();
			 #endif
			 //showLcdSavedMessage();
			 updateLcd(); // update the lcd
			 
			 flagTaskUpdateLcd=0;
		 }
		 
		 // save to eeprom
		 if(flagSaveParametersEeprom){
			 #ifdef USE_WDT
			 WDT_reset();
			 #endif
			 
			 paramSavetoEeprom(); // save value sto eeprom
			 showLcdSavedMessage();
			 setLcdInitialFields();
			 updateLcd();
			 flagSaveParametersEeprom=0;
		 }
		 
    }
}