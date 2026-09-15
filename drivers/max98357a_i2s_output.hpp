#pragma once

#include <cstdint>
#include <cstddef>
#include "hardware/pio.h"
#include "audio_output.hpp"

namespace eyes {

// PIO + DMA driven stereo I2S transmitter for a MAX98357A (or dual MAX98357A
// stereo breakout). BCLK and LRCLK must be on consecutive GPIOs (LRCLK = BCLK + 1).
class Max98357aI2sOutput : public AudioOutput {
public:
    Max98357aI2sOutput(uint8_t pin_bclk, uint8_t pin_lrclk, uint8_t pin_din)
        : bclk_(pin_bclk), lrclk_(pin_lrclk), din_(pin_din) {}

    bool init(uint32_t sample_rate_hz) override;
    bool start() override;
    void stop() override;
    size_t write_frames(const int16_t* interleaved_lr, size_t frame_count) override;
    size_t available_frames() const override;
    uint32_t underrun_count() const override { return underrun_count_; }

private:
    static constexpr size_t kRingFrames = 2048; // ~93ms of buffering at 22.05kHz
    static constexpr size_t kSilenceFrames = 256;

    uint8_t bclk_;
    uint8_t lrclk_;
    uint8_t din_;

    PIO pio_ = nullptr;
    uint sm_ = 0;
    uint pio_offset_ = 0;
    int dma_chan_ = -1;
    bool started_ = false;

    // Ring buffer of packed stereo frames: high 16 bits = left, low 16 bits = right.
    static int32_t ring_[kRingFrames];
    static const int32_t silence_[kSilenceFrames];

    // Monotonic frame counters; only the ring index derived via modulo kRingFrames wraps.
    volatile uint32_t frames_written_ = 0;
    volatile uint32_t frames_completed_ = 0;
    volatile uint32_t chunk_len_in_flight_ = 0;
    volatile bool chunk_is_silence_ = false;
    volatile uint32_t underrun_count_ = 0;

    void start_next_dma_chunk();
    void on_dma_complete();
    static void dma_irq_handler();

    static Max98357aI2sOutput* s_instance_;
};

} // namespace eyes
