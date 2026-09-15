#include <stdio.h>
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "boards/pico2_pins.hpp"
#include "drivers/max98357a_i2s_output.hpp"
#include "include/bluetooth_audio_source.hpp"
#include "src/app.hpp"

namespace {
eyes::App app;

void run_eyes() {
    app.loop();
}
}

int main() {
    stdio_init_all();

    if (!app.init()) {
        while (true) { sleep_ms(1000); }
    }

    multicore_launch_core1(run_eyes);

    eyes::Max98357aI2sOutput audio(eyes::pins::i2s_bclk, eyes::pins::i2s_lrclk, eyes::pins::i2s_din);
    eyes::BluetoothAudioSource bluetooth(audio);
    if (!bluetooth.init()) {
        while (true) { sleep_ms(1000); }
    }
    bluetooth.run();
}
