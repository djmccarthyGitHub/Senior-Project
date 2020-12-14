/*
 * This is the file called to perform ADC on our signal 
 * and store the values collected in data_arr[]. This file
 * also calls the FFT every 1000 samples collected (approximately
 * 0.1 seconds due to our sampling frequency). This code runs as long
 * as determined by the FFT algorithm's changes to the global variable Most of this
 * code is taken from the pigpio library's example code page: 
 * http://abyz.me.uk/rpi/pigpio/examples.html under the 'SPI bit
 * bang MCP3202' link. We have modified to code to work for our 
 * MCP3204 and included the ADC initialization in adc_ini.c. 
 * Below this comment is the original code's disclaimer/explanation
 */


/*
rawMCP3202.c
Public Domain
2016-03-20
gcc -Wall -pthread -o rawMCP3202 rawMCP3202.c -lpigpio
This code shows how to bit bang SPI using DMA.
Using DMA to bit bang allows for two advantages
1) the time of the SPI transaction can be guaranteed to within a
   microsecond or so.
2) multiple devices of the same type can be read or written
  simultaneously.
This code shows how to read more than one MCP3202 at a time.
Each MCP3202 shares the SPI clock, MOSI, and slave select lines but has
a unique MISO line.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpio.h>
#include "adc.h"

#define SPI_SS 8 // GPIO for slave select.

#define ADCS 1    // Number of connected MCP3202.

#define BITS 12            // Bits per reading.
#define BX 8               // Bit position of data bit B11.
#define B0 (BX + BITS - 1) // Bit position of data bit B0.

#define MISO1 9   // ADC 1 MISO.
/*#define MISO2 26  //     2
#define MISO3 13  //     3
#define MISO4 23  //     4
#define MISO5 24  //     */

#define BUFFER 250       // Generally make this buffer as large as possible.

#define REPEAT_MICROS 100 // Reading every x microseconds.

//#define SAMPLES 500000  // Number of samples to take,

#define NOS 1000 //number of samples per FFT
#define FS 10000 //tested sampling frequency of roughly 10k
#define MAX_RUNTIME 60 //60 seconds max runtime
#define MAX_FFTS 6000 //MAX_RUNTIME * FS /NOS
#define FLATLINE 40

int MISO[ADCS]={MISO1}; 
float t[MAX_FFTS]; //index is number of FFTs
float freq_peak[MAX_FFTS]; //FFT computed peak frequencies reached
float accel_peak[MAX_FFTS]; //FFT computed frequency acceleration
int count; //number of FFTS
int flag;
float start_freq, end_freq;
float thres_passed; 
int repeat;
float count_thres = (float)MAX_FFTS;
float pos_thres;

/*
   This function extracts the MISO bits for each ADC and
   collates them into a reading per ADC.
*/

void getReading(
   int adcs,  // Number of attached ADCs.
   int *MISO, // The GPIO connected to the ADCs data out.
   int OOL,   // Address of first OOL for this reading.
   int bytes, // Bytes between readings.
   int bits,  // Bits per reading.
   char *buf) 
{
   int i, a, p;
   uint32_t level;

   p = OOL;

   for (i=0; i<bits; i++)
   {
      level = rawWaveGetOut(p);

      for (a=0; a<adcs; a++)
      {
         putBitInBytes(i, buf+(bytes*a), level & (1<<MISO[a]));
      }

      p--;
   }
}

int adc(int botCB, int topOOL, float *cbs_per_reading)
{
   FILE *fp = fopen("arr_data.txt", "wb");
   //float arr_intv[1000];
   char char_intv [8][1000];
   int i; //, wid , offset;
   //char buf[2];
   //gpioPulse_t final[2];
   char rx[8];
   int sample;
   //float count_thres = (float)MAX_FFTS;
   int val;
   //int flag = 0; // 1:Threshold met  0:Not met
   int cb, reading, now_reading;
   //float cbs_per_reading;
   //rawWaveInfo_t rwi;
   double start, end; 
   int pause;
   double dob_val;
   int s;
   double data_arr[1000];
   thres_passed = 0.0;
   
   //if (argc > 1) pause = atoi(argv[1]); else pause =0;
   count = 0; //number of FFTs taken
   sample = 0; //number of samples taken
   reading = 0;
   pause=0;
   
   //start_freq used in FFT algorithm to determine cut-off
   // (determined from avg of first 10 FFTs)
   // Reset at being of new ADC
   start_freq = 0; 
   
   //similar to start_freq, determines cut-off 
   //for position thres stop method
   end_freq = 0;
   start = time_time();
   
   //repeat is used for flatline stop method
   // incremented in FFT but reset for each ADC
   //call
   repeat = 0;
   
   //Determines how many more times we loop
   //before stopping dispension (determined
   //in algorithm, reset for each ADC call)
   count_thres = (float)MAX_FFTS;

   //printf("cbs_per_block: %f %d %d\n", *cbs_per_reading, botCB, topOOL);
   
   while (!gpioRead(25) && count <= count_thres) //(sample<SAMPLES)
   {
      // Which reading is current?

      cb = rawWaveCB() - botCB;

      now_reading = (float) cb / *cbs_per_reading;

      // Loop gettting the fresh readings.
      //
      // Do nothing while sensor does no detect
	//gpioWrite(26, 0);

      while (now_reading != reading && !gpioRead(25) && count <= count_thres)
      {
	 //Add loop that collects 1000 samples

         /*
            Each reading uses BITS OOL.  The position of this readings
            OOL are calculated relative to the waves top OOL.
         */
         getReading(ADCS, MISO, topOOL - ((reading%BUFFER)*BITS) - 1, 2, BITS, rx);

	 ++sample;
         //printf("%d", ++sample);

	 
	 //print reading
         for (i=0; i<ADCS; i++)
         {
            //   7   6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0
            // B11 B10 B9 B8 B7 B6 B5 B4 | B3 B2 B1 B0  X  X  X  X
	    
	    //switched from 4 shifts left to 5 shifts left
            val = (rx[i*2]<<4) + (rx[(i*2)+1]>>4);
	    dob_val =val * 0.00080566 - 1.65; //value multiplied by resolution (3.3/4096)
            //printf(" %d\t %f",val, dob_val);

	    
	    s = snprintf(char_intv[i], sizeof(char_intv[i]), "%f", dob_val);
	    fprintf(fp, "%s\n", char_intv[i]);

	    //printf(" %s",char_intv[i]);
         }
	    
	 //store into array 
	 data_arr[(sample - 1) % NOS] = dob_val;
	 
	 if((sample % NOS) == NOS - 1) {
		count++;
	 	
		//give OK to FFT algorithm		
		fft_algo(data_arr);
	 }	

         //printf("\n");

         if (++reading >= BUFFER) reading = 0;
      }
      usleep(1000);
   }
   
   end = time_time();

   printf("# %d samples in %.1f seconds (%.0f/s)\n",
      sample, end-start, (float)sample/(end-start));

   fprintf(stderr, "ending...\n");


   if (pause) time_sleep(pause);

   /*int written = fwrite(char_intv, sizeof(char), sizeof(char_intv), fp);
   if (written == 0) printf("Error\n");*/
   fclose(fp);
   
   //if (repeat >= FLATLINE) printf("Flatlined: %d Test: %d\n", repeat, test);

   return 0;
   //gpioWrite(26, 0);
   printf("Function ends\n");
}
