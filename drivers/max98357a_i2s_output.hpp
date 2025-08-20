#pragma once

#include <cstdint>
#include <cstddef>
#include "audio_output.hpp"

namespace eyes {

class Max98357aI2sOutput : public AudioOutput {
public:
    Max98357aI2sOutput(uint8_t pin_bclk, uint8_t pin_lrclk, uint8_t pin_din)
        : bclk_(pin_bclk), lrclk_(pin_lrclk), din_(pin_din) {}

    bool init(uint32_t sample_rate_hz) override;
    bool start() override;
    void stop() override;
    size_t write_samples(const int16_t* samples, size_t count) override;

private:
    uint8_t bclk_;
    uint8_t lrclk_;
    uint8_t din_;
};

} // namespace eyes
