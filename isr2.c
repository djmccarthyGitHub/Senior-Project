#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pigpio.h>
#include "adc.h"

int start = 0;
int last_level = 1;

void fillingAlgorithm(void)
{
	//call adc
	
}

void motionSensor(int gpio, int level, uint32_t tick)
{
	//printf("%d\n", tick);
	if (tick - start > 250000) {
	
	if (!level && last_level != level) {
	printf("Motion sensor activated\n");
        gpioWrite(3, 1);  // blink LED
        time_sleep(0.500);
        gpioWrite(3, 0);
        time_sleep(0.500);
        gpioWrite(3, 1);
        time_sleep(0.500);
        gpioWrite(3, 0);
        time_sleep(0.500);
        gpioWrite(3, 1);  // keep LED on
        gpioWrite(16, 1); //Turn on pump
        time_sleep(0.500);
        gpioWrite(6, 1);  // Open Valve
	time_sleep(0.2);
        // start recording data
	// call ADC/FFT
	adc();
	last_level = level;
    }
    else if (gpioRead(25)) {
    	printf("Motion sensor deactivated\n");
    	gpioWrite(6, 0);  // close valve
        time_sleep(0.100);
    	gpioWrite(16, 0); //Turn off pump
    	gpioWrite(3, 0);  // turn off LED
    	// save data to file
	last_level = level;
    }
	}
    start = tick;
}




void manualButton(int gpio, int level, uint32_t tick)
{
    if (level) {
	gpioSetISRFunc(25, EITHER_EDGE, 0, NULL);
    	printf("Manual Dispense activated\n");
    	gpioWrite(3, 1);  // turn on LED
    	gpioWrite(16, 1); //Start pump
    	time_sleep(0.1);
	gpioWrite(6, 1);  // Open valve
    }
    else {
    	printf("Manual Dispense deactivated\n");
   	gpioWrite(6, 0);  // Close valve
	time_sleep(0.1);
    	gpioWrite(16, 0); //Stop pump
    	gpioWrite(3, 0);  // turn off LED
	gpioSetISRFunc(25, EITHER_EDGE, 0, motionSensor);
    }
}

void killSwitch(int gpio, int level, uint32_t tick)
{	
	gpioSetISRFunc(25, EITHER_EDGE, 0, NULL);
	gpioSetISRFunc(23, EITHER_EDGE, 0, NULL);

	if (tick - start > 250000) {
		printf("In if statement\n");

    if (!gpioRead(17)) {
	gpioWrite(6, 0); //close valve
	gpioWrite(16, 0); //turn off pump
    	printf("Killswitch engaged, system in standby\n");
    	// save data to file
    }
    while (!gpioRead(17)) {
    	gpioWrite(3, 1);
    	time_sleep(.100);
    	gpioWrite(3, 0);
    	time_sleep(.100);
    }

	gpioSetISRFunc(25, EITHER_EDGE, 0, motionSensor);
	gpioSetISRFunc(23, EITHER_EDGE, 0, manualButton);
	printf("Killswitch disengaged, system ready\n");
    //graph
    // exit
	}
	start = tick;
}

int main () {
	sleep(.2);

   	if (gpioInitialise() < 0) return 1;
	gpioSetMode(16, PI_OUTPUT);
	gpioSetMode(6, PI_OUTPUT);
	gpioSetMode(25, PI_INPUT);
	gpioSetMode(3, PI_OUTPUT);
	gpioSetMode(17, PI_INPUT);
	gpioSetMode(23, PI_INPUT);

	gpioSetISRFunc(25, EITHER_EDGE, 0, motionSensor);
	gpioSetISRFunc(17, FALLING_EDGE, 0, killSwitch);
	gpioSetISRFunc(23, EITHER_EDGE, 0, manualButton);

	gpioWrite(3, 0);
	gpioWrite(6, 0);
	gpioWrite(16, 0);

	while (1);
}
