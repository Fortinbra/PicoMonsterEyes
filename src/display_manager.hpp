#pragma once

#include <cstdint>
#include <memory>
#include "display.hpp"

namespace eyes {

class DisplayManager {
public:
    DisplayManager(Display& left, Display& right)
        : left_(left), right_(right) {}

    bool init() { return left_.init() && right_.init(); }
    Display& left() { return left_; }
    Display& right() { return right_; }

private:
    Display& left_;
    Display& right_;
};

} // namespace eyes
