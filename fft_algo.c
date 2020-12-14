/*
 * This file contains our FFT algorithm. Due to the 
 * often erratic nature of the sampled signals, 3
 * methods were implemented to determine a dispension
 * cut-off time: acceleration threshold met, position threshold
 * met, and the flatline method.
 *
 * The acceleration threshold method is used when the signal's
 * frequency acceleration has surpassed our defined threshold of 
 * 100. The flow is not instantly cut, but instead set to run for
 * 0.4 times longer than its current runtime (determined by the 
 * global count variable).
 *
 * Similarly, the position threshold method is used when the 
 * highest frequency taken from the FFTs has met a certain threshold.
 * This threshold is determined by the signal's start_freq, (which 
 * is the average of the signal's first 10 freq_peak values), 
 * and stored in pos_thres. The flow
 * is stopped 0.43 times longer than its current runtime.
 *
 * Finally the flatline method is used when the highest frequency 
 * taken has not been passed for 40 samples. The flow is stopped
 * right away.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fftw3.h>

#define NOS 1000 //number of samples per FFT
#define THRESHOLD 100 //Freq. accel. threshold in Hz/s^2
#define FS 10000 //tested sampling frequency of roughly 10kHz
#define MAX_RUNTIME 60 //60 seconds max runtime
#define MAX_FFTS 6000 //MAX_RUNTIME * FS /NOS
//FLATLINE is number of identical samples 
//in a row from freq_peaks[] required before cut-off
#define FLATLINE 40 

float freq_peak[MAX_FFTS] ; //stores peak frequnecy values (does not take lower)
float accel_peak[MAX_FFTS]; //stores freq acceleration values
int count; //number of FFTS
float start_freq, end_freq; //avg of first 10 freq_peak & test value
float thres_passed; //test value
int repeat; //number of repeat freq_peak values
float count_thres; //determines cut-off time
int flag; //set when any case has been met
float pos_thres; //position threshold criteria

void fft_algo(double data_arr[]) {
	fftw_complex data_fft[3*NOS];
	int i = 0, j = 0, k = 0;
	float temp = 0;
	float tempf = 0;
	
	double psd_mag;
	double data[3*NOS];
	int index = 0;
	
	//Zero padding
	for (k = 0; k < NOS; k++) {
		data[index] = data_arr[k];
		index++;
	} 
	
	for (j = 0; j < 2 * NOS; j++) {
		data[index] = 0;
		index++;
	} 
	
	//Take FFT and store data in data_fft
	fftw_plan plan = fftw_plan_dft_r2c_1d(3*NOS, data, data_fft, FFTW_ESTIMATE);
	fftw_execute(plan);
	
	// This loop is used to set freq_peak vales. If the peak frequency for current FFT
	// is less than previous FFT it is NOT stored. Instead we have a repeat.
	for (i = 0; i < 3 * NOS / 2; i++) {
		psd_mag = data_fft[i][0] * data_fft[i][0] + data_fft[i][1] * data_fft[i][1];
		if (psd_mag > temp) {
			
			tempf = ((float) i * FS) / (3.0*NOS); //store peak frequency
			if (count != 0 && tempf <= freq_peak[count - 1] ) {
				tempf = freq_peak[count -1];
			}
			temp = psd_mag;
		} 
	}
	
    	//Determine starting position
    	//tested value of 2.13 (adjusted to 1.8)
    	if (count < 10) {
		start_freq += tempf;
	}
	if (count == 10) {
		start_freq = start_freq / 10.0;
		pos_thres = start_freq * 1.8;
		printf("Position threshold: %f\n", pos_thres);
		printf("Start freq: %f\n", start_freq);
	}
	
	//begin storing acceleration (must first have position and velocity samples)
	if (count > 2) {
		accel_peak[count] = tempf - 2 * freq_peak[count-1] + freq_peak[count-2];
	}
	
	freq_peak[count] = tempf;
	
	//Determine if freq_peak has repeat (for flatline case)
	if (freq_peak[count] == freq_peak[count-1]) repeat++;
	else repeat = 0;
	
	/*if (accel_peak[count] >= THRESHOLD || repeat >= FLATLINE) {
		flag = 1;
	}*/
	
	//Store time threshold that passed (test)
	
	//Position threshold case
	//calculated 1.58 from test (adjusted)
	if (count >= 10 && freq_peak[count] >= pos_thres && count_thres > count * 1.43) { //&& !thres_passed) {
		//thres_passed = (float) (count * NOS) / (float) FS;
		//end_freq = freq_peak[count];
		//printf("Passed value: %f\n", freq_peak[count]);
		//printf("Passed value: %f\n", pos_thres);
		count_thres = count * 1.43; //determined through testing
		printf("Position thres case\tExpected fill time: %f\n", (float)(count_thres * NOS) / (float) FS);
		flag = 1;
	}

	//Flatline case
	if (repeat >= FLATLINE && count_thres > count + 1) {
		printf("Flatline case\n");
		count_thres = count + 1;
		flag = 1;
	}

	//Acceleration threshold case
	//calculated 1.3 from test (adjusted to 1.4)
	if (accel_peak[count] > THRESHOLD && count_thres > count * 1.4) {
		
		count_thres = count * 1.4; 
		printf("Accel thres case\tExpected fill time: %f\n", (float)(count_thres * NOS) / (float) FS);
		flag = 1;
	}
	//return flag;
}
