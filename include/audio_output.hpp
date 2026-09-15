#pragma once

#include <cstddef>
#include <cstdint>

namespace eyes {

// Abstract audio output. PCM data is 16-bit stereo, interleaved as
// [left0, right0, left1, right1, ...]; one frame == one left + one right sample.
class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual bool init(uint32_t sample_rate_hz) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;

    // Copies up to frame_count interleaved stereo frames into the output ring buffer.
    // Returns the number of frames actually accepted (less than requested if the buffer is full).
    virtual size_t write_frames(const int16_t* interleaved_lr, size_t frame_count) = 0;

    // Free space in the output ring buffer, in frames.
    virtual size_t available_frames() const = 0;

    // Count of playback gaps where no producer data was available in time.
    virtual uint32_t underrun_count() const = 0;
};

} // namespace eyes
