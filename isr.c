/*
 * This file contains our main function which
 * initializes all gpio and the ISRs for our motion
 * sensor, push button, and killswitch. 
 *
 * The program gnuplot is used to plot the 
 * acceleration vs time and position vs time 
 * values from our freq_peak and accel_peak arrays.
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pigpio.h>
#include "adc.h"

#define NOS 1000 //number of samples per FFT
#define FS 10000 //tested sampling frequency of roughly 10kHz
#define MAX_RUNTIME 60 //60 seconds max runtime
#define MAX_FFTS 6000 //MAX_RUNTIME * FS / NOS

int start = 0; //compared to motionSensor tick value (used for debounce)
int start_ks = 0; //compared to killSwitch tick value (used for debounce)
int last_level = 1;
int count; //Number of FFTS taken
int flag; //Indicates criteria has been met for dispension halt
int botCB, topOOL; //values used for ADC
int opened = 0; //Indicates files stored into FP
int repeat; //Number of repeat freq_peak values
float cbs_per_reading; //used for ADC

float freq_peak[MAX_FFTS];
float accel_peak[MAX_FFTS];
float t[MAX_FFTS];
float start_freq, end_freq;
float thres_passed;

FILE *fp1, *fp2; //Stores Accel v Time and Pos v Time in file
FILE *plot_accel, *plot_pos; //Used to pull up plots for AvT and PvT

void killSwitch(int gpio, int level, uint32_t tick);

void motionSensor(int gpio, int level, uint32_t tick)
{
    if (tick - start > 250000) {
	//Check for level (active low)
	if (!level && last_level != level && gpioRead(17)) {
	printf("Motion sensor activated\n");
	//Open files for plotting
	fp1 = fopen("PvT.txt", "wb");
	fp2 = fopen("AvT.txt", "wb");
	plot_accel = popen("gnuplot -persist", "w");
	plot_pos = popen("gnuplot -persist", "w");
	opened = 1;
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
	time_sleep(1.6);
	// start recording data
	// call ADC/FFT
	adc(botCB, topOOL, &cbs_per_reading);
	printf("ADC complete\n");
	
	// Momentarily disable killswitch to write FFT values
	// to files and display graphs 
	gpioSetISRFunc(17, FALLING_EDGE, 0, NULL);
	
	//Store and reset freq_peak and accel_peak values	
	for (int j = 0; j < count; j++) {
		fprintf(fp1, "%f\t%f\n", t[j], freq_peak[j]);
		fprintf(fp2, "%f\t%f\n", t[j], accel_peak[j]);
		freq_peak[j] = 0;
		accel_peak[j] = 0;
	}
	
	fclose(fp1);
	fclose(fp2);
	
	//Plot Acceleration vs Time and Position vs Time
	fprintf(plot_accel, "set term wxt title 'Acceleration vs Time'\n");
	fprintf(plot_accel, "set xlabel \"Time (s)\"\n");
	fprintf(plot_accel, "set ylabel \"Acceleration (Hz/s^2)\"\n");
	fprintf(plot_accel, "plot 'AvT.txt' w lp\n");
	fprintf(plot_pos, "set term wxt title 'Position vs Time'\n");
	fprintf(plot_pos, "set xlabel \"Time (s)\"\n");
	fprintf(plot_pos, "set ylabel \"Peak Position (Hz)\"\n");
	fprintf(plot_pos, "plot 'PvT.txt' w lp\n");
	fclose(plot_accel);
	fclose(plot_pos);
	//printf("Start freq: %f\t End freq: %f\n", start_freq, end_freq);
	//printf("Time crossed: %f\n", thres_passed);
	
	//Indicate files have been closed to other processes
	opened = 0;
	
	//Used as another debounce measure
	//When sensor goes high we do not want it to 
	//trigger dispense if it bounces low
	last_level = level;
	//Reenable killswitch ISR
	gpioSetISRFunc(17, FALLING_EDGE, 0, killSwitch);
	time_sleep(0.1);
	}
	
	// If statement determines when dispension should halt
	// (when criteria from FFT algorithm has been met or
	// bottle has been pulled away from IR sensor)
	if (gpioRead(25) || flag) {
		if (flag) {
			printf("Fill completed\n");
			printf("Repeats: %d\n", repeat);
			flag = 0;
		}
		else {
			printf("Motion sensor deactivated\n");
		}
		gpioWrite(6, 0);  // close valve
		time_sleep(0.100);
		gpioWrite(16, 0); //Turn off pump
		gpioWrite(3, 0);  // turn off LED
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
	//Disable other interrupts
	gpioSetISRFunc(25, EITHER_EDGE, 0, NULL);
	gpioSetISRFunc(23, EITHER_EDGE, 0, NULL);

	if (tick - start_ks > 250000) {
		//printf("In if statement\n");

    		if (!gpioRead(17)) {
			gpioWrite(6, 0); //close valve
			gpioWrite(16, 0); //turn off pump
    			printf("Killswitch engaged, system in standby\n");
    			// save data to file
			if (opened) {
				for (int j = 0; j < count; j++) {
					fprintf(fp1, "%f\t%f\n", t[j], freq_peak[j]);
					fprintf(fp2, "%f\t%f\n", t[j], accel_peak[j]);
					freq_peak[j] = 0;
					accel_peak[j] = 0;
				}	
				fclose(fp1);
				fclose(fp2);
				//Plot values taken from FFTs
				fprintf(plot_accel, "set term wxt title 'Acceleration vs Time'\n");
				fprintf(plot_accel, "set xlabel \"Time (s)\"\n");
				fprintf(plot_accel, "set ylabel \"Acceleration (Hz/s^2)\"\n");
				fprintf(plot_accel, "plot 'AvT.txt' w lp\n");
				fprintf(plot_pos, "set term wxt title 'Position vs Time'\n");
				fprintf(plot_pos, "set xlabel \"Time (s)\"\n");
				fprintf(plot_pos, "set ylabel \"Peak Position (Hz)\"\n");
				fprintf(plot_pos, "plot 'PvT.txt' w lp\n");
				fprintf(plot_accel, "plot 'AvT.txt' w lp\n");
				fclose(plot_accel);
				fprintf(plot_pos, "plot 'PvT.txt' w lp\n");
				fclose(plot_pos);
				printf("Start freq: %f\t End freq: %f\n", start_freq, end_freq);
				printf("Time crossed: %f\n", thres_passed);
				printf("Repeats: %d\n", repeat);
				opened = 0;
			}	
    		}
    		while (!gpioRead(17)) {
    			gpioWrite(3, 1);
    			time_sleep(.100);
    			gpioWrite(3, 0);
    			time_sleep(.100);
    		}
		
		flag = 0;
		
		//Enable other interrupts
		gpioSetISRFunc(25, EITHER_EDGE, 0, motionSensor);
		gpioSetISRFunc(23, EITHER_EDGE, 0, manualButton);
		printf("Killswitch disengaged, system ready\n");
	}
	start_ks = tick;
}

int main () {
   	if (gpioInitialise() < 0) return 1;
	gpioSetMode(16, PI_OUTPUT); //Pump
	gpioSetMode(6, PI_OUTPUT); //Valve
	gpioSetMode(25, PI_INPUT); //Motion sensor
	gpioSetMode(3, PI_OUTPUT); //LED
	gpioSetMode(17, PI_INPUT); //Killswitch
	gpioSetMode(23, PI_INPUT); //Push button
	//Initialize all pins used for ADC
	adc_ini(&botCB, &topOOL, &cbs_per_reading); 

	gpioSetISRFunc(25, EITHER_EDGE, 0, motionSensor); //Set motion sensor ISR
	gpioSetISRFunc(17, FALLING_EDGE, 0, killSwitch); //Set killswitch ISR
	gpioSetISRFunc(23, EITHER_EDGE, 0, manualButton); //Push button ISR

	gpioWrite(3, 0); //Set LED low
	gpioWrite(6, 0); //Set valve low
	gpioWrite(16, 0); //Set pump low

	while (1); //Continue running
}
