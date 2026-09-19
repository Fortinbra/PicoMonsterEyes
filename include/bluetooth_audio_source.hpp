#pragma once

#include <cstdint>

#include "btstack.h"

namespace eyes {

class AudioOutput;

class BluetoothAudioSource {
public:
    explicit BluetoothAudioSource(AudioOutput& output);

    bool init();
    [[noreturn]] void run();

    uint32_t dropped_frames() const { return dropped_frames_; }
    uint32_t sample_rate_hz() const { return sample_rate_hz_; }

private:
    // Upper bound on frames requested per refill(); must cover the worst-case elapsed
    // time between timer callbacks (observed run-loop jitter can be several x
    // kRefillIntervalMs), or excess demand silently gets clamped away below.
    static constexpr uint16_t kPcmBlockFrames = 1024;
    static constexpr uint32_t kRefillIntervalMs = 5;

    AudioOutput& output_;
    btstack_timer_source_t refill_timer_{};
    void (*playback_)(int16_t*, uint16_t, const btstack_audio_context_t*) = nullptr;
    uint32_t sample_rate_hz_ = 0;
    uint32_t dropped_frames_ = 0;
    uint32_t last_refill_time_ms_ = 0;
    uint8_t volume_ = 127;
    bool output_ready_ = false;
    bool streaming_ = false;
    // static: kPcmBlockFrames*2 int16s would otherwise blow the stack frame of whatever
    // scope constructs this object (e.g. main()'s locals)
    static int16_t pcm_[kPcmBlockFrames * 2];

    int configure(uint8_t channels, uint32_t sample_rate_hz,
                  void (*playback)(int16_t*, uint16_t, const btstack_audio_context_t*));
    void start_stream();
    void stop_stream();
    void close();
    void refill();

    static BluetoothAudioSource* instance_;
    static int sink_init(uint8_t channels, uint32_t sample_rate_hz,
                         void (*playback)(int16_t*, uint16_t, const btstack_audio_context_t*));
    static uint32_t sink_get_sample_rate();
    static void sink_set_volume(uint8_t volume);
    static void sink_start_stream();
    static void sink_stop_stream();
    static void sink_close();
    static void refill_timer_handler(btstack_timer_source_t* timer);
    static const btstack_audio_sink_t sink_;
};

} // namespace eyes