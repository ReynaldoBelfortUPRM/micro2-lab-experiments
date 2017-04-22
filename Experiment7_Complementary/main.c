/*
 * main.c
 * Main program for TIVA MCU
 * Complementary Task: Digital Alarm Clock
 *
 * Station 13
 * Lab Team:
 * Emmanuel Ramos
 * Reynaldo Belfort
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "driverlib/i2c.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"

//Custom libraries
#include "MIL_LCD_lib.h"
#include "tivaUtils.h"

 //Address definitions for the DS1307 chip
 #define SLAVE_ADDRESS 0x68
 #define SEC 0x00
 #define MIN 0x01
 #define HRS 0x02
 #define DAY 0x03
 #define DATE 0x04
 #define MONTH 0x05
 #define YEAR 0x06
 #define CNTRL 0x07

//Constant definition for LCD positions
#define POSITION_DATE_MONTH 5
#define POSITION_DATE_DAY 8
#define POSITION_DATE_YEAR 11
#define POSITION_TIME_HOUR 45
#define POSITION_TIME_MIN 48
#define POSITION_TIME_SEC 51

//Constant definitions for PWM Module
#define DESIRED_PWM_FRECUENCY 440 //In Hz
#define DUTY_CYCLE 0.50 //% DutyCycle (decimal value)

volatile uint32_t pwmClockFreq = 0;
volatile uint32_t pwmLoadValue = 0;

//Global variables
uint32_t _globalSystemClock;
unsigned short time_sec, time_min, time_hour, date_day, date_month, date_year, alarm_sec, alarm_min, alarm_hour;
//Define flags
uint8_t CLOCK_STATE, CURRENT_DISPLAY_INFO, ENTER_PUSH_FLAG, VALUE_POSITION;
short UP_DOWN_PUSH_VAL;


//initialize I2C module 3
//Slightly modified version of TI's example code
void InitializeI2C(void)
{
  //enable I2C module 3
  SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
  //reset module
  SysCtlPeripheralReset(SYSCTL_PERIPH_I2C2);
  //enable GPIO peripheral that contains I2C 3
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);


  // Select the I2C function for these pins.
  GPIOPinTypeI2CSCL(GPIO_PORTE_BASE, GPIO_PIN_4);
  GPIOPinTypeI2C(GPIO_PORTE_BASE, GPIO_PIN_5);
  // Configure the pin muxing for I2C3 functions on port D0 and D1.
  GPIOPinConfigure(GPIO_PE4_I2C2SCL);
  GPIOPinConfigure(GPIO_PE5_I2C2SDA);

  I2CTxFIFOConfigSet(I2C2_BASE, I2C_FIFO_CFG_TX_MASTER);    // dudable **

  // Enable and initialize the I2C3 master module. Use the system clock for
  // the I2C3 module. The last parameter sets the I2C data transfer rate.
  // If false the data rate is set to 100kbps and if true the data rate will
  // be set to 400kbps.
  I2CMasterInitExpClk(I2C2_BASE, _globalSystemClock, false);
  //clear I2C FIFOs
  //HWREG(I2C0_BASE + I2C_O_FIFOCTL) = 80008000;
  I2CRxFIFOFlush(I2C2_BASE);    //dudable ***
  I2CTxFIFOFlush(I2C2_BASE);    // dudable ***
}

//sends an I2C command to the specified slave
void I2CSend(uint8_t slave_addr, uint8_t num_of_args, ...)
{
  // Tell the master module what address it will place on the bus when
  // communicating with the slave.
  I2CMasterSlaveAddrSet(I2C2_BASE, slave_addr, false);
  //stores list of variable number of arguments
  va_list vargs;
  //specifies the va_list to "open" and the last fixed argument
  //so vargs knows where to start looking
  va_start(vargs, num_of_args);
  //put data to be sent into FIFO
  I2CMasterDataPut(I2C2_BASE, va_arg(vargs, uint32_t));
  //if there is only one argument, we only need to use the
  //single send I2C function
  if(num_of_args == 1)
  {
    //Initiate send of data from the MCU
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_SEND);
    // Wait until MCU is done transferring.
    while(I2CMasterBusy(I2C2_BASE));
    //"close" variable argument list
    va_end(vargs);
  }
  //otherwise, we start transmission of multiple bytes on the
  //I2C bus
  else
  {
    //Initiate send of data from the MCU
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    // Wait until MCU is done transferring.
    while(I2CMasterBusy(I2C2_BASE));
    //send num_of_args-2 pieces of data, using the
    //BURST_SEND_CONT command of the I2C module
    unsigned char i;
    for(i = 1; i < (num_of_args - 1); i++)
    {
      //put next piece of data into I2C FIFO
      I2CMasterDataPut(I2C2_BASE, va_arg(vargs, uint32_t));
      //send next data that was just placed into FIFO
      I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
      // Wait until MCU is done transferring.
      while(I2CMasterBusy(I2C2_BASE));
    }
    //put last piece of data into I2C FIFO
    I2CMasterDataPut(I2C2_BASE, va_arg(vargs, uint32_t));
    //send next data that was just placed into FIFO
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    // Wait until MCU is done transferring.
    while(I2CMasterBusy(I2C2_BASE));
    //"close" variable args list
    va_end(vargs);
  }
}

//read specified register on slave device
uint32_t I2CReceive(uint32_t slave_addr, uint8_t reg)
{
  //while(I2CMasterBusy(I2C3_BASE));
  //specify that we are writing (a register address) to the
  //slave device
  I2CMasterSlaveAddrSet(I2C2_BASE, slave_addr, false);
  //specify register to be read
  I2CMasterDataPut(I2C2_BASE, reg);
  //send control byte and register address byte to slave device
  I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);

  //SysCtlDelay(10000);

  //wait for MCU to finish transaction
  while(I2CMasterBusy(I2C2_BASE)){};
  //specify that we are going to read from slave device
  I2CMasterSlaveAddrSet(I2C2_BASE, slave_addr, true);
  //send control byte and read from the register we
  //specified
  I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
  //wait for MCU to finish transaction
  //SysCtlDelay(10000);
  while(I2CMasterBusy(I2C2_BASE)){};
  //return data pulled from the specified register
  return I2CMasterDataGet(I2C2_BASE);
}

//convert dec to bcd
unsigned char dec2bcd(unsigned char val)
{
     return (((val / 10) << 4) | (val % 10));
}
// convert BCD to binary
unsigned char bcd2dec(unsigned char val)
{
 return (((val & 0xF0) >> 4) * 10) + (val & 0x0F);
}

//Set Time
void SetTimeDate(unsigned char sec, unsigned char min, unsigned char hour,unsigned char day, unsigned char date, unsigned char month, unsigned char year)
{
     I2CSend(SLAVE_ADDRESS,8,SEC,dec2bcd(sec),dec2bcd(min),dec2bcd(hour),dec2bcd(day),dec2bcd(date),dec2bcd(month),dec2bcd(year));
}

//Get Time and Date
unsigned char GetClock(unsigned char reg)
{
     unsigned char clockData = I2CReceive(SLAVE_ADDRESS,reg);
     return bcd2dec(clockData);
     //return clockData;
}

void DisplayTimeDateValue_NoRTC(){

	writeMessage("Date:", 5);
	setCursorPosition(0x40);
	writeMessage("Time:", 5);

	//Date values

		setCursorPosition(0x05);

		//Day
		writeChar(date_day/10 + 48);

		writeChar((date_day-((date_day/10)*10)) + 48);

		writeChar('-');   // write character = "-"

		//Month
		writeChar(date_month/10 + 48);

		writeChar((date_month-((date_month/10)*10)) + 48);

		writeChar('-');   // write character = "-"

		//Year
		writeChar(date_year/10 + 48);

		writeChar((date_year-((date_year/10)*10)) + 48);

	//Time values

		setCursorPosition(0x45);

		//Hours
		writeChar(time_hour/10 + 48);

		writeChar((time_hour-((time_hour/10)*10)) + 48);

		writeChar(10 + 48);   // writes semi-colon character = ":"

		//Minutes
		writeChar(time_min/10 + 48);

		writeChar((time_min-((time_min/10)*10)) + 48);

		writeChar(10 + 48);   // writes semi-colon character = ":"

		//Years
		writeChar(time_sec/10 + 48);

		writeChar((time_sec-((time_sec/10)*10)) + 48);

}

void DisplayCurrentRTCValue(){

	setCursorPosition(0x00);
	writeMessage("Date:", 5);
	setCursorPosition(0x40);
	writeMessage("Time:", 5);

	//Date values

	//date_day = GetClock(DAY);

	setCursorPosition(0x05);

	date_day = GetClock(DATE);

	writeChar(date_day/10 + 48);

	writeChar((date_day-((date_day/10)*10)) + 48);

	writeChar('-');   // write character = "-"

	date_month = GetClock(MONTH);
	writeChar(date_month/10 + 48);

	writeChar((date_month-((date_month/10)*10)) + 48);

	writeChar('-');   // write character = "-"

	date_year = GetClock(YEAR);

	writeChar(date_year/10 + 48);

	writeChar((date_year-((date_year/10)*10)) + 48);

	//Time values

	setCursorPosition(0x45);

	time_hour = GetClock(HRS);

	writeChar(time_hour/10 + 48);

	writeChar((time_hour-((time_hour/10)*10)) + 48);

	writeChar(10 + 48);   // writes semi-colon character = ":"

	time_min = GetClock(MIN);
	writeChar(time_min/10 + 48);

	writeChar((time_min-((time_min/10)*10)) + 48);

	writeChar(10 + 48);   // writes semi-colon character = ":"

	time_sec = GetClock(SEC);

	writeChar(time_sec/10 + 48);

	writeChar((time_sec-((time_sec/10)*10)) + 48);

	setDelay(1); //1ms
}

void DisplayCurrentAlarmValue(){

	setCursorPosition(0);
	writeMessage("---Alarm Time---", 16);
	setCursorPosition(0x40);
	writeMessage("Time:", 5);

	//Time values

			//Hours
			writeChar(alarm_hour/10 + 48);

			writeChar((alarm_hour-((alarm_hour/10)*10)) + 48);

			writeChar(10 + 48);   // writes semi-colon character = ":"

			//Minutes
			writeChar(alarm_min/10 + 48);

			writeChar((alarm_min-((alarm_min/10)*10)) + 48);

			writeChar(10 + 48);   // writes semi-colon character = ":"

			//Years
			writeChar(alarm_sec/10 + 48);

			writeChar((alarm_sec-((alarm_sec/10)*10)) + 48);

}

/*
 * Adjust the value specified in the parameters depending on the Up/Down push button action
 *
 * */
