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
//-,-,-,-,-,PUSHB,Pot. (ADC),- port E - LCD Module Pull-up Push Button 1 + potentiometer ADC

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
#include <string.h> // for sprintf

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
//UART
#include "driverlib/uart.h"
//ADC
#include "driverlib/adc.h"
//PWM
#include "driverlib/pwm.h"
#define DESIRED_PWM_FRECUENCY 1000 //In Hz
#define DUTY_CYCLE 0.50 //% DutyCycle (decimal value)
volatile uint32_t pwmClockFreq = 0;
volatile uint32_t pwmLoadValue = 0;

//Custom libraries
#include "MIL_LCD_lib.h"
#include "tivaUtils.h"

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
//ADC related
uint32_t ui32ADC0Value[1];
volatile uint32_t ui32TempAvg;
volatile uint32_t ui32TempValueC;
volatile uint32_t ui32TempValueF;
volatile uint32_t inputValueADC;
volatile float resultPercent;
volatile float resultADC;


uint32_t timerPeriod;
uint32_t dac_counter;
int displayDecimalNumber;
char lcdOutput[100];
bool temperatureToggle;

//Fuction declatations
//void ENTER_PUSHB_INTERRUPT();
//void convertDecToHex(int decimalValue, char * resultArr);
void printMessageUART(char message[]);
void ENTER_PUSHB_INTERRUPT();


//TODO
//PINES UART DEL TIVA LAUNCHPAD PA0, PA1

int main(void) {

	//--------------------MCU Initialization------------------------
		//Set MCU to 40MHz
		SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

		//--------LCD Setup--------
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);  // Enable, RS and R/W port for LCD Display
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);  // Data port for LCD display
		//Set LCD pins as outputs
		GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_5);
		GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, ENTIRE_PORT);
		//-------------------------

		//Port E interrupts (LCD Module Push Button)
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
		GPIOIntRegister(GPIO_PORTE_BASE, ENTER_PUSHB_INTERRUPT);
		GPIOIntTypeSet(GPIO_PORTE_BASE, GPIO_PIN_2, GPIO_FALLING_EDGE);
		GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_2, GPIO_DIR_MODE_IN);
		GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_2, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPD); //Set up pull down push button
		//Interrupt enable
		GPIOIntEnable(GPIO_PORTE_BASE, GPIO_PIN_2);


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

//		GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, MSB_DAC_PINS); 	//MSBits DAC
		GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, LSB_DAC_PINS);	//LSBits DAC

		//----UART Initialization (USB PORT)----

		SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
		//Configure pins to be used as UART Rx & TX: Pins 1 & 0 from port c
		GPIOPinConfigure(GPIO_PA0_U0RX);
		GPIOPinConfigure(GPIO_PA1_U0TX);
		GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
		UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE ));

		//------- ADC Initialization -------
		 SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
		SysCtlPeripheralReset(SYSCTL_PERIPH_ADC0);
		ADCSequenceDisable(ADC0_BASE, 1);
		ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
		ADCSequenceStepConfigure(ADC0_BASE,1,0,ADC_CTL_CH4|ADC_CTL_IE|ADC_CTL_END);
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
		GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_3);
		ADCSequenceEnable(ADC0_BASE, 1);

		//------------PWM Initialization (For buzzer)------------
		 //Clock the PWM module by the system clock
		 SysCtlPWMClockSet(SYSCTL_PWMDIV_32); //Divide system clock by 32 to run the PWM at 1.25MHz

		 //Enabling PWM1 module and Port C
		 SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
		 SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC); 	//Port is already initialized above

		 //Selecting PWM generator 0 and port C pin 0 (PC4) aa a PWM output pin for module one
		 GPIOPinTypePWM(GPIO_PORTC_BASE, GPIO_PIN_4); 	//Set Port D pin 0 as output //TODO Checkout which ports can be used for PWM functionallity
		 GPIOPinConfigure(GPIO_PC4_M0PWM6); 			//Select PWM Generator 0

		 //Determine period register load value
		 pwmClockFreq = SysCtlClockGet() / 32; //TODO as Isnt the same as ROM_SysCtlPWMClockSet(SYSCTL_PWMDIV_64);? Is something being repeated?
		 pwmLoadValue = (pwmClockFreq / DESIRED_PWM_FRECUENCY) - 1; 	//Load Value = (PWMGeneratorClock * DesiredPWMPeriod) - 1
		 PWMGenConfigure(PWM0_BASE, PWM_GEN_3, PWM_GEN_MODE_DOWN); 		//Set a count-down generator type
		 PWMGenPeriodSet(PWM0_BASE, PWM_GEN_3, pwmLoadValue); 			//Set PWM period determined by the load value

		 //Setup PWM Pulse Width Duty Cycle
		 PWMPulseWidthSet(PWM0_BASE, PWM_OUT_6, pwmLoadValue * DUTY_CYCLE);
		 PWMOutputState(PWM0_BASE, PWM_OUT_6_BIT, false); 		//Start with the PWM Gen off
		 PWMGenEnable(PWM0_BASE, PWM_GEN_3); 					//Enable PWM Generator
		//-------------------------------------------

    //--------------------------------------------------------

	//----Variable Initialization----
	dac_counter = 0;
	displayDecimalNumber = 0;
	temperatureToggle = false;

    //----LCD screen initialization----
