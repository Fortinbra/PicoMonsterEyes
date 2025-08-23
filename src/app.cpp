#include "app.hpp"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "boards/pico2_pins.hpp"
#include "drivers/spi_bus.hpp"
#include "drivers/ssd1351_display.hpp"
#include "default_eye.hpp" // sclera asset (partial)

namespace eyes {

bool App::init() {
    // SPI pins
    gpio_set_function(pins::spi0_sck,  GPIO_FUNC_SPI);
    gpio_set_function(pins::spi0_mosi, GPIO_FUNC_SPI);
    if (pins::spi0_miso != 0xFF) {
        gpio_set_function(pins::spi0_miso, GPIO_FUNC_SPI);
    }

    static SpiBus spi(spi0, 16 * 1000 * 1000); // 16 MHz to start
    spi.init();

    // Two displays sharing the SPI bus
    static Ssd1351Display left(spi, 128, 128, pins::left_cs, pins::left_dc, pins::left_res);
    static Ssd1351Display right(spi, 128, 128, pins::right_cs, pins::right_dc, pins::right_res);

    bool ok = left.init() && right.init();
    if (!ok) return false;

    // Allocate a static framebuffer (RGB565)
    static uint16_t frame[128 * 128];
    Rect full{0,0,128,128};

    // Simple static eye test: center-crop 128x128 from 200x200 sclera asset
    constexpr int SRC_W = PME_SCLERA_WIDTH;
    constexpr int SRC_H = PME_SCLERA_HEIGHT;
    const int x0 = (SRC_W - 128) / 2;
    const int y0 = (SRC_H - 128) / 2;
    for (int y = 0; y < 128; ++y) {
        const uint16_t* row = &pme_sclera[y0 + y][x0];
        for (int x = 0; x < 128; ++x) {
            frame[y*128 + x] = row[x];
        }
    }
    left.blit(frame, full);
    right.blit(frame, full);

    // NOTE: Currently only sclera background. Iris/eyelids will be layered later.

    return true;
}

void App::loop() {
    // Idle; nothing dynamic yet
    while (true) { tight_loop_contents(); }
}

} // namespace eyes
