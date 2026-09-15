#include "max98357a_i2s_output.hpp"

#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h"

namespace eyes {

int32_t Max98357aI2sOutput::ring_[Max98357aI2sOutput::kRingFrames];
const int32_t Max98357aI2sOutput::silence_[Max98357aI2sOutput::kSilenceFrames] = {};
Max98357aI2sOutput* Max98357aI2sOutput::s_instance_ = nullptr;

bool Max98357aI2sOutput::init(uint32_t sample_rate_hz) {
    stop();

    if (!pio_) {
        gpio_set_function(bclk_, GPIO_FUNC_PIO0);
        gpio_set_function(lrclk_, GPIO_FUNC_PIO0);
        gpio_set_function(din_, GPIO_FUNC_PIO0);

        pio_ = pio0;
        pio_offset_ = pio_add_program(pio_, &audio_i2s_program);
        sm_ = pio_claim_unused_sm(pio_, true);
        audio_i2s_program_init(pio_, sm_, pio_offset_, din_, bclk_);
    } else {
        pio_sm_clear_fifos(pio_, sm_);
        pio_sm_restart(pio_, sm_);
    }

    // SM runs at sample_rate_hz * 64 (32 bits/frame, 2 SM cycles/bit); see audio_i2s.pio.
    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t divider = sys_hz * 4 / sample_rate_hz;
    pio_sm_set_clkdiv_int_frac(pio_, sm_, static_cast<uint16_t>(divider >> 8u), static_cast<uint8_t>(divider & 0xffu));

    if (dma_chan_ < 0) {
        dma_chan_ = dma_claim_unused_channel(true);
        dma_channel_config c = dma_channel_get_default_config(dma_chan_);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pio_get_dreq(pio_, sm_, true));
        dma_channel_configure(dma_chan_, &c, &pio_->txf[sm_], nullptr, 0, false);

        s_instance_ = this;
        irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
        irq_set_enabled(DMA_IRQ_0, true);
        dma_irqn_set_channel_enabled(0, dma_chan_, true);
    }

    frames_written_ = 0;
    frames_completed_ = 0;
    chunk_len_in_flight_ = 0;
    chunk_is_silence_ = false;
    underrun_count_ = 0;
    return true;
}

bool Max98357aI2sOutput::start() {
    if (!pio_ || dma_chan_ < 0) return false;
    pio_sm_set_enabled(pio_, sm_, true);
    started_ = true;
    start_next_dma_chunk();
    return true;
}

void Max98357aI2sOutput::stop() {
    if (!started_) return;
    dma_channel_abort(dma_chan_);
    pio_sm_set_enabled(pio_, sm_, false);
    started_ = false;
}

size_t Max98357aI2sOutput::write_frames(const int16_t* interleaved_lr, size_t frame_count) {
    size_t avail = available_frames();
    size_t n = frame_count < avail ? frame_count : avail;
    size_t write_idx = frames_written_ % kRingFrames;
    for (size_t i = 0; i < n; ++i) {
        int16_t l = interleaved_lr[2 * i];
        int16_t r = interleaved_lr[2 * i + 1];
        ring_[write_idx] = (static_cast<uint32_t>(static_cast<uint16_t>(l)) << 16)
                          | static_cast<uint32_t>(static_cast<uint16_t>(r));
        write_idx = (write_idx + 1) % kRingFrames;
    }
    frames_written_ += static_cast<uint32_t>(n);
    return n;
}

size_t Max98357aI2sOutput::available_frames() const {
    uint32_t completed = frames_completed_;
    uint32_t written = frames_written_;
    return kRingFrames - (written - completed);
}

void Max98357aI2sOutput::start_next_dma_chunk() {
    uint32_t pending = frames_written_ - frames_completed_;
    if (pending == 0) {
        if (!chunk_is_silence_) {
            underrun_count_++;
        }
        chunk_is_silence_ = true;
        chunk_len_in_flight_ = kSilenceFrames;
        dma_channel_config c = dma_get_channel_config(dma_chan_);
        channel_config_set_read_increment(&c, false);
        dma_channel_set_config(dma_chan_, &c, false);
        dma_channel_transfer_from_buffer_now(dma_chan_, const_cast<int32_t*>(&silence_[0]), kSilenceFrames);
        return;
    }
    chunk_is_silence_ = false;
    size_t read_idx = frames_completed_ % kRingFrames;
    size_t contiguous = kRingFrames - read_idx;
    if (contiguous > pending) contiguous = pending;
    chunk_len_in_flight_ = static_cast<uint32_t>(contiguous);
    dma_channel_config c = dma_get_channel_config(dma_chan_);
    channel_config_set_read_increment(&c, true);
    dma_channel_set_config(dma_chan_, &c, false);
    dma_channel_transfer_from_buffer_now(dma_chan_, &ring_[read_idx], static_cast<uint32_t>(contiguous));
}

void Max98357aI2sOutput::on_dma_complete() {
    if (!chunk_is_silence_) {
        frames_completed_ += chunk_len_in_flight_;
    }
    start_next_dma_chunk();
}

void Max98357aI2sOutput::dma_irq_handler() {
    if (!s_instance_) return;
    int chan = s_instance_->dma_chan_;
    if (dma_irqn_get_channel_status(0, chan)) {
        dma_irqn_acknowledge_channel(0, chan);
        s_instance_->on_dma_complete();
    }
}

} // namespace eyes