//    initializeLCD();
//    clearLCD();
//    writeMessage("--- DAC ---", 11);
//    setCursorPosition(0x40); //Jump to second line
//    writeMessage("-- Interfacer --", 16);
//    setDelay(2000); //2s delay
//    clearLCD();

    //Start timer
    TimerEnable(TIMER0_BASE, TIMER_A);


    //Main Loop
    while(true) {
    	//Set current table value on DAC device
//    	//TODO  USE THIS CODE FOR FINAL CODE VERSION
//    	GPIOPinWrite(GPIO_PORTD_BASE, MSB_DAC_PINS, dacTable[dac_counter][0]);
//    	GPIOPinWrite(GPIO_PORTF_BASE, LSB_DAC_PINS, dacTable[dac_counter][1])
//
    	setDelay(1000);
    	ADCIntClear(ADC0_BASE, 1);
    	ADCProcessorTrigger(ADC0_BASE, 1);

    	//Wait for new ADC data
    	while(!ADCIntStatus(ADC0_BASE, 1, false)) { }

    	//Retrieve new data
    	ADCSequenceDataGet(ADC0_BASE, 1, ui32ADC0Value); //The value retrieved here represents the voltage magnitude that was read on that pin.

    	inputValueADC = (ui32ADC0Value[0]);
    	resultPercent = (float)inputValueADC/4096.0;
    	resultADC = (float)inputValueADC/4096.0*5;


    	//TODO exercise 8.2.
//    	sprintf(lcdOutput, "ADC input val (dec): %.2f V ; hex: %x V perc.: %.2f%%\n\r", resultADC, *(unsigned int*)&resultADC, resultPercent*100); //Potentiometer Voltage
//    	printMessageUART(lcdOutput);

    	//TODO exercise 8.2.3 Dimmer
//    	sprintf(lcdOutput, "Current LED Brighness: %.2f%%\n\r", resultPercent*100); //Potentiometer Voltage
//    	printMessageUART(lcdOutput);
//
//    	float evalResult = resultPercent*100;
//    	if(evalResult > 99.0){
//    		sprintf(lcdOutput, "MAXUMUM level reached!\n\r");
//    		printMessageUART(lcdOutput);
//    	}
//    	else if(evalResult < 1.0){
//    		sprintf(lcdOutput, "LOWEST level reached!\n\r");
//    		printMessageUART(lcdOutput);
//    	}
//    	//Change LED Brighness
//		PWMPulseWidthSet(PWM0_BASE, PWM_OUT_6, pwmLoadValue * (1 - resultPercent));  // Set PWM to new selected duty cycle
//		PWMOutputState(PWM0_BASE, PWM_OUT_6_BIT, true);  // turn PWM on
//		PWMGenEnable(PWM0_BASE, PWM_GEN_3); //Enable PWM Generator

    	//Complementary task
    	float temperatureValue = (resultADC/0.01); //Result is Celcius unit
    	if(temperatureToggle){ //Show farenheit instead of celcius
    		temperatureValue = ( temperatureValue * 9)/5 + 32;
    		sprintf(lcdOutput, "Room temperature: %.2f F\n\r", temperatureValue); //Temperature sensor value
    	}
    	else{
    		sprintf(lcdOutput, "Room temperature: %.2f C\n\r", temperatureValue); //Temperature sensor value
    	}
    	printMessageUART(lcdOutput);

//		sprintf(lcdOutput, "ADC input val (hex): %xV \n\r", *(unsigned int*)&resultADC); //Potentiometer Voltage

    	//F = ( C * 9)/5 +32 or F = ((C * 9) + 160) / 5
    	//

    	//TODO Temp code: other ADC Values
//    	ui32TempAvg = (ui32ADC0Value[0] + ui32ADC0Value[1] + ui32ADC0Value[2] + ui32ADC0Value[3] + 2)/4;
//    	ui32TempValueC = (1475 - ((2475 * ui32TempAvg)) / 4096)/10;
//    	ui32TempValueF = ((ui32TempValueC * 9) + 160) / 5;

    } //End of while
}

void printMessageUART(char message[]){
	int i = 0;
	for(; i < strlen(message); i++){
		UARTCharPut(UART0_BASE, message[i]);
	}
}

//Program functions
void TIMER_INTERRUPT_ISR(void){
	//Clear timer interrupt flag
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
//
//	//printf("LOG - displayDecimalNumber: %d \n", displayDecimalNumber);
//
	//Increment coutner
	if(dac_counter + 1 >= 12){ //Reset counter if needed
		dac_counter = 0;
	}
	else{
		dac_counter++;
	}

}

//void convertDecToHex(int decimalValue, char * resultArr){
//	int quotient, remainder;
//	int i, j = 0;
//
//	quotient = decimalValue;
//
//	while (quotient != 0)
//	{
//		remainder = quotient % 16;
//		if (remainder < 10)
//			(*resultArr)[j++] = 48 + remainder;
//		else
//			(*resultArr)[j++] = 55 + remainder;
//		quotient = quotient / 16;
//	}
//}

void ENTER_PUSHB_INTERRUPT(){

	setDelay(30); //Software deboncing

//	//Increment coutner
//	if(dac_counter + 1 >= 12){ //Reset counter if needed
//		dac_counter = 0;
//	}
//	else{
//		dac_counter++;
//	}

	if(temperatureToggle)  temperatureToggle = false;
	else temperatureToggle = true;

	//Clear interrupt flag
	GPIOIntClear(GPIO_PORTE_BASE, GPIO_PIN_2);
}

//void DisplayDacValue(){
//
//}
