#include "max98357a_i2s_output.hpp"

namespace eyes {

bool Max98357aI2sOutput::init(uint32_t /*sample_rate_hz*/) {
    return true;
}

bool Max98357aI2sOutput::start() {
    return true;
}

void Max98357aI2sOutput::stop() {
}

size_t Max98357aI2sOutput::write_samples(const int16_t* /*samples*/, size_t count) {
    return count;
}

} // namespace eyes
