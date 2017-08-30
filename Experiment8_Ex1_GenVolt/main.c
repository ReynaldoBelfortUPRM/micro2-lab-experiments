/*
 * main.c
 * Main program for TIVA MCU
 * Exercise 8.2.1 Generating voltages Using a DAC
 *
 * Station 13
 * Lab Team:
 * Emmanuel Ramos
 * Reynaldo Belfort
 */

//Pin configuration
//7  6  5  4  3  2  1  0
//D7,D6,D5,D4,D3,D2,D1,D0 	- port B GPIOs - Data Port
//RS,R/W,E,PUSHB       		- port A GPIOs - Control Port + LCD Module Push Button
//-,-,-,-,D7,D6,D5,D4		- port D - Second Half of DAC data pins MSB
//-,-,-,-,D3,D2,D1,D0		- port F - First Half of DAC data pins LSB

//MCU objective #1: Change the current DAC value each 1 second, following the DAC table.
//				 The current DAC Value must be displayed on LCD
//MCU objective #2: Produce a sinusoidal wave with freq: 500Hz adn peak-to-peak voltage of 3.3V

//MCU objective #1 Pseudocode - Main program
//Initialize DAC Value table (array, hex values)
//<<Initialize LCD>>
//<<Initialize Timer, register ISR>>
//<<Initialize DAC pins>>
//<<Display LCD welcome message>>

//Main loop
	//<<Send dacTable[dac_counter] to DAC>>
	//<<Display current DAC Value>>
	//<<If dac_counter == 12>>
		//Reset dac_counter to 0

//<<Send dacTable[dac_counter]>>
	//Set dacTable[dac_counter][0] (4 LSB bits) on port F
	//Set dacTable[dac_counter][1] (4 MSB bits) on port D

//Pseudocode - Timer ISR
//Clear timer interrupt flag
//Increment dac_counter

#include <stdio.h> // for printf
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"

//Timer functionality
#include "inc/tm4c123gh6pm.h"
#include "driverlib/timer.h"

//Custom libraries
#include "MIL_LCD_lib.h"
#include "tivaUtils.h"

//0001 0111

//DAC Look-up Table
//uint16_t dacTable[12][2] = {
//		{0x00,0x00}, //0
//		{0x10,0x07}, //23
//		{0x20,0x0E}, //46
//		{0x40,0x05}, //69
//		{0x50,0x0C}, //92
//		{0x70,0x03}, //115
//		{0x80,0x0A}, //138
//		{0xA0,0x01}, //161
//		{0xB0,0x08}, //184
//		{0xC0,0x0F}, //207
//		{0xE0,0x06}, //230
//		{0xF0,0x0F}, //255
//};

//This has to be a mixture dec10 & hex values, in order to get the proper binary digits that we need in the GPIO output pins
//int dacTable[12][2] = {
//		{0,0}, //0
//		{1,7}, //23
//		{2,0x0E}, //46
//		{4,0x05}, //69
//		{5,0x0C}, //92
//		{7,0x03}, //115
//		{8,0x0A}, //138
//		{0xA0,0x01}, //161 //TODO Why doen't 0xA0 work??????
//		{0xB0,0x08}, //184
//		{0xC0,0x0F}, //207
//		{0xE0,0x06}, //230
//		{0xF0,0x0F}, //255
//};

int dacTable[12][2] = { //TODO WORKING VERSION
		{0,0}, //0
		{1,7}, //23
		{2,14}, //46
		{4,5}, //69
		{5,12}, //92
		{7,3}, //115
		{8,10}, //138
		{10,1}, //161 //TODO Why doen't 0xA0 work??????
		{11,8}, //184
		{12,15}, //207
		{14,6}, //230
		{15,15}, //255
};

//Constant definition for LCD positions

//Constant definitions
//Timer
#define DESIRED_FRECUENCY 1  //1Hz - Desired timer count frequency
//GPIO
//#define MSB_DAC_PINS GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_5|GPIO_PIN_4	//Remove if not needed
#define MSB_DAC_PINS GPIO_PIN_3|GPIO_PIN_2|GPIO_PIN_1|GPIO_PIN_0
#define LSB_DAC_PINS GPIO_PIN_3|GPIO_PIN_2|GPIO_PIN_1|GPIO_PIN_0

