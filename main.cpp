#include <stdio.h>
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

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
    while (!stdio_usb_connected()) { sleep_ms(100); } // block until a host terminal opens the USB CDC port
    printf("PicoMonsterEyes booting...\n");

    if (!app.init()) {
        // Silent hang otherwise looks identical to a dead board; UART tells us it's a display/SPI init failure.
        printf("App init FAILED (display/SPI init) - halting\n");
        while (true) { sleep_ms(1000); }
    }
    printf("App init OK\n");

    multicore_launch_core1(run_eyes);

    eyes::Max98357aI2sOutput audio(eyes::pins::i2s_bclk, eyes::pins::i2s_lrclk, eyes::pins::i2s_din);
    eyes::BluetoothAudioSource bluetooth(audio);
    if (!bluetooth.init()) {
        printf("Bluetooth init FAILED (cyw43/btstack) - halting\n");
        while (true) { sleep_ms(1000); }
    }
    printf("Bluetooth init OK, running\n");
    bluetooth.run();
}
