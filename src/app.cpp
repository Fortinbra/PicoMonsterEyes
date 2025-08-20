#include "app.hpp"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "boards/pico2_pins.hpp"
#include "drivers/spi_bus.hpp"
#include "drivers/ssd1351_display.hpp"

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

    // Simple test pattern
    left.fill(0xF800);  // Red
    right.fill(0x001F); // Blue

    return true;
}

void App::loop() {
    // Idle; nothing dynamic yet
    while (true) { tight_loop_contents(); }
}

} // namespace eyes