void AdjustPositionValue(unsigned short * targetValue, short valueLimit, short currDispPosition){
	//Set cursor to what the user is currently targeting
	setCursorPosition(currDispPosition);

	if(UP_DOWN_PUSH_VAL == 1){ //SW1 push button
		UP_DOWN_PUSH_VAL = -1; //Lower push flag
			//Proceed to increment value
			if((*targetValue) + 1 < valueLimit){
				(*targetValue)++;
			}
			else{
				(*targetValue) = 0;
			}

			//Write target value on LCD screen at the current position
			setCursorPosition(currDispPosition);

			//Write targetValue's MSB value on screen
			writeChar((*targetValue)/10 + 48);
			//Write targetValue's LSB value on screen
			writeChar(((*targetValue)-(((*targetValue)/10)*10)) + 48);

			//Return cursor to it's original position
			setCursorPosition(currDispPosition);
	}
	else if(UP_DOWN_PUSH_VAL == 16){ //SW2 push button
		UP_DOWN_PUSH_VAL = -1; //Lower push flag
		//Proceed to decrement value
		if((*targetValue) - 1 >= 0){
			(*targetValue)--;
		}
		else{
			(*targetValue) = valueLimit;
		}

		//Write target value on LCD screen at the current position
		setCursorPosition(currDispPosition);

		//Write targetValue's MSB value on screen
		writeChar((*targetValue)/10 + 48);
		//Write targetValue's LSB value on screen
		writeChar(((*targetValue)-(((*targetValue)/10)*10)) + 48);

		//Return cursor to it's original position
		setCursorPosition(currDispPosition);
	}

}

