#pragma once

#include <cstdint>
#include "display.hpp"

namespace eyes {

class Eye {
public:
    explicit Eye(Display& display) : display_(display) {}
    bool init() { return display_.init(); }
    void tick(uint32_t /*ms*/) {}
    void render() {}
private:
    Display& display_;
};

} // namespace eyes
