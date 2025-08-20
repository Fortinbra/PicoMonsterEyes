#pragma once

#include <cstdint>
#include "hardware/spi.h"

namespace eyes {

class SpiBus {
public:
    explicit SpiBus(spi_inst_t* inst, uint32_t hz) : inst_(inst), hz_(hz) {}
    bool init() { spi_init(inst_, hz_); return true; }
    spi_inst_t* inst() const { return inst_; }
private:
    spi_inst_t* inst_;
    uint32_t hz_;
};

} // namespace eyes