void RunDateSetup(){
	VALUE_POSITION = 0;

	while(VALUE_POSITION < 3){

		switch(VALUE_POSITION){
			case 0:
				AdjustPositionValue(&date_month, 31, POSITION_DATE_MONTH);
				break;
			case 1:
				AdjustPositionValue(&date_day, 12, POSITION_DATE_DAY);
				break;
			case 2:
				AdjustPositionValue(&date_year, 99, POSITION_DATE_YEAR);
				break;
		}
	}
}

void RunTimeSetup(){
	VALUE_POSITION = 0;

	while(VALUE_POSITION < 3){

		switch(VALUE_POSITION){
			case 0:
				AdjustPositionValue(&time_hour, 23, POSITION_TIME_HOUR);
				break;
			case 1:
				AdjustPositionValue(&time_min, 59, POSITION_TIME_MIN);
				break;
			case 2:
				AdjustPositionValue(&time_sec, 59, POSITION_TIME_SEC);
				break;
		}
	}
}

void RunAlarmSetup(){
	//Clear LCD screen to display alarm info
	clearLCD();
	DisplayCurrentAlarmValue();
	VALUE_POSITION = 0;

	while(VALUE_POSITION < 3){

		switch(VALUE_POSITION){
			case 0:
				AdjustPositionValue(&alarm_hour, 23, POSITION_TIME_HOUR);
				break;
			case 1:
				AdjustPositionValue(&alarm_min, 59, POSITION_TIME_MIN);
				break;
			case 2:
				AdjustPositionValue(&alarm_sec, 59, POSITION_TIME_SEC);
				break;
		}
	}

	CLOCK_STATE = 1;
}

