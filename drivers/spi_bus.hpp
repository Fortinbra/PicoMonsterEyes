#pragma once

#include <cstdint>
#include "hardware/spi.h"

namespace eyes {

class SpiBus {
public:
    explicit SpiBus(spi_inst_t* inst, uint32_t hz) : inst_(inst), hz_(hz) {}
    bool init() { spi_init(inst_, hz_); return true; }
    spi_inst_t* inst() const { return inst_; }
    // Attempt to change SPI frequency at runtime; returns actual set rate
    uint32_t set_frequency(uint32_t hz) { hz_ = spi_set_baudrate(inst_, hz); return hz_; }
private:
    spi_inst_t* inst_;
    uint32_t hz_;
};

} // namespace eyes
