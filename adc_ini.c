/* 
 * ADC initialization 
 * This code was taken entirely from the PIGPIO library's example page: 
 * http://abyz.me.uk/rpi/pigpio/examples.html under SPI Bit Bang 3202 link.
 * We slightly modified the code to instead work with our MCP3204 and only one
 * ADC. This required only a few changes to certain definitions and variables.
 * Only part of the example code has been pasted to this file because we 
 * only want our ADC initialization to be called once. The rest of the modified
 * code can be found in adc.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wiringPi.h>
#include <pigpio.h>

#define SPI_SS 8 // GPIO for slave select.

#define ADCS 1    // Number of connected MCP3202. (MCP3204 for us)

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
#define NOS 1000
#define FS 10000
#define MAX_RUNTIME 60
#define MAX_FFTS 6000

float t[MAX_FFTS];
int count;

rawSPI_t rawSPI =
{
   .clk     =  11, // GPIO for SPI clock.
   .mosi    = 10, // GPIO for SPI MOSI.
   .ss_pol  =  1, // Slave select resting level.
   .ss_us   =  1, // Wait 1 micro after asserting slave select.
   .clk_pol =  0, // Clock resting level.
   .clk_pha =  0, // 0 sample on first edge, 1 sample on second edge.
   .clk_us  =  1, // 2 clocks needed per bit so 500 kbps.
};

// Need to set GPIO as outputs otherwise wave will have no effect.
int adc_ini(int *botCB,int *topOOL,float *cbs_per_reading) {
   //int topOOL, botCB;
   //float cbs_per_reading;
   int i, wid, offset;
   int pause = 0;
   char buf[2];
   gpioPulse_t final[2];
   rawWaveInfo_t rwi;
   
   //Initialize time array
   for (count = 0; count < MAX_FFTS; count++) {
	  t[count] = (float)count * NOS / FS;
   } 

   gpioSetMode(rawSPI.clk,  PI_OUTPUT);
   gpioSetMode(rawSPI.mosi, PI_OUTPUT);
   gpioSetMode(SPI_SS,      PI_OUTPUT);

   gpioWaveAddNew(); // Flush any old unused wave data.

   offset = 0;

   /*
      Now construct lots of bit banged SPI reads.  Each ADC reading
      will be stored separately.  We need to ensure that the
      buffer is big enough to cope with any reasonable rescehdule.
      In practice make the buffer as big as you can.
   */

   for (i=0; i<BUFFER; i++)
   {
      //buf[0] = 0xC0; // Start bit, single ended, channel 0.
      
      //Modified for MCP3204: start bit, single ended, channel 0
      buf[0] = 0xB0;

      rawWaveAddSPI(&rawSPI, offset, SPI_SS, buf, 2, BX, B0, B0);

      /*
         REPEAT_MICROS must be more than the time taken to
         transmit the SPI message.
      */

      offset += REPEAT_MICROS;
   }

   // Force the same delay after the last reading.

   final[0].gpioOn = 0;
   final[0].gpioOff = 0;
   final[0].usDelay = offset;

   final[1].gpioOn = 0; // Need a dummy to force the final delay.
   final[1].gpioOff = 0;
   final[1].usDelay = 0;

   gpioWaveAddGeneric(2, final);

   wid = gpioWaveCreate(); // Create the wave from added data.

   if (wid < 0)
   {
      fprintf(stderr, "Can't create wave, %d too many?\n", BUFFER);
      return 1;
   }

   /*
      The wave resources are now assigned,  Get the number
      of control blocks (CBs) so we can calculate which reading
      is current when the program is running.
   */

   rwi = rawWaveInfo(wid);

   /*printf("# cb %d-%d ool %d-%d del=%d ncb=%d nb=%d nt=%d\n",
      rwi.botCB, rwi.topCB, rwi.botOOL, rwi.topOOL, rwi.deleted,
      rwi.numCB,  rwi.numBOOL,  rwi.numTOOL);*/

   /*
      CBs are allocated from the bottom up.  As the wave is being
      transmitted the current CB will be between botCB and topCB
      inclusive.
   */

   *botCB = rwi.botCB;

   /*
      Assume each reading uses the same number of CBs (which is
      true in this particular example).
   */

   *cbs_per_reading = (float)rwi.numCB / (float)BUFFER;

   /*printf("# cbs=%d per read=%.1f base=%d\n",
      rwi.numCB, *cbs_per_reading, *botCB);*/

   /*
      OOL are allocated from the top down. There are BITS bits
      for each ADC reading and BUFFER ADC readings.  The readings
      will be stored in topOOL - 1 to topOOL - (BITS * BUFFER).
   */

   *topOOL = rwi.topOOL;

   fprintf(stderr, "ADC Initialized...\n");
   //printf("cbs_per_reading: %f %d %d\n", *cbs_per_reading, *botCB, *topOOL);

   if (pause) time_sleep(pause); // Give time to start a monitor.

   gpioWaveTxSend(wid, PI_WAVE_MODE_REPEAT);
   
   return 0;

   //start = time_time();
}
