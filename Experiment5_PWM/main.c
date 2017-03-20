/*
 * main.c
 * Main program for TIVA MCU - Experiment 4
 * Digital Tachometer
 * Complementary Task Excercise
 *
 * Lab Team:
 * Emmanuel Ramos
 * Reynaldo Belfort
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
//#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "inc/hw_gpio.h"
#include "driverlib/rom.66h"
//#include "customLibs/MIL_LCD_lib.h"

#define DESIRED_PWM_FRECUENCY 55

//Note Volatile guarantees that the comoiler will not eliminate the variables with that keywoerd

volatile uint32_t pwmClockFreq = 0;
volatile uint32_t pwmLoadValue = 0;


int main(void) {
	
	//----MCU Initialization----

	//TODO NOTE: ROM version of the API functions is used to reduce code size

	//SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN); //Set-up the clocking of the MCU to 40MHz
	ROM_SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
	//Clock the PWM module by the system clock
	ROM_SysCtlPWMClockSet(SYSCTL_PWMDIV_64); //Divide system clock by 64 to run the PWM at 625kHz

	//Enabling PWM1 module and Port D
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD); //Port where the PWM pin will be selected
	//ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); //TODO Why needed?

	//Selecting PWM generator 0 and port D pin 0 (PD0) aa a PWM output pin for module one
	ROM_GPIOPinTypePWM(GPIO_PORTD_BASE, GPIO_PIN_0); //Set Port D pin 0 as output //TODO Checkout which ports can be used for PWM functionallity
	ROM_GPIOPinConfigure(GPIO_PD0_M1PWM0); //Select PWM Generator 0

	pwmClockFreq = SysCtlClockGet() / 64; //TODO as Isnt the same as ROM_SysCtlPWMClockSet(SYSCTL_PWMDIV_64);? Is something being repeated?
	pwmLoadValue = (pwmClockFreq / DESIRED_PWM_FRECUENCY) - 1;
	PWMGenConfigure(PMW1_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN); //Set a down-count generator type
	PWMGenPeriodSet(PWM1_BASE, PWM_GEN_0, pwmLoadValue); //Set PWM period determined by the load value

	ROM_PWMPulseWidthSet(PWM1_BASEPWM_OUT_0, pwmLoadValue / 2); //TODO 50% Duty Cyclen???
	ROM_PWMOutputState(PWM1_BASE, PWM_OUT_0_BIT, true); //
	ROM_PWMGenEnable(PWM1_BASE, PWM_GEN_0); //Enable PWM Generator

	//Main loop
	while(1){

	}
}
