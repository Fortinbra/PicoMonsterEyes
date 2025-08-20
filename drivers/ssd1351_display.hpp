#pragma once

#include <cstdint>
#include "display.hpp"
#include "spi_bus.hpp"

namespace eyes {

class Ssd1351Display : public Display {
public:
    Ssd1351Display(SpiBus& bus, uint16_t width, uint16_t height,
                   uint8_t pin_cs, uint8_t pin_dc, uint8_t pin_res)
        : bus_(bus), w_(width), h_(height), cs_(pin_cs), dc_(pin_dc), res_(pin_res) {}

    bool init() override;
    void fill(uint16_t color) override;
    void blit(uint16_t const* pixels, const Rect& area) override;
    uint16_t width() const override { return w_; }
    uint16_t height() const override { return h_; }

private:
    // SSD1351 command set (subset)
    static constexpr uint8_t CMD_SETCOLUMN      = 0x15;
    static constexpr uint8_t CMD_SETROW         = 0x75;
    static constexpr uint8_t CMD_WRITERAM       = 0x5C;
    static constexpr uint8_t CMD_COMMANDLOCK    = 0xFD;
    static constexpr uint8_t CMD_DISPLAYOFF     = 0xAE;
    static constexpr uint8_t CMD_DISPLAYON      = 0xAF;
    static constexpr uint8_t CMD_CLOCKDIV       = 0xB3;
    static constexpr uint8_t CMD_MUXRATIO       = 0xCA;
    static constexpr uint8_t CMD_SETREMAP       = 0xA0;
    static constexpr uint8_t CMD_STARTLINE      = 0xA1;
    static constexpr uint8_t CMD_DISPLAYOFFSET  = 0xA2;
    static constexpr uint8_t CMD_FUNCTIONSELECT = 0xAB;
    static constexpr uint8_t CMD_PRECHARGE      = 0xB1;
    static constexpr uint8_t CMD_VCOMH          = 0xBE;
    static constexpr uint8_t CMD_NORMALDISPLAY  = 0xA6;
    static constexpr uint8_t CMD_CONTRASTABC    = 0xC1;
    static constexpr uint8_t CMD_CONTRASTMASTER = 0xC7;
    static constexpr uint8_t CMD_SETVSL         = 0xB4;
    static constexpr uint8_t CMD_PRECHARGE2     = 0xB6;

    void cs_select();
    void cs_deselect();
    void dc_command();
    void dc_data();
    void hw_reset();
    void write_cmd(uint8_t cmd);
    void write_data(const uint8_t* data, size_t len);
    void write_data_u16(const uint16_t* data, size_t count);
    void set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    SpiBus& bus_;
    uint16_t w_;
    uint16_t h_;
    uint8_t cs_;
    uint8_t dc_;
    uint8_t res_;
};

} // namespace eyes
