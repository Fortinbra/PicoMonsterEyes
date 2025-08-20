#pragma once

#include <cstddef>
#include <cstdint>

namespace eyes {

// Abstract audio output (PCM 16-bit mono)
class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual bool init(uint32_t sample_rate_hz) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual size_t write_samples(const int16_t* samples, size_t count) = 0; // returns frames written
};

} // namespace eyes
