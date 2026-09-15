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

// Audio (MAX98357A / dual MAX98357A stereo breakout)
// LRCLK must immediately follow BCLK: the audio_i2s PIO program side-sets both
// from a single consecutive GPIO pair (BCLK = base, LRCLK = base + 1).
constexpr uint8_t i2s_bclk  = 10;
constexpr uint8_t i2s_lrclk = 11;
constexpr uint8_t i2s_din   = 12;

namespace detail {
constexpr uint8_t kAllPins[] = {
    spi0_sck, spi0_mosi,
    left_cs, left_dc, left_res,
    right_cs, right_dc, right_res,
    i2s_bclk, i2s_lrclk, i2s_din,
};
constexpr bool no_duplicate_pins() {
    constexpr size_t n = sizeof(kAllPins) / sizeof(kAllPins[0]);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (kAllPins[i] == kAllPins[j]) return false;
        }
    }
    return true;
}
static_assert(no_duplicate_pins(), "Duplicate GPIO assignment detected in pico2_pins.hpp");
static_assert(i2s_lrclk == i2s_bclk + 1, "i2s_lrclk must immediately follow i2s_bclk for the audio_i2s PIO program");
} // namespace detail

} // namespace eyes::pins
