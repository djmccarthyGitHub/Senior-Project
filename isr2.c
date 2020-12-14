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

int start = 0;
int start_ks = 0;
int last_level = 1;
int count;
int flag;
int botCB, topOOL;
int opened = 0;
int repeat;
int btn;
float cbs_per_reading;

float freq_peak[MAX_FFTS];
float accel_peak[MAX_FFTS];
float t[MAX_FFTS];
float start_freq, end_freq;
float thres_passed;

FILE *fp1, *fp2;
FILE *plot_accel, *plot_pos;

void killSwitch(int gpio, int level, uint32_t tick);

void motionSensor(int gpio, int level, uint32_t tick)
{
    //printf("%d\n", tick);
    if (tick - start > 250000) {
	
	if (!level && !btn && last_level != level && gpioRead(17)) {
	printf("Motion sensor activated\n");
	//printf("cbs_per_reading in isr: %f %d %d  \n", cbs_per_reading, botCB, topOOL);
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
	
	gpioSetISRFunc(17, FALLING_EDGE, 0, NULL);
	if (opened) {
		for (int j = 0; j < count; j++) {
			fprintf(fp1, "%f\t%f\n", t[j], freq_peak[j]);
			fprintf(fp2, "%f\t%f\n", t[j], accel_peak[j]);
			freq_peak[j] = 0;
			accel_peak[j] = 0;
		}
	
	
	fclose(fp1);
	fclose(fp2);
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
	opened = 0;
	}
	last_level = level;
	gpioSetISRFunc(17, FALLING_EDGE, 0, killSwitch);
	time_sleep(0.1);
	}
	
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
		// save data to file
		last_level = level;
	}
		btn = 0;
    }
	start = tick;
}




void manualButton(int gpio, int level, uint32_t tick)
{
    if (gpioRead(23)) {
	gpioSetISRFunc(25, EITHER_EDGE, 0, NULL);
	btn = 1;
    	printf("Manual Dispense activated\n");
    	gpioWrite(3, 1);  // turn on LED
    	gpioWrite(16, 1); //Start pump
    	time_sleep(0.1);
	gpioWrite(6, 1);  // Open valve
    }
    else {
    	printf("Manual Dispense deactivated\n");
	gpioSetISRFunc(17, FALLING_EDGE, 0, NULL);
   	gpioWrite(6, 0);  // Close valve
	time_sleep(0.1);
    	gpioWrite(16, 0); //Stop pump
    	gpioWrite(3, 0);  // turn off LED
	if (opened) {
		fclose(fp1);
		fclose(fp2);
		fclose(plot_accel);
		fclose(plot_pos);
		opened = 0;
	}
	flag = 0;
	gpioSetISRFunc(17, FALLING_EDGE, 0, killSwitch);
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
			if (opened && !btn) {
				for (int j = 0; j < count; j++) {
					fprintf(fp1, "%f\t%f\n", t[j], freq_peak[j]);
					fprintf(fp2, "%f\t%f\n", t[j], accel_peak[j]);
					freq_peak[j] = 0;
					accel_peak[j] = 0;
				}	
				fclose(fp1);
				fclose(fp2);
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
	btn = 0;
	start_ks = tick;
}

int main () {
   	if (gpioInitialise() < 0) return 1;
	gpioSetMode(16, PI_OUTPUT);
	gpioSetMode(6, PI_OUTPUT);
	gpioSetMode(25, PI_INPUT);
	gpioSetMode(3, PI_OUTPUT);
	gpioSetMode(17, PI_INPUT);
	gpioSetMode(23, PI_INPUT);
	adc_ini(&botCB, &topOOL, &cbs_per_reading); 

	gpioSetISRFunc(25, EITHER_EDGE, 0, motionSensor);
	gpioSetISRFunc(17, FALLING_EDGE, 0, killSwitch);
	gpioSetISRFunc(23, EITHER_EDGE, 0, manualButton);

	gpioWrite(3, 0);
	gpioWrite(6, 0);
	gpioWrite(16, 0);

	while (1);
}
