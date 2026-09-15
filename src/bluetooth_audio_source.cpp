#include "bluetooth_audio_source.hpp"

#include <algorithm>

#include "audio_output.hpp"
#include "pico/cyw43_arch.h"

extern "C" int btstack_main(int argc, const char* argv[]);

namespace eyes {

BluetoothAudioSource* BluetoothAudioSource::instance_ = nullptr;

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
    return output_ready_ ? 0 : 1;
}

void BluetoothAudioSource::start_stream() {
    if (!output_ready_ || streaming_ || !output_.start()) {
        return;
    }

    streaming_ = true;
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

    const size_t requested = std::min(output_.available_frames(), static_cast<size_t>(kPcmBlockFrames));
    if (requested == 0) {
        return;
    }

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
    btstack_run_loop_set_timer(&instance_->refill_timer_, kRefillIntervalMs);
    btstack_run_loop_add_timer(&instance_->refill_timer_);
}

} // namespace eyes