#pragma once

#include <cstdint>

namespace eyes::pins {

// SPI0 shared
constexpr uint8_t spi0_sck = 18;
constexpr uint8_t spi0_mosi = 19;
constexpr uint8_t spi0_miso = 0xFF; // optional (set to 0xFF to skip)

// Left display
constexpr uint8_t left_cs  = 17; // example
constexpr uint8_t left_dc  = 20; // example
constexpr uint8_t left_res = 21; // example

// Right display
constexpr uint8_t right_cs  = 22; // example
constexpr uint8_t right_dc  = 26; // example
constexpr uint8_t right_res = 27; // example

// Audio (MAX98357A)
constexpr uint8_t i2s_bclk  = 10;
constexpr uint8_t i2s_lrclk = 11;
constexpr uint8_t i2s_din   = 12;

} // namespace eyes::pins
