#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <unistd.h>
#include <time.h>

/*
    #       NAME          TYPE
    0    Motion Sensor    Input
    1    Kill Switch      Input
    2    Manual Dispense  Input
    3    LED              Output
    6    Valve Dispense   Output
    7    Microphone       Input
*/


void fillingAlgorithm(void)
{

}


void motionSensorRise(void)
{
    printf("Motion sensor deactivated");
    digitalWrite(6, LOW);  // close valve
    digitalWrite(3, LOW);  // turn off LD
    // save data to file
}


void motionSensorFall(void)
{
    printf("Motion sensor activated");
    digitalWrite(3, HIGH);  // blink LED
    delay(500);
    digitalWrite(3, LOW);
    delay(500);
    digitalWrite(3, HIGH);
    delay(500);
    digitalWrite(3, LOW);
    delay(500);
    digitalWrite(3, HIGH);  // keep LED on
    digitalWrite(6, HIGH);  // Open Valve
    // start recording data
    fillingAlgorithm();
}


void killSwitchRise(void)
{
    printf("Killswitch flipped, system shutting down");
    // save data to file
    digitalWrite(3, HIGH);
    delay(100);
    digitalWrite(3, LOW);
    delay(100);
    digitalWrite(3, HIGH);
    delay(100);
    digitalWrite(3, LOW);
    delay(100);
    digitalWrite(3, HIGH);
    delay(100);
    digitalWrite(3, LOW);
    delay(100);
    digitalWrite(3, HIGH);
    delay(100);
    digitalWrite(3, LOW);
    delay(100);
    digitalWrite(3, HIGH);
    delay(100);
    digitalWrite(3, LOW);
    // exit
}


void manualButtonRise(void)
{
    printf("Manual Dispense activated");
    digitalWrite(3, HIGH);  // turn on LED
    digitalWrite(6, HIGH);  // Open valve
}

void manualButtonFall(void)
{
    printf("Manual Dispense deactivated");
    digitalWrite(6, LOW);  // Close valve
    digitalWrite(3, LOW);  // turn off LED
}



int main (void)
{
    wiringPiSetup();

    // Attach ISR functions to appropriate inputs
    wiringPiISR(0, INT_EDGE_RISING, &motionSensorRise);
    wiringPiISR(1, INT_EDGE_RISING, &killSwitchRise);
    wiringPiISR(2, INT_EDGE_RISING, &manualButtonRise);
    wiringPiISR(0, INT_EDGE_FALLING, &motionSensorFall);
    wiringPiISR(2, INT_EDGE_FALLING, &manualButtonFall);

    // Set pin modes
    pinMode(3, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(7, INPUT);

    // Init values
    digitalWrite(3, LOW);
    digitalWrite(6, LOW);

    // run continuously
    while(1);

    return 0;
}
