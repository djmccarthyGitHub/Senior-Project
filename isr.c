#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <unistd.h>
#include <time.h>

/*
    #       NAME          TYPE
    12  Motion Sensor    Input
    18  Kill Switch      Input
    23  Manual Dispense  Input
    3   LED              Output
    6   Valve Dispense   Output
    16  Pump             Output
*/


void fillingAlgorithm(void)
{

}

void motionSensor(void)
{
    if (!digitalRead(26)) {
	printf("Motion sensor activated\n");
        digitalWrite(9, HIGH);  // blink LED
        delay(500);
        digitalWrite(9, LOW);
        delay(500);
        digitalWrite(9, HIGH);
        delay(500);
        digitalWrite(9, LOW);
        delay(500);
        digitalWrite(9, HIGH);  // keep LED on
        digitalWrite(27, HIGH); //Turn on pump
        delay(100);
        digitalWrite(22, HIGH);  // Open Valve
        // start recording data
        //fillingAlgorithm();
    }
    else {
    	printf("Motion sensor deactivated\n");
    	digitalWrite(22, LOW);  // close valve
    	delay(100);
    	digitalWrite(27, LOW); //Turn off pump
    	digitalWrite(9, LOW);  // turn off LED
    	// save data to file
    }
}


void killSwitch(void)
{
    if (!digitalRead(1)) {
	digitalWrite(22, LOW); //close valve
	digitalWrite(27, LOW); //turn off pump
    	printf("Killswitch flipped, system shutting down\n");
    	// save data to file
    }
    while (!digitalRead(1)) {
    	digitalWrite(9, HIGH);
    	delay(100);
    	digitalWrite(9, LOW);
    	delay(100);
    }

    //graph
    // exit
}


void manualButton(void)
{
    if (digitalRead(4)) {
    	printf("Manual Dispense activated");
    	digitalWrite(9, HIGH);  // turn on LED
    	digitalWrite(27, HIGH); //Start pump
    	delay(100);
    	digitalWrite(22, HIGH);  // Open valve
    }
    else {
    	printf("Manual Dispense deactivated");
   	digitalWrite(22, LOW);  // Close valve
    	delay(100);
    	digitalWrite(27, LOW); //Stop pump
    	digitalWrite(9, LOW);  // turn off LED
    }
}

int main (void)
{
	printf("Starting\n");
	
    wiringPiSetup();

	    printf("Setup complete\n");


    // Set pin modes
    pinMode(9, OUTPUT); //LED
    pinMode(22, OUTPUT); //valve
    pinMode(27, OUTPUT); //pump
    pinMode(26, INPUT); //Motion sens
    pinMode(1, INPUT); //Killswitch
    pinMode(4, INPUT); //Push btn
     printf("Pins set\n");
    // Init values
    
    digitalWrite(9, LOW);
    digitalWrite(22, LOW);

    // Attach ISR fudnctions to appropriate inputs
    wiringPiISR(1, INT_EDGE_BOTH, &killSwitch);
    wiringPiISR(26, INT_EDGE_BOTH, &motionSensor);
    wiringPiISR(4, INT_EDGE_BOTH, &manualButton);
    
    printf("Entering loop\n");
    // run continuously
    while(1);

    return 0;
}