//Global variables
uint32_t timerPeriod;
uint32_t dac_counter;
int displayDecimalNumber;

int main(void) {

	//--------------------MCU Initialization------------------------
		//Set MCU to 40MHz
		SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

//		//--------Push button initialization----------
//		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);  //Enable PB F0 & F4
//		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);  //Enable ENTER PB A4
//
//		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
//		HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
//		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
//		GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_IN);
//		GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
//		//Port F interrupts
//		GPIOIntRegister(GPIO_PORTF_BASE, UP_DOWN_PUSHB_INTERRUPT);
//		GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_RISING_EDGE);
//		//Port A interrupts (LCD Module Push Button)
//		GPIOIntRegister(GPIO_PORTA_BASE, ENTER_PUSHB_INTERRUPT);
//		GPIOIntTypeSet(GPIO_PORTA_BASE, GPIO_PIN_4, GPIO_RISING_EDGE);
//		GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_4, GPIO_DIR_MODE_IN);
//		GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_4, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU); //Set up pull down push button
//		//Interrupt enable
//		GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0);
//		GPIOIntEnable(GPIO_PORTA_BASE, GPIO_PIN_4);
//		//--------------------

		//--------LCD Setup--------
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);  // Enable, RS and R/W port for LCD Display
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);  // Data port for LCD display
		//Set LCD pins as outputs
		GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_5);
		GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, ENTIRE_PORT);
		//-------------------------

		//--------Timer initialization--------

		SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
		//Configure Timer 0 as a 32-bit timer in periodic mode
		TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

		// Calculate number of clock cycles required at desired freq. Hz period by getting clock Hz and dividing it by our freq
		// then divide by two, since we want a count that is 1/2 of tthat for the interrupt
		timerPeriod = (SysCtlClockGet() / DESIRED_FRECUENCY); // Internal clock divided by desired frequency, in this case 1 ms
		// The calculated period is then loaded into the timer's Interval Load register(subtract 1, timer starts at 0)
		TimerLoadSet(TIMER0_BASE, TIMER_A, timerPeriod - 1);

		//Enable specific vector associated with interrupt for timer A
		IntEnable(INT_TIMER0A);
		//Enables specific event within the to generate an interrupt.  In this case generated on a timeout of Timer 0A
		TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
		//Enable master interrupt enable api for all interrupts
		IntMasterEnable();

		//--------Initialize DAC pins--------
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

		//Important steps to be able to use pins PF0, PD3, PD2
		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
		HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
		GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_OUT);
		GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD);

		GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, MSB_DAC_PINS); 	//MSBits DAC
		GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, LSB_DAC_PINS);	//LSBits DAC

    //--------------------------------------------------------

	//----Variable Initialization----
	dac_counter = 0;
	displayDecimalNumber = 0;

    //----LCD screen initialization----
    initializeLCD();
//TODO Uncomment
//    clearLCD();
//    writeMessage("--- DAC ---", 11);
//    setCursorPosition(0x40); //Jump to second line
//    writeMessage("-- Interfacer --", 16);
//    setDelay(2000); //2s delay
//    clearLCD();

    //Display initial DAC value
//    writeMessage("Current Val: ", 15);
//    writeChar(dacTable[dac_counter][0] + NUMERIC_DIGIT_OFFSET); 	//Get left-most digit
//    writeChar(dacTable[dac_counter][1] + NUMERIC_DIGIT_OFFSET);		//Get right-most digit
    //    	writeChar(dacTable[dac_counter][0]/10 + NUMERIC_DIGIT_OFFSET); 	//Get left-most digit
    //    	writeChar(dacTable[dac_counter] - (dacTable[dac_counter][0]/10*10) + NUMERIC_DIGIT_OFFSET);	//Get right-most digit

    //Start timer
    TimerEnable(TIMER0_BASE, TIMER_A);

	setCursorPosition(0x00);
	writeMessage("Current Val:", 12);

	//DEBUG CODE
