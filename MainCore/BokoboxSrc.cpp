/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#if defined(ARDUINO_ARCH_SPRESENSE) && !defined(SUBCORE)

#include "BokoboxSrc.h"

#include <Arduino.h>

#include <MP.h>
#include <OutputMixer.h>

// #define DEBUG (1)

// clang-format off
#define nop(...) do {} while (0)
// clang-format on
#ifdef DEBUG
#define trace_printf nop
#define debug_printf printf
#define error_printf printf
#else  // DEBUG
#define trace_printf nop
#define debug_printf nop
#define error_printf printf
#endif  // DEBUG

static const char kClassName[] = "BokoboxSrc";

#define UP_THRSH_PERCENT (80)      //< 80%
#define DOWN_THRSH_PERCENT (20)    //< 20%
#define NORMAL_THRSH_PERCENT (50)  //< 50%

struct FreqNotePair {
    unsigned int freq;
    int note;
};

static const struct FreqNotePair g_freq2note[] = {                                           //< index = note + 1
    {77, -1},                                                                                //< sentinel
    {82, 0},       {87, 1},       {92, 2},      {97, 3},      {103, 4},      {109, 5},       //< C-1 to B-1
    {116, 6},      {122, 7},      {130, 8},     {138, 9},     {146, 10},     {154, 11},      //<
    {164, 12},     {173, 13},     {184, 14},    {194, 15},    {206, 16},     {218, 17},      //< C0 to B0
    {231, 18},     {245, 19},     {260, 20},    {275, 21},    {291, 22},     {309, 23},      //<
    {327, 24},     {346, 25},     {367, 26},    {389, 27},    {412, 28},     {437, 29},      //< C1 to B1
    {462, 30},     {490, 31},     {519, 32},    {550, 33},    {583, 34},     {617, 35},      //<
    {654, 36},     {693, 37},     {734, 38},    {778, 39},    {824, 40},     {873, 41},      //< C2 to B2
    {925, 42},     {980, 43},     {1038, 44},   {1100, 45},   {1165, 46},    {1235, 47},     //<
    {1308, 48},    {1386, 49},    {1468, 50},   {1556, 51},   {1648, 52},    {1746, 53},     //< C3 to B3
    {1850, 54},    {1960, 55},    {2077, 56},   {2200, 57},   {2331, 58},    {2469, 59},     //<
    {2616, 60},    {2772, 61},    {2937, 62},   {3111, 63},   {3296, 64},    {3492, 65},     //< C4 to B4
    {3700, 66},    {3920, 67},    {4153, 68},   {4400, 69},   {4662, 70},    {4939, 71},     //<
    {5233, 72},    {5544, 73},    {5873, 74},   {6223, 75},   {6593, 76},    {6985, 77},     //< C5 to B5
    {7400, 78},    {7840, 79},    {8306, 80},   {8800, 81},   {9323, 82},    {9878, 83},     //<
    {10465, 84},   {11087, 85},   {11747, 86},  {12445, 87},  {13185, 88},   {13969, 89},    //< C6 to B6
    {14800, 90},   {15680, 91},   {16612, 92},  {17600, 93},  {18647, 94},   {19755, 95},    //<
    {20930, 96},   {22175, 97},   {23493, 98},  {24890, 99},  {26370, 100},  {27938, 101},   //< C7 to B7
    {29600, 102},  {31360, 103},  {33224, 104}, {35200, 105}, {37293, 106},  {39511, 107},   //<
    {41860, 108},  {44349, 109},  {46986, 110}, {49780, 111}, {52740, 112},  {55877, 113},   //< C8 to B8
    {59199, 114},  {62719, 115},  {66449, 116}, {70400, 117}, {74586, 118},  {79021, 119},   //<
    {83720, 120},  {88698, 121},  {93973, 122}, {99561, 123}, {105481, 124}, {111753, 125},  //< C9 to G9
    {118398, 126}, {125439, 127},                                                            //<
    {132898, 128}};                                                                          //< sentinel