void ENTER_PUSHB_INTERRUPT(){

	setDelay(30); //Software deboncing

	//Modify values according to the current state of the Digital Clock
	switch(CLOCK_STATE){
		case 0: //Clock setup state
			//Change to next positon for time, date or alarm time
			VALUE_POSITION++;
			break;
		case 1: //Clock & Alarm display state
			CURRENT_DISPLAY_INFO = CURRENT_DISPLAY_INFO ^ 0x01; //Toggle flag value to change what is displayed on LCD
			clearLCD(); //So LCD screen any old messages that the new display information won't overrride
			break;
		case 2: //Alarm triggered state
			ENTER_PUSH_FLAG = true; //Send signal to stop alarm
			break;
	}

	//Clear interrupt flag
	GPIOIntClear(GPIO_PORTA_BASE, GPIO_PIN_4);
}

void UP_DOWN_PUSHB_INTERRUPT(){

	setDelay(30); //Software debouncing

	//Get which button was interrumped
	UP_DOWN_PUSH_VAL = GPIOIntStatus(GPIO_PORTF_BASE,true);

	//Clear interrupt flag
	GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4 | GPIO_PIN_0);
}

int main(void) {

	//--------------------MCU Initialization------------------------
		//Set MCU to 40MHz
		SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

		//initialize I2C module 3
		_globalSystemClock = SysCtlClockGet();
		InitializeI2C();

		//--------Push button initialization----------
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);  //Enable PB F0 & F4
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);  //Enable ENTER PB C4

		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
		HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
		HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
		GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_IN);
		GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
		//Port F interrupts
		GPIOIntRegister(GPIO_PORTF_BASE, UP_DOWN_PUSHB_INTERRUPT);
		GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_RISING_EDGE);
		//Port C interrupts (LCD Module Push Button)
		GPIOIntRegister(GPIO_PORTA_BASE, ENTER_PUSHB_INTERRUPT);
		GPIOIntTypeSet(GPIO_PORTA_BASE, GPIO_PIN_4, GPIO_RISING_EDGE);
		GPIODirModeSet(GPIO_PORTA_BASE, GPIO_PIN_4, GPIO_DIR_MODE_IN);
		GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_4, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU); //Set up pull down push button
		//Interrupt enable
		GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0);
		GPIOIntEnable(GPIO_PORTA_BASE, GPIO_PIN_4);
		//--------------------

		//------------PWM Initialization------------
		 //Clock the PWM module by the system clock
		 SysCtlPWMClockSet(SYSCTL_PWMDIV_32); //Divide system clock by 32 to run the PWM at 1.25MHz

		 //Enabling PWM1 module and Port D
		 SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
		 SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD); //Port where the PWM pin will be selected

		 //Selecting PWM generator 0 and port D pin 0 (PD0) aa a PWM output pin for module one
		 GPIOPinTypePWM(GPIO_PORTD_BASE, GPIO_PIN_0); //Set Port D pin 0 as output //TODO Checkout which ports can be used for PWM functionallity
		 GPIOPinConfigure(GPIO_PD0_M1PWM0); //Select PWM Generator 0

		 //Determine period register load value
		 pwmClockFreq = SysCtlClockGet() / 32; //TODO as Isnt the same as ROM_SysCtlPWMClockSet(SYSCTL_PWMDIV_64);? Is something being repeated?
		 pwmLoadValue = (pwmClockFreq / DESIRED_PWM_FRECUENCY) - 1; //Load Value = (PWMGeneratorClock * DesiredPWMPeriod) - 1
		 PWMGenConfigure(PWM1_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN); //Set a count-down generator type
		 PWMGenPeriodSet(PWM1_BASE, PWM_GEN_0, pwmLoadValue); //Set PWM period determined by the load value

		 //Setup PWM Pulse Width Duty Cycle
		 PWMPulseWidthSet(PWM1_BASE, PWM_OUT_0, pwmLoadValue * DUTY_CYCLE);
		 PWMOutputState(PWM1_BASE, PWM_OUT_0_BIT, false); //Start with the PWM Gen off
		 PWMGenEnable(PWM1_BASE, PWM_GEN_0); //Enable PWM Generator
		 //-------------------------------------------

		//--------LCD Setup--------
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);  // Enable, RS and R/W port for LCD Display
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);  // Data port for LCD display
		//Set LCD pins as outputs
		GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_5);
		GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, ENTIRE_PORT);
		//-------------------------

		//Variable initialization
		time_sec = time_min = time_hour = date_day = date_month = date_year = alarm_sec = alarm_min = alarm_hour = 0;
		CLOCK_STATE = 0;
    //--------------------------------------------------------

    //----LCD screen initialization----
    initializeLCD();
    DisplayTimeDateValue_NoRTC(); //Display initial Date & Time values
    //-------------------------

    //Run a series of setups to initialize RTC values
    RunDateSetup();
    RunTimeSetup();
    RunAlarmSetup();

    //Set the current time & day set by the user on RTC chip
    SetTimeDate(time_sec, time_min, time_hour, 0, date_day, date_month, date_year);

    //Alert user, the alarm and time has been set.
    clearLCD();
    writeMessage("-Setup completed", 16);
    setCursorPosition(0x40); //Jump to second line
    writeMessage("& alarm set-", 12);
    setDelay(2000); //2s delay
    clearLCD();

    //Main Loop
    while(true) {

    	switch(CLOCK_STATE){
			case 1: //Clock & Alarm running
				//Select the info that will be displayed on LCD
				switch(CURRENT_DISPLAY_INFO){
					case 0:
						DisplayCurrentRTCValue();
						break;
					case 1:
						DisplayCurrentAlarmValue();
						break;
				}

				//Check if alarm has to be triggered
				if((time_sec == alarm_sec) && (time_min == alarm_min) &&  (time_hour == alarm_hour)){
					//All LCD messages should be written first before turning on buzzer in order to avoid LCD vs. Buzzer interference
					//Write alarm message on LCD
					clearLCD();
					DisplayCurrentAlarmValue();
					setCursorPosition(0x00);
					writeMessage("ALARM TRIGGERED ", 16);
					//Turn ON buzzer
					PWMOutputState(PWM1_BASE, PWM_OUT_0_BIT, true);
					//Jump to alarm clock state
					CLOCK_STATE = 2;
				}
				break;

			case 2: //Alarm triggered
				if(ENTER_PUSH_FLAG){
					//Turn OFF buzzer & return to STATE 1
					PWMOutputState(PWM1_BASE, PWM_OUT_0_BIT, false);
					ENTER_PUSH_FLAG = false;
					CLOCK_STATE = 1;
				}
				break;
    	}
    }
}