//    puts("hello world!");
	uint8_t debug_counter = 1; //TODO Why 7 doesn't work????

    //Main Loop
    while(true) {
    	//Set current table value on DAC device
    	//TODO  USE THIS CODE FOR FINAL CODE VERSION
    	GPIOPinWrite(GPIO_PORTD_BASE, MSB_DAC_PINS, dacTable[dac_counter][0]);
    	GPIOPinWrite(GPIO_PORTF_BASE, LSB_DAC_PINS, dacTable[dac_counter][1]);

    	//DEBUG CODE
//    	GPIOPinWrite(GPIO_PORTD_BASE, MSB_DAC_PINS, dacTable[debug_counter][0]);
//    	GPIOPinWrite(GPIO_PORTF_BASE, LSB_DAC_PINS, dacTable[debug_counter][1]);

    	//Display current DAC val
    	//TODO This should output HEX vals (try if you can)
    	//TODO Make this a function

    	setDelay(1);
//    	setDelay(5);
//    	setCursorPosition(0x0C);


//   	//DISPLAY VALUE ON LCD
//    	if(dac_counter < 12){
//    		displayDecimalNumber = dacTable[dac_counter][0] + dacTable[dac_counter][1];
//
//    		uint8_t digitOutput[3];
//			//Calculate digits to be displayed
////    		digitOutput[0] = displayDecimalNumber/1000;
//    		digitOutput[0] = displayDecimalNumber/100;
//    		digitOutput[1] = displayDecimalNumber/10 - (digitOutput[1] * 10);
//    		digitOutput[2] = displayDecimalNumber - (digitOutput[1] * 100) - (digitOutput[2]* 10);
//
////    		strRPMS[0] = rpms/1000;
////			strRPMS[1] = rpms/100 - (strRPMS[0] * 10);
////			strRPMS[2] = rpms/10 - (strRPMS[0] *100) - (strRPMS[1] * 10);
////			strRPMS[3] = rpms - (strRPMS[0] *1000) - (strRPMS[1] * 100) - (strRPMS[2]* 10);
//
//    		//Display decimal number
//			writeChar(digitOutput[0] + 48);
//			writeChar(digitOutput[1] + 48);
//			writeChar(digitOutput[2] + 48);
//
////    		    	uint8_t digitResult = 8;
////    		//    	uint8_t dividend = 5;
////    		    	uint8_t dividend = 100;
////    		    	if(displayDecimalNumber < 100) {
////    		    		dividend = 10;
////    		    		writeChar((displayDecimalNumber/dividend) + NUMERIC_DIGIT_OFFSET);	//Decenas 150
////    		    	    writeChar( ((displayDecimalNumber - ((displayDecimalNumber/dividend)*dividend))) +  NUMERIC_DIGIT_OFFSET);	//Unidades
////    		    	}
////    		    	else{
////    		    		//    		    	digitResult = displayDecimalNumber/dividend;
////    		    		//    		    	writeChar(digitResult + NUMERIC_DIGIT_OFFSET);
////
////
////    		    		writeChar((displayDecimalNumber/dividend) + NUMERIC_DIGIT_OFFSET); 		//Centenas
//////						writeChar((displayDecimalNumber - ((displayDecimalNumber/dividend)*dividend)) + NUMERIC_DIGIT_OFFSET);	//Decenas 150
////
//////    		    		digitResult = (displayDecimalNumber - ((displayDecimalNumber/dividend)*dividend));
//////    		    		writeChar(digitResult + NUMERIC_DIGIT_OFFSET);
////
////						writeChar((displayDecimalNumber - ((displayDecimalNumber/dividend)*dividend) - ((displayDecimalNumber/dividend)*dividend)) +  NUMERIC_DIGIT_OFFSET);	//Unidades
////    		    	}
//
//
////    		    	digitResult = displayDecimalNumber/dividend;
////    		    	writeChar(digitResult + NUMERIC_DIGIT_OFFSET);
//    		//    	writeChar(9 + NUMERIC_DIGIT_OFFSET);
//    	}

    } //End of while
}

//Program functions
void TIMER_INTERRUPT_ISR(void){
	//Clear timer interrupt flag
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	//printf("LOG - displayDecimalNumber: %d \n", displayDecimalNumber);

	//Increment coutner
	if(dac_counter + 1 >= 12){ //Reset counter if needed
		dac_counter = 0;
	}
	else{
		dac_counter++;
	}
}

//void DisplayDacValue(){
//
//}