static const int kDefaultActiveThresh = 300;
static const int kDefaultCorrectTime = 5;
static const int kDefaultSuppressTime = 3;
static const int kDefaultKeepTime = 3;

BokoboxSrc::BokoboxSrc(Filter& filter) : BokoboxSrc(PIN_NOT_ASSIGNED, PIN_NOT_ASSIGNED, filter) {
}

BokoboxSrc::BokoboxSrc(uint8_t perform_pin, uint8_t volume_pin, Filter& filter)
    : SoundCapture(filter),
      active_level_(kDefaultActiveThresh),
      is_button_enable_(perform_pin != PIN_NOT_ASSIGNED ? true : false),
      perform_note_(INVALID_NOTE_NUMBER),
      extend_frames_(kDefaultCorrectTime),
      suppress_frames_(kDefaultSuppressTime),
      keep_frames_(kDefaultKeepTime),
      invalid_counter_(0),
      active_counter_(0),
      silent_counter_(0),
      scale_mask_(SCALE_ALL),
      max_note_(NOTE_NUMBER_MAX),
      min_note_(NOTE_NUMBER_MIN),
      level_meter_(0),
      is_monitor_enable_(false),
      monitor_volume_(0),
      perform_pin_(perform_pin),
      volume_pin_(volume_pin) {
}

bool BokoboxSrc::isAvailable(int param_id) {
    if (param_id == BokoboxSrc::PARAMID_ACTIVE_LEVEL) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_PLAY_BUTTON_ENABLE) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_MAX_NOTE) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_MIN_NOTE) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_SCALE) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_CORRECT_FRAMES) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_SUPPRESS_FRAMES) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_KEEP_FRAMES) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_VOLUME_METER) {
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_MONITOR_ENABLE) {
        return true;
    } else {
        return SoundCapture::isAvailable(param_id);
    }
}

intptr_t BokoboxSrc::getParam(int param_id) {
    if (param_id == BokoboxSrc::PARAMID_ACTIVE_LEVEL) {
        return getActiveLevel();
    } else if (param_id == BokoboxSrc::PARAMID_PLAY_BUTTON_ENABLE) {
        return is_button_enable_;
    } else if (param_id == BokoboxSrc::PARAMID_MAX_NOTE) {
        return max_note_;
    } else if (param_id == BokoboxSrc::PARAMID_MIN_NOTE) {
        return min_note_;
    } else if (param_id == BokoboxSrc::PARAMID_SCALE) {
        return scale_mask_;
    } else if (param_id == BokoboxSrc::PARAMID_CORRECT_FRAMES) {
        return extend_frames_;
    } else if (param_id == BokoboxSrc::PARAMID_SUPPRESS_FRAMES) {
        return suppress_frames_;
    } else if (param_id == BokoboxSrc::PARAMID_KEEP_FRAMES) {
        return keep_frames_;
    } else if (param_id == BokoboxSrc::PARAMID_VOLUME_METER) {
        return level_meter_;
    } else if (param_id == BokoboxSrc::PARAMID_MONITOR_ENABLE) {
        return is_monitor_enable_;
    } else {
        return SoundCapture::getParam(param_id);
    }
}

bool BokoboxSrc::setParam(int param_id, intptr_t value) {
    if (param_id == BokoboxSrc::PARAMID_ACTIVE_LEVEL) {
        setActiveLevel(value);
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_PLAY_BUTTON_ENABLE) {
        is_button_enable_ = value;
        return true;
    } else if (param_id == BokoboxSrc::PARAMID_MAX_NOTE) {
        max_note_ = value;
    } else if (param_id == BokoboxSrc::PARAMID_MIN_NOTE) {
        min_note_ = value;
    } else if (param_id == BokoboxSrc::PARAMID_SCALE) {
        scale_mask_ = value;
    } else if (param_id == BokoboxSrc::PARAMID_CORRECT_FRAMES) {
        extend_frames_ = value;
    } else if (param_id == BokoboxSrc::PARAMID_SUPPRESS_FRAMES) {
        suppress_frames_ = value;
    } else if (param_id == BokoboxSrc::PARAMID_KEEP_FRAMES) {
        keep_frames_ = value;
    } else if (param_id == BokoboxSrc::PARAMID_MONITOR_ENABLE) {
        if (value) {
            printf("millis in_freq out_freq volume threshold\n");
        }
        is_monitor_enable_ = value;
    } else {
        return SoundCapture::setParam(param_id, value);
    }
    return true;
}

