#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fftw3.h>

#define NOS 1000 //number of samples per FFT
#define THRESHOLD 100 //Freq. accel. threshold in Hz/s^2
#define FS 10000 //tested sampling frequency of roughly 10kHz
#define MAX_RUNTIME 60 //60 seconds max runtime
#define MAX_FFTS 6000 //MAX_RUNTIME * FS /NOS
#define FLATLINE 40

float freq_peak[MAX_FFTS] ;
float accel_peak[MAX_FFTS];
int count; //number of FFTS
float start_freq, end_freq;
float thres_passed;
int repeat;
float count_thres;
int flag;
float pos_thres;

void fft_algo(double data_arr[]) {
	fftw_complex data_fft[3*NOS];
	//int flag = 0;
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
	
	fftw_plan plan = fftw_plan_dft_r2c_1d(3*NOS, data, data_fft, FFTW_ESTIMATE);
	fftw_execute(plan);
    
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
    //tested value of 2.13 (adjusted)
    if (count < 10) {
		start_freq += tempf;
	}
	if (count == 10) {
		start_freq = start_freq / 10.0;
		pos_thres = start_freq * 1.8;
		printf("Position threshold: %f\n", pos_thres);
		printf("Start freq: %f\n", start_freq);
	}
	
	if (count > 2) {
		accel_peak[count] = tempf - 2 * freq_peak[count-1] + freq_peak[count-2];
	}
	
	freq_peak[count] = tempf;
	
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
	//calculated 1.3 from test (adjusted)
	if (accel_peak[count] > THRESHOLD && count_thres > count * 1.4) {
		
		count_thres = count * 1.4; 
		printf("Accel thres case\tExpected fill time: %f\n", (float)(count_thres * NOS) / (float) FS);
		flag = 1;
	}
	//return flag;
}
