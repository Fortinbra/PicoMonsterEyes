#include "ssd1351_display.hpp"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"

namespace eyes {

bool Ssd1351Display::init() {
    // Init GPIOs
    gpio_init(cs_);
    gpio_set_dir(cs_, GPIO_OUT);
    gpio_put(cs_, 1);

    gpio_init(dc_);
    gpio_set_dir(dc_, GPIO_OUT);
    gpio_put(dc_, 0);

    gpio_init(res_);
    gpio_set_dir(res_, GPIO_OUT);
    gpio_put(res_, 1);

    // Ensure SPI is initialized
    bus_.init();

    // Hardware reset
    hw_reset();

    cs_select();
    // Unlock commands
    write_cmd(CMD_COMMANDLOCK); write_data((const uint8_t*)"\x12", 1);
    write_cmd(CMD_COMMANDLOCK); write_data((const uint8_t*)"\xB1", 1);
    // Display off
    write_cmd(CMD_DISPLAYOFF);

    // Clock div
    write_cmd(CMD_CLOCKDIV); write_data((const uint8_t*)"\xF1", 1); // 7:4=div,3:0=osc freq
    // Mux ratio
    write_cmd(CMD_MUXRATIO); uint8_t mux = static_cast<uint8_t>(h_ - 1); write_data(&mux, 1);
    // Display offset + start line
    write_cmd(CMD_DISPLAYOFFSET); uint8_t off = 0; write_data(&off, 1);
    write_cmd(CMD_STARTLINE); uint8_t sl = 0; write_data(&sl, 1);
    // Remap: RGB565, 65k color, COM split etc. Typical 0x72 or 0x74; 0x72 gives RGB565
    // Set remap & color depth: 0x72 yielded swapped R/B on this module; 0x76 corrects color order (RGB565)
    write_cmd(CMD_SETREMAP); uint8_t remap[2] = {0x76, 0x00}; write_data(remap, 2);
    // Function select: internal regulator
    write_cmd(CMD_FUNCTIONSELECT); uint8_t fs = 0x01; write_data(&fs, 1);
    // Contrast/brightness settings (reasonable defaults)
    uint8_t contrastABC[3] = {0xC8, 0x80, 0xC8};
    write_cmd(CMD_CONTRASTABC); write_data(contrastABC, 3);
    uint8_t contrastMaster = 0x0F; // max
    write_cmd(CMD_CONTRASTMASTER); write_data(&contrastMaster, 1);
    // Precharge
    write_cmd(CMD_PRECHARGE); uint8_t pre = 0x32; write_data(&pre, 1);
    // VCOMH
    write_cmd(CMD_VCOMH); uint8_t vcomh = 0x05; write_data(&vcomh, 1);
    // Set VSL
    uint8_t vsl[3] = {0xA0, 0xB5, 0x55};
    write_cmd(CMD_SETVSL); write_data(vsl, 3);
    // Precharge2
    write_cmd(CMD_PRECHARGE2); uint8_t pre2 = 0x01; write_data(&pre2, 1);
    // Normal display
    write_cmd(CMD_NORMALDISPLAY);
    // Column/row range full
    set_window(0, 0, w_, h_);
    // Display on
    write_cmd(CMD_DISPLAYON);
    cs_deselect();

    sleep_ms(20);

    // Allocate TX DMA channel (optional)
    if (use_dma_ && dma_tx_chan_ < 0) {
        int ch = dma_claim_unused_channel(false);
        if (ch >= 0) {
            dma_tx_chan_ = ch;
            // Configure channel for 8-bit transfers paced by SPI TX DREQ
            dma_channel_config c = dma_channel_get_default_config(ch);
            channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
            channel_config_set_dreq(&c, spi_get_dreq(bus_.inst(), true));
            dma_channel_configure(ch, &c,
                &spi_get_hw(bus_.inst())->dr, // dst: SPI data register
                nullptr,                      // src set per transfer
                0,                            // count set per transfer
                false);
        } else {
            use_dma_ = false; // fallback
        }
    }
    return true;
}

void Ssd1351Display::fill(uint16_t color) {
    Rect r{0, 0, w_, h_};
    // Prepare a small buffer and stream it repeatedly to avoid large stack
    const size_t chunk_pixels = 64; // multiple of 2 not required, RGB565 2 bytes each
    uint16_t buf[chunk_pixels];
    for (size_t i = 0; i < chunk_pixels; ++i) buf[i] = color;

    cs_select();
    set_window(0, 0, w_, h_);
    write_cmd(CMD_WRITERAM);
    size_t total = static_cast<size_t>(w_) * static_cast<size_t>(h_);
    while (total > 0) {
        size_t now = total > chunk_pixels ? chunk_pixels : total;
        write_data_u16(buf, now);
        total -= now;
    }
    cs_deselect();
}

void Ssd1351Display::blit(uint16_t const* pixels, const Rect& area) {
    if (!pixels || area.w == 0 || area.h == 0) return;
    cs_select();
    set_window(area.x, area.y, area.w, area.h);
    write_cmd(CMD_WRITERAM);
    size_t count = (size_t)area.w * area.h;
    if (use_dma_ && dma_tx_chan_ >= 0) {
        // Convert line-by-line to reduce temp buffer size
        constexpr size_t LINE_MAX = 128; // width cap
        uint8_t conv[LINE_MAX * 2];
        const uint16_t* src = pixels;
        for (int y=0; y<area.h; ++y) {
            size_t line_pixels = area.w;
            for (size_t i=0;i<line_pixels;++i) {
                uint16_t px = src[i];
                conv[2*i]   = (uint8_t)(px >> 8);
                conv[2*i+1] = (uint8_t)(px & 0xFF);
            }
            src += area.w;
            dc_data();
            dma_channel_set_read_addr(dma_tx_chan_, conv, false);
            dma_channel_set_trans_count(dma_tx_chan_, line_pixels*2, true);
            dma_channel_wait_for_finish_blocking(dma_tx_chan_);
        }
    } else {
        write_data_u16(pixels, count);
    }
    cs_deselect();
}

void Ssd1351Display::cs_select() { gpio_put(cs_, 0); }
void Ssd1351Display::cs_deselect() { gpio_put(cs_, 1); }
void Ssd1351Display::dc_command() { gpio_put(dc_, 0); }
void Ssd1351Display::dc_data() { gpio_put(dc_, 1); }

void Ssd1351Display::hw_reset() {
    gpio_put(res_, 0);
    sleep_ms(10);
    gpio_put(res_, 1);
    sleep_ms(10);
}

void Ssd1351Display::write_cmd(uint8_t cmd) {
    dc_command();
    spi_write_blocking(bus_.inst(), &cmd, 1);
}

void Ssd1351Display::write_data(const uint8_t* data, size_t len) {
    if (!data || !len) return;
    dc_data();
    spi_write_blocking(bus_.inst(), data, len);
}

void Ssd1351Display::write_data_u16(const uint16_t* data, size_t count) {
    if (!data || !count) return;
    dc_data();
    // Convert in chunks to reduce per-pixel SPI calls
    constexpr size_t CHUNK = 256; // larger burst for better throughput
    uint8_t buf[CHUNK * 2];
    size_t i = 0;
    while (i < count) {
        size_t n = count - i;
        if (n > CHUNK) n = CHUNK;
        for (size_t k = 0; k < n; ++k) {
            uint16_t px = data[i + k];
            buf[2*k]     = static_cast<uint8_t>(px >> 8);
            buf[2*k + 1] = static_cast<uint8_t>(px & 0xFF);
        }
        spi_write_blocking(bus_.inst(), buf, n * 2);
        i += n;
    }
}

void Ssd1351Display::set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint8_t col[2] = { static_cast<uint8_t>(x), static_cast<uint8_t>(x + w - 1) };
    uint8_t row[2] = { static_cast<uint8_t>(y), static_cast<uint8_t>(y + h - 1) };
    write_cmd(CMD_SETCOLUMN); write_data(col, 2);
    write_cmd(CMD_SETROW);    write_data(row, 2);
}

} // namespace eyes
