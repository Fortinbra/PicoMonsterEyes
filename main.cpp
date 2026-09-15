// Minimal main wired to App skeleton
#include <stdio.h>
#include "pico/stdlib.h"
#include "src/app.hpp"

int main() {
    stdio_init_all();

    eyes::App app;
    if (!app.init()) {
        // Fallback to a simple heartbeat if init fails
        while (true) { sleep_ms(1000); }
    }
    app.loop();
}
