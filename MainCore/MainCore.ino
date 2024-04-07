/*
 * SPDX-License-Identifier: (Apache-2.0 OR LGPL-2.1-or-later)
 *
 * Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef ARDUINO_ARCH_SPRESENSE
#error "Board selection is wrong!!"
#endif
#ifdef SUBCORE
#error "Core selection is wrong!!"
#endif

#include <SDSink.h>
// #include <YuruhornSrc.h>
#include "BokoboxSrc.h"


// this file names are deifned middle C (60) as C4
const SDSink::Item table[2] = {
    {60, "AUDIO/drum1.wav"}, {61, "AUDIO/drum2.wav"},  //< C4, C#3
};

SDSink sink(table, 2);
BokoboxSrc inst(sink);

void setup() {
    // init built-in I/O
    Serial.begin(115200);
    pinMode(LED0, OUTPUT);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);

    // setup instrument
    if (!inst.begin()) {
        Serial.println("ERROR: init error.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Ready to play Bokobox");
}

void loop() {
    // run instrument
    inst.update();
}