int BokoboxSrc::getActiveLevel() {
    return active_level_;
}

void BokoboxSrc::setActiveLevel(int active_level) {
    active_level_ = active_level;
    return;
}

void BokoboxSrc::onCapture(unsigned int freq_numer, unsigned int freq_denom, unsigned int volume, unsigned int volume2) {

    float max_power=volume;
    int c=0;
    if(max_power<volume2){
        max_power=volume2;
        c=1;
    }

    if(max_power>0){
        if(max_power>120){
            uint8_t vel = (uint8_t)max(127, 127 * (max_power - 80) / 120);
            sendNoteOn(60, DEFAULT_VELOCITY, DEFAULT_CHANNEL);
            printf("%d:A %d %d\n", c, volume, volume2);
        } else {
            uint8_t vel = (uint8_t)max(127, 127 * max_power / 120);
            sendNoteOn(61, DEFAULT_VELOCITY, DEFAULT_CHANNEL);
            printf("%d:A %d %d\n", c, volume, volume2);
        }
    }
    ledOff(LED2);
}

uint8_t BokoboxSrc::lookupNote(unsigned int freq) {
    uint8_t note = INVALID_NOTE_NUMBER;

    unsigned int thresh = 0;
    unsigned int current_freq = 0;
    if (perform_note_ != INVALID_NOTE_NUMBER) {
        current_freq = g_freq2note[perform_note_ + 1].freq;  // index = note + 1
    }

    const struct FreqNotePair* lo = nullptr;
    for (const auto& hi : g_freq2note) {
        if (lo != nullptr) {
            if ((scale_mask_ & (1U << (hi.note % PITCH_NUM))) == 0) {
                continue;
            }
            int ratio = NORMAL_THRSH_PERCENT;
            if (current_freq == 0) {
                ratio = NORMAL_THRSH_PERCENT;
            } else if (freq > current_freq) {
                ratio = UP_THRSH_PERCENT;
            } else {
                ratio = DOWN_THRSH_PERCENT;
            }
            thresh = lo->freq + (hi.freq - lo->freq) * ratio / 100;
            if (freq < thresh) {
                break;
            }
        }
        lo = &hi;
    }

    if (lo != nullptr && min_note_ <= lo->note && lo->note <= max_note_) {
        note = lo->note;
    }
    debug_printf("[%s::%s] thresh=%d, note=%d\n", kClassName, __func__, thresh, note);

    return note;
}

uint8_t BokoboxSrc::lingerNote(uint8_t note, unsigned int volume) {
    uint8_t ret = note;

    // if input volume is enough, keep making the sound
    if (note == INVALID_NOTE_NUMBER && volume >= (unsigned int)active_level_) {
        if (invalid_counter_ < extend_frames_) {
            invalid_counter_++;
            ret = perform_note_;
        }
    } else {
        invalid_counter_ = 0;
    }

    if (volume < (unsigned int)active_level_) {
        active_counter_ = 0;
        if (silent_counter_ < keep_frames_) {
            ret = perform_note_;
        }
        silent_counter_++;
    } else {
        if (active_counter_ < suppress_frames_) {
            ret = INVALID_NOTE_NUMBER;
        }
        active_counter_++;
        silent_counter_ = 0;
    }
    return ret;
}

#endif  // ARDUINO_ARCH_SPRESENSE