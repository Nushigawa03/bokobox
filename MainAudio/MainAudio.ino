/*
 *  MainAudio.ino - FFT Example with Audio (voice changer)
 *  Copyright 2019, 2021 Sony Semiconductor Solutions Corporation
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

#include <string.h>

#include <SDHCI.h>
#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <FrontEnd.h>
#include <MemoryUtil.h>
#include <arch/board/board.h>

SDClass theSD;

MediaPlayer *thePlayer;
OutputMixer *theMixer;
FrontEnd *theFrontEnd;

File myFile;

const char* FileList[] = {"AUDIO/drum1.wav", "AUDIO/drum2.wav"};

/* Setting audio parameters */
static const int32_t channel_num  = 2;
static const int32_t bit_length   = AS_BITLENGTH_16;
static const int32_t frame_sample = 256;
static const int32_t capture_size = frame_sample * (bit_length / 8) * channel_num;
static const int32_t render_size  = frame_sample * (bit_length / 8) * 2;

/* Multi-core parameters */
const int subcore = 1;

/* Application flags */
bool isCaptured = false;
bool isEnd = false;
bool ErrEnd = false;

/**
 * @brief Frontend attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */

static void frontend_attention_cb(const ErrorAttentionParam *param)
{
  puts("Attention!");

  if (param->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

/**
 * @brief OutputMixer attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */
static void mixer_attention_cb(const ErrorAttentionParam *param)
{
  puts("Attention!");

  if (param->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

/**
 * @brief MediaPlayer attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */
static void mediaplayer_attention_cb(const ErrorAttentionParam *param)
{
  puts("Attention!");

  if (param->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

/**
 * @brief Frontend done callback procedure
 *
 * @param [in] event        AsMicFrontendEvent type indicator
 * @param [in] result       Result
 * @param [in] sub_result   Sub result
 *
 * @return true on success, false otherwise
 */

static bool frontend_done_callback(AsMicFrontendEvent ev, uint32_t result, uint32_t sub_result)
{
  UNUSED(ev);
  UNUSED(result);
  UNUSED(sub_result);
  return true;
}

/**
 * @brief Mixer done callback procedure
 *
 * @param [in] requester_dtq    MsgQueId type
 * @param [in] reply_of         MsgType type
 * @param [in,out] done_param   AsOutputMixDoneParam type pointer
 */
static void outputmixer_done_callback(MsgQueId requester_dtq,
                                      MsgType reply_of,
                                      AsOutputMixDoneParam* done_param)
{
  UNUSED(requester_dtq);
  UNUSED(reply_of);
  UNUSED(done_param);
  return;
}

/**
 * @brief Player done callback procedure
 *
 * @param [in] event        AsPlayerEvent type indicator
 * @param [in] result       Result
 * @param [in] sub_result   Sub result
 *
 * @return true on success, false otherwise
 */
static bool mediaplayer_done_callback(AsPlayerEvent event, uint32_t result, uint32_t sub_result)
{
  printf("mp cb %x %lx %lx\n", event, result, sub_result);

  return true;
}

/**
 * @brief Player decode callback procedure
 *
 * @param [in] pcm_param    AsPcmDataParam type
 */
void mediaplayer_decode_callback(AsPcmDataParam pcm_param)
{
  {
    /* You can process a data here. */
    
    signed short *ptr = (signed short *)pcm_param.mh.getPa();

    for (unsigned int cnt = 0; cnt < pcm_param.size; cnt += 4)
      {
        *ptr = *ptr + 0;
        ptr++;
        *ptr = *ptr + 0;
        ptr++;
      }
  }
  
  theMixer->sendData(OutputMixer0,
                     outmixer_send_callback,
                     pcm_param);
}

struct Request {
  void *buffer;
  int  sample;
  int  channel;
};

struct Result {
  float power[channel_num];
  int  channel;
};

/**
 * @brief Pcm capture on FrontEnd callback procedure
 *
 * @param [in] pcm          PCM data structure
 */
static void frontend_pcm_callback(AsPcmDataParam pcm)
{
  int8_t sndid = 100; /* user-defined msgid */
  static Request request;

  if (pcm.size > capture_size) {
    puts("Capture size is too big!");
    pcm.size = capture_size;
  }

  request.buffer  = pcm.mh.getPa();
  request.sample  = pcm.size / (bit_length / 8) / channel_num;
  request.channel = channel_num;

  if (!pcm.is_valid) {
    puts("Invalid data !");
    memset(request.buffer , 0, request.sample);
  }

  MP.Send(sndid, &request, subcore);

  if (pcm.is_end) {
    isEnd = true;
  }

  return;
}

/**
 * @brief Mixer data send callback procedure
 *
 * @param [in] identifier   Device identifier
 * @param [in] is_end       For normal request give false, for stop request give true
 */
static void outmixer_send_callback(int32_t identifier, bool is_end)
{
  AsRequestNextParam next;

  next.type = (!is_end) ? AsNextNormalRequest : AsNextStopResRequest;

  AS_RequestNextPlayerProcess(AS_PLAYER_ID_0, &next);

  return;
}

/**
 * @brief Setup Audio & MP objects
 */
void setup()
{
  /* Initialize serial */
  Serial.begin(115200);
  while (!Serial);

  /* Launch SubCore */
  int ret = MP.begin(subcore);
  if (ret < 0) {
    printf("MP.begin error = %d\n", ret);
  }

  /* receive with non-blocking */
  //  MP.RecvTimeout(20);

  /* Initialize memory pools and message libs */
  Serial.println("Init memory Library");

  initMemoryPools();
  createStaticPools(MEM_LAYOUT_RECORDINGPLAYER);

  /* Begin objects */
  theFrontEnd = FrontEnd::getInstance();
  theMixer = OutputMixer::getInstance();
  thePlayer = MediaPlayer::getInstance();

  theFrontEnd->begin(frontend_attention_cb);
  theMixer->begin();
  thePlayer->begin();

  puts("begin FrontEnd and OutputMixer");

  /* Create Objects */
  theMixer->create(mixer_attention_cb);

  thePlayer->create(MediaPlayer::Player0, mediaplayer_attention_cb);

  /* Set capture clock */
  theFrontEnd->setCapturingClkMode(FRONTEND_CAPCLK_NORMAL);

  /* Activate objects */
  theFrontEnd->activate(frontend_done_callback);
  theMixer->activate(OutputMixer0, HPOutputDevice, outputmixer_done_callback);
  thePlayer->activate(MediaPlayer::Player0, mediaplayer_done_callback);

  usleep(100 * 1000); /* waiting for Mic startup */

  thePlayer->init(MediaPlayer::Player0, AS_CODECTYPE_WAV, "/mnt/sd0/BIN", AS_SAMPLINGRATE_44100, AS_CHANNEL_STEREO);

  /* Initialize each objects */
  AsDataDest dst;
  dst.cb = frontend_pcm_callback;
  theFrontEnd->init(channel_num, bit_length, frame_sample, AsDataPathCallback, dst);

  /* Initialize SD */
  while (!theSD.begin())
    {
      /* wait until SD card is mounted. */
      Serial.println("Insert SD card.");
    }

  /* Set rendering volume */
  theMixer->setVolume(-6, 0, 0);

  /* Unmute */
  board_external_amp_mute_control(false);

  theFrontEnd->start();
}

static bool start(char str[])
{
  err_t err;

  theFrontEnd->stop();

  myFile = theSD.open(str);

  if (!myFile){
    printf("File open error\n");
    return false;
  }

  /* Send first frames to be decoded */

  err = thePlayer->writeFrames(MediaPlayer::Player0, myFile);

  if ((err != MEDIAPLAYER_ECODE_OK) && (err != MEDIAPLAYER_ECODE_FILEEND)) {
    printf("File Read Error! =%d\n",err);
    return false;
  }

  printf("start\n");
  thePlayer->start(MediaPlayer::Player0, mediaplayer_decode_callback);

  theFrontEnd->start();

  return true;
}
static void stop()
{
  printf("stop\n");
  thePlayer->stop(MediaPlayer::Player0, AS_STOPPLAYER_NORMAL);
  myFile.close();
}

/**
 * @brief Audio loop
 */
void loop()
{
  static enum State {
    Stopped,
    Ready,
    Active
  } s_state = Stopped;
  static int fileIndex = 0;

  /* Send new frames to decode in a loop until file ends */
  err_t err = thePlayer->writeFrames(MediaPlayer::Player0, myFile);
  
  int8_t   rcvid = 0;
  static Result*  result;

  /* Receive sound from SubCore */
  int ret = MP.Recv(&rcvid, &result, subcore);

  if (ret < 0) {
    printf("MP error! %d\n", ret);
    return;
  }

  float max_power=0;
  int c=0;
  for (int i=0;i<channel_num;i++) {
    float power = result->power[i];
    if(max_power<power){
      max_power=power;
      c=i;
    }
  }
  if(max_power>0){
    if(max_power>100){
      if (s_state == Stopped) {
        s_state = Ready;
        fileIndex = 0;
      }
      printf("%d:A %f\n", c, max_power);
    }else{
      if (s_state == Stopped) {
        s_state = Ready;
        fileIndex = 1;
      }
      printf("%d:B %f\n", c, max_power);
    }
  }

  switch (s_state) {
  case Stopped:
    break;

  case Ready:
    if (start(FileList[fileIndex])) {
      s_state = Active;
    } else {
      goto stop_player;
    }
    break;

  case Active:
    err = thePlayer->writeFrames(MediaPlayer::Player0, myFile);
    if (err == MEDIAPLAYER_ECODE_FILEEND) {
      stop();
      s_state = Stopped;
    } else if (err != MEDIAPLAYER_ECODE_OK) {
      printf("Main player error code: %d\n", err);
      goto stop_player;
    }
    break;

  default:
    break;
  }

  return;

stop_player:
  thePlayer->stop(MediaPlayer::Player0);
  myFile.close();
  thePlayer->deactivate(MediaPlayer::Player0);
  thePlayer->end();
  exit(1);
}
