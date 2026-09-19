#include "bluetooth_audio_source.hpp"

#include <cstdio>

#include "audio_output.hpp"
#include "pico/cyw43_arch.h"

extern "C" int btstack_main(int argc, const char* argv[]);

namespace eyes {

BluetoothAudioSource* BluetoothAudioSource::instance_ = nullptr;
int16_t BluetoothAudioSource::pcm_[kPcmBlockFrames * 2];

const btstack_audio_sink_t BluetoothAudioSource::sink_ = {
    sink_init,
    sink_get_sample_rate,
    sink_set_volume,
    sink_start_stream,
    sink_stop_stream,
    sink_close,
};

BluetoothAudioSource::BluetoothAudioSource(AudioOutput& output) : output_(output) {}

bool BluetoothAudioSource::init() {
    instance_ = this;
    if (cyw43_arch_init() != PICO_OK) {
        instance_ = nullptr;
        return false;
    }

    btstack_audio_sink_set_instance(&sink_);
    btstack_main(0, nullptr);
    gap_set_local_name("Pico Monster Eyes");
    return true;
}

[[noreturn]] void BluetoothAudioSource::run() {
    btstack_run_loop_execute();
    while (true) {
        tight_loop_contents();
    }
}

int BluetoothAudioSource::configure(
    uint8_t channels, uint32_t sample_rate_hz,
    void (*playback)(int16_t*, uint16_t, const btstack_audio_context_t*)) {
    if (channels != 2 || playback == nullptr) {
        return 1;
    }

    playback_ = playback;
    sample_rate_hz_ = sample_rate_hz;
    output_ready_ = output_.init(sample_rate_hz_);
    printf("BluetoothAudioSource: configure channels=%u rate=%luHz output_ready=%d\n",
           channels, static_cast<unsigned long>(sample_rate_hz_), output_ready_ ? 1 : 0);
    return output_ready_ ? 0 : 1;
}

void BluetoothAudioSource::start_stream() {
    if (!output_ready_ || streaming_ || !output_.start()) {
        printf("BluetoothAudioSource: start_stream FAILED output_ready=%d streaming=%d\n",
               output_ready_ ? 1 : 0, streaming_ ? 1 : 0);
        return;
    }

    printf("BluetoothAudioSource: start_stream OK\n");
    streaming_ = true;
    last_refill_time_ms_ = btstack_run_loop_get_time_ms();
    refill();
    btstack_run_loop_set_timer_handler(&refill_timer_, refill_timer_handler);
    btstack_run_loop_set_timer(&refill_timer_, kRefillIntervalMs);
    btstack_run_loop_add_timer(&refill_timer_);
}

void BluetoothAudioSource::stop_stream() {
    if (!streaming_) {
        return;
    }

    btstack_run_loop_remove_timer(&refill_timer_);
    output_.stop();
    streaming_ = false;
}

void BluetoothAudioSource::close() {
    stop_stream();
    playback_ = nullptr;
}

void BluetoothAudioSource::refill() {
    if (!streaming_ || playback_ == nullptr) {
        return;
    }

    // Request a frame count proportional to actual elapsed wall-clock time rather than
    // a fixed guess. The run loop's timer callbacks can fire noticeably later than
    // kRefillIntervalMs under load (HCI/L2CAP/SBC decode all share this single core),
    // and always pulling a fixed block undersupplied real-time demand whenever that
    // happened, starving our own output DMA ring (silent underruns) even though SBC's
    // ring buffer was draining fine. Any local backpressure is absorbed by write_frames()
    // dropping already-decoded PCM rather than stalling the SBC decode pipeline.
    const uint32_t now = btstack_run_loop_get_time_ms();
    const uint32_t elapsed_ms = now - last_refill_time_ms_;
    last_refill_time_ms_ = now;

    uint32_t requested = (static_cast<uint64_t>(elapsed_ms) * sample_rate_hz_) / 1000;
    if (requested == 0) requested = 1;
    if (requested > kPcmBlockFrames) requested = kPcmBlockFrames;

    playback_(pcm_, static_cast<uint16_t>(requested), nullptr);
    if (volume_ < 127) {
        const size_t sample_count = requested * 2;
        for (size_t i = 0; i < sample_count; ++i) {
            pcm_[i] = static_cast<int16_t>((static_cast<int32_t>(pcm_[i]) * volume_) / 127);
        }
    }

    const size_t written = output_.write_frames(pcm_, requested);
    dropped_frames_ += static_cast<uint32_t>(requested - written);
}

int BluetoothAudioSource::sink_init(
    uint8_t channels, uint32_t sample_rate_hz,
    void (*playback)(int16_t*, uint16_t, const btstack_audio_context_t*)) {
    return instance_ ? instance_->configure(channels, sample_rate_hz, playback) : 1;
}

uint32_t BluetoothAudioSource::sink_get_sample_rate() {
    return instance_ ? instance_->sample_rate_hz_ : 0;
}

void BluetoothAudioSource::sink_set_volume(uint8_t volume) {
    if (instance_) {
        instance_->volume_ = volume > 127 ? 127 : volume;
    }
}

void BluetoothAudioSource::sink_start_stream() {
    if (instance_) {
        instance_->start_stream();
    }
}

void BluetoothAudioSource::sink_stop_stream() {
    if (instance_) {
        instance_->stop_stream();
    }
}

void BluetoothAudioSource::sink_close() {
    if (instance_) {
        instance_->close();
    }
}

void BluetoothAudioSource::refill_timer_handler(btstack_timer_source_t*) {
    if (!instance_ || !instance_->streaming_) {
        return;
    }

    instance_->refill();

    // rate-limited runtime stats (~once/second at kRefillIntervalMs=5ms) to diagnose
    // sustained SBC ring buffer overflows without flooding stdio
    static uint32_t tick = 0;
    if ((++tick % 200) == 0) {
        printf("BluetoothAudioSource: dropped=%lu underrun=%lu\n",
               static_cast<unsigned long>(instance_->dropped_frames_),
               static_cast<unsigned long>(instance_->output_.underrun_count()));
    }

    btstack_run_loop_set_timer(&instance_->refill_timer_, kRefillIntervalMs);
    btstack_run_loop_add_timer(&instance_->refill_timer_);
}

} // namespace eyes