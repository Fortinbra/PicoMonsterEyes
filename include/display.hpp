#pragma once

#include <cstdint>

namespace eyes {

struct Rect {
    uint16_t x{0}, y{0}, w{0}, h{0};
};

// Abstract display interface (RGB565 assumed)
class Display {
public:
    virtual ~Display() = default;
    virtual bool init() = 0;
    virtual void fill(uint16_t color) = 0;
    virtual void blit(uint16_t const* pixels, const Rect& area) = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
};

} // namespace eyes
