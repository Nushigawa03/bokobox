#include <MP.h>

#include "GetPower.h"

/*-----------------------------------------------------------------*/
/*
 * FFT parameters
 */
/* Select FFT length */

/* Number of channels*/
//#define MAX_CHANNEL_NUM 1
//#define MAX_CHANNEL_NUM 2
#define MAX_CHANNEL_NUM 4

#define SAMPLING_RATE   48000 // ex.) 48000, 16000

#define SIGNAL_LEN          256 // ex.) 128, 256, 1024
#define OVERLAP             0  // ex.) 0, 128, 256

GetPowerClass<MAX_CHANNEL_NUM, SIGNAL_LEN > GetPower;

/*-----------------------------------------------------------------*/
/*
 * Detector parameters
 */
#define POWER_THRESHOLD       3  // Power
#define INTERVAL_THRESHOLD    100 // 100ms

#define MS2FRAME(x)           (((x)*SAMPLING_RATE/1000/(SIGNAL_LEN -OVERLAP))+1)
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

  GetPower.begin();
}

#define RESULT_SIZE 4
struct Sounds {
  Sounds(){
    clear();
  }

  int interval;
  bool get;

  void clear(){
    interval=0;
    get=false;
  }
};
static Sounds sounds;
float powers[MAX_CHANNEL_NUM] = {0, 0, 0, 0};
void loop()
{
  int      ret;
  int8_t   sndid = 10; /* user-defined msgid */
  int8_t   rcvid;
  Request *request;
  static Result result[RESULT_SIZE];
  static int pos=0;

  result[pos].clear();

  static float pDst[SIGNAL_LEN /2];

  /* Receive PCM captured buffer from MainCore */
  ret = MP.Recv(&rcvid, &request);
  if (ret >= 0) {
      GetPower.put((q15_t*)request->buffer,request->sample);
  }

  while(!GetPower.empty(0)){
      result[pos].channel = MAX_CHANNEL_NUM;
      get_power();
    for (int i = 0; i < MAX_CHANNEL_NUM; i++) {
      if (sounds.get) {
        result[pos].power[i] = powers[i];
      } else {
        result[pos].power[i] = 0;
      }
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

void get_power()
{
  for (int i = 0; i < MAX_CHANNEL_NUM; i++) {
    powers[i] = GetPower.get(i);
  }
  if(sounds.interval> 0){ /* Do not detect in interval time.*/
    sounds.interval--;
    sounds.get = false;
  } else {
    for (int i = 0; i < MAX_CHANNEL_NUM; i++) {
      if (powers[i] > POWER_THRESHOLD) {
        sounds.get = true;
        sounds.interval = INTERVAL_FRAME;
        break;
      }
    }
  }
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
