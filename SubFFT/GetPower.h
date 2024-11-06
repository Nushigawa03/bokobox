#ifndef _GETPOWER_H_
#define _GETPOWER_H_

/* Use CMSIS library */
#define ARM_MATH_CM4
#define __FPU_PRESENT 1U
#include <cmsis/arm_math.h>
#include <math.h>

#include "RingBuff.h"

/*------------------------------------------------------------------*/
/* Type Definition                                                  */
/*------------------------------------------------------------------*/
/* WINDOW TYPE */
typedef enum e_windowType {
  WindowHamming,
  WindowHanning,
  WindowFlattop,
  WindowRectangle
} windowType_t;

/*------------------------------------------------------------------*/
/* Input buffer                                                      */
/*------------------------------------------------------------------*/
template <int MAX_CHNUM, int SIGNAL_LEN> class GetPowerClass
{
public:
  void begin(){
      begin(WindowHamming, MAX_CHNUM, (SIGNAL_LEN / 2));
  }

  bool begin(windowType_t type, int channel, int overlap){
    if (channel > MAX_CHNUM) return false;
    if (overlap > (SIGNAL_LEN / 2)) return false;

    m_overlap = overlap;
    m_channel = channel;

    clear();
    create_coef(type);

    for(int i = 0; i < MAX_CHNUM; i++) {
      ringbufs[i] = new RingBuff(MAX_CHNUM * SIGNAL_LEN * sizeof(q15_t));
    }

    return true;
  }

  bool put(q15_t* pSrc, int sample) {
    /* Ringbuf size check */
    if(m_channel > MAX_CHNUM) return false;
    if(sample > ringbufs[0]->remain()) return false;

    if (m_channel == 1) {
      /* the faster optimization */
      ringbufs[0]->put((q15_t*)pSrc, sample);
    } else {
      for (int i = 0; i < m_channel; i++) {
        ringbufs[i]->put(pSrc, sample, m_channel, i);
      }
    }
    return  true;
  }

  float get(int channel) {
    return _get_power(channel);
  }

  void clear() {
    for (int i = 0; i < MAX_CHNUM; i++) {
      memset(tmpInBuf[i], 0, SIGNAL_LEN);
    }
  }

  void end(){}


  bool empty(int channel){
    return (ringbufs[channel]->stored() < SIGNAL_LEN);
  }

private:

  RingBuff* ringbufs[MAX_CHNUM];

  int m_channel;
  int m_overlap;
  arm_rfft_fast_instance_f32 S;

  /* Temporary buffer */
  float tmpInBuf[MAX_CHNUM][SIGNAL_LEN];
  float coef[SIGNAL_LEN];

  void create_coef(windowType_t type) {
    for (int i = 0; i < SIGNAL_LEN / 2; i++) {
      if (type == WindowHamming) {
        coef[i] = 0.54f - (0.46f * arm_cos_f32(2 * PI * (float)i / (SIGNAL_LEN - 1)));
      } else if (type == WindowHanning) {
        coef[i] = 0.54f - (1.0f * arm_cos_f32(2 * PI * (float)i / (SIGNAL_LEN - 1)));
      } else if (type == WindowFlattop) {
        coef[i] = 0.21557895f - (0.41663158f  * arm_cos_f32(2 * PI * (float)i / (SIGNAL_LEN - 1)))
                              + (0.277263158f * arm_cos_f32(4 * PI * (float)i / (SIGNAL_LEN - 1)))
                              - (0.083578947f * arm_cos_f32(6 * PI * (float)i / (SIGNAL_LEN - 1)))
                              + (0.006947368f * arm_cos_f32(8 * PI * (float)i / (SIGNAL_LEN - 1)));
      } else {
        coef[i] = 1;
      }
      coef[SIGNAL_LEN -1 - i] = coef[i];
    }
  }

  float _get_power(int channel) {
    if(channel >= m_channel) return false;
    if (ringbufs[channel]->stored() < SIGNAL_LEN) return 0;

    /* Read from the ring buffer */
    ringbufs[channel]->get(&tmpInBuf[channel][m_overlap], SIGNAL_LEN);

    float out = 0;
    for (int i = 0; i < SIGNAL_LEN; i++) {
      out += tmpInBuf[channel][i]*tmpInBuf[channel][i];
    }

    return out;
  }


};

#endif /*_GETPOWER_H_*/
