/*
 *  SubFFT.ino - FFT Example with Audio (Sound Detector)
 *  Copyright 2019 Sony Semiconductor Solutions Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <MP.h>

#include "FFT.h"

/*-----------------------------------------------------------------*/
/*
 * FFT parameters
 */
/* Select FFT length */

//#define FFT_LEN 32
//#define FFT_LEN 64
//#define FFT_LEN 128
//#define FFT_LEN 256
//#define FFT_LEN 512
#define FFT_LEN 1024
//#define FFT_LEN 2048
//#define FFT_LEN 4096

/* Number of channels*/
#define MAX_CHANNEL_NUM 1
//#define MAX_CHANNEL_NUM 2
// #define MAX_CHANNEL_NUM 4

#define SAMPLING_RATE   48000 // ex.) 48000, 16000

#define FFT_LEN         1024 // ex.) 128, 256, 1024
#define OVERLAP         (FFT_LEN/2)  // ex.) 0, 128, 256

FFTClass<MAX_CHANNEL_NUM, FFT_LEN> FFT;

/*-----------------------------------------------------------------*/
/*
 * Detector parameters
 */
#define POWER_THRESHOLD       10  //30  // Power
#define LENGTH_THRESHOLD      10  //30  // 20ms
#define INTERVAL_THRESHOLD    100 // 100ms

#define BOTTOM_SAMPLING_RATE  100 // 1kHz
#define TOP_SAMPLING_RATE     1000 // 1.5kHz

#define FS2BAND(x)            ((x)*FFT_LEN/SAMPLING_RATE)
#define BOTTOM_BAND           (FS2BAND(BOTTOM_SAMPLING_RATE))
#define TOP_BAND              (FS2BAND(TOP_SAMPLING_RATE))

#define MS2FRAME(x)           (((x)*SAMPLING_RATE/1000/(FFT_LEN-OVERLAP))+1)
#define LENGTH_FRAME          MS2FRAME(LENGTH_THRESHOLD)
#define INTERVAL_FRAME        MS2FRAME(INTERVAL_THRESHOLD)

/*-----------------------------------------------------------------*/
/* Allocate the larger heap size than default */

USER_HEAP_SIZE(64 * 1024);

/* MultiCore definitions */

struct Request {
  void *buffer;
  int  sample;
  int  chnum;
};

struct Result {
  Result(){
    clear();
  }

  float power[MAX_CHANNEL_NUM];
  int  channel;

  void clear(){
    for(int i=0;i<MAX_CHANNEL_NUM;i++){
      power[i]=0;
    }
  }
};

void setup()
{
  /* Initialize MP library */
  int ret = MP.begin();
  if (ret < 0) {
    errorLoop(2);
  }

  /* receive with non-blocking */
  MP.RecvTimeout(MP_RECV_POLLING);

  FFT.begin();
}

#define RESULT_SIZE 4
void loop()
{
  int      ret;
  int8_t   sndid = 10; /* user-defined msgid */
  int8_t   rcvid;
  Request *request;
  static Result result[RESULT_SIZE];
  static int pos=0;

  result[pos].clear();

  static float pDst[FFT_LEN/2];

  /* Receive PCM captured buffer from MainCore */
  ret = MP.Recv(&rcvid, &request);
  if (ret >= 0) {
      FFT.put((q15_t*)request->buffer,request->sample);
  }

  while(!FFT.empty(0)){
      result[pos].channel = MAX_CHANNEL_NUM;
    for (int i = 0; i < MAX_CHANNEL_NUM; i++) {
      FFT.get(pDst,i);
      result[pos].power[i] = detect_sound(BOTTOM_BAND,TOP_BAND,pDst,i);
    }
    ret = MP.Send(sndid, &result[pos],0);
    pos = (pos+1)%RESULT_SIZE;
    if (ret < 0) {
      errorLoop(1);
    }
  }

}


/*-----------------------------------------------------------------*/
/*
 * Detector functions
 */
struct Sounds {
  Sounds(){
    clear();
  }

  int continuity[MAX_CHANNEL_NUM];
  int interval[MAX_CHANNEL_NUM];

  void clear(){
    for(int i=0;i<MAX_CHANNEL_NUM;i++){
      continuity[i]=0;
      interval[i]=0;
    }
  }
};

float detect_sound(int bottom, int top, float* pdata, int channel )
{
  static Sounds sounds;
  float Power=0;
  if(bottom > top) return Power;

  if(sounds.interval[channel]> 0){ /* Do not detect in interval time.*/
    sounds.interval[channel]--;
    sounds.continuity[channel]=0;
    return Power;
  }

  for(int i=bottom;i<=top;i++){
    if(*(pdata+i) > POWER_THRESHOLD){ // find sound.
      sounds.continuity[channel]++;
      Power+=*(pdata+i);
    }
  }

  sounds.continuity[channel]=0;
  if(Power>30){
    sounds.interval[channel] = INTERVAL_FRAME;
  }
  return Power;
}


void errorLoop(int num)
{
  int i;

  while (1) {
    for (i = 0; i < num; i++) {
      ledOn(LED0);
      delay(300);
      ledOff(LED0);
      delay(300);
    }
    delay(1000);
  }
}
