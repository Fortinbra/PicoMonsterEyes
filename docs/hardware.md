# Hardware

This project drives two WaveShare 1.5" RGB OLED modules (SSD1351-compatible, 128×128, 65K colors) over SPI and outputs mono audio via a MAX98357A I2S amplifier. The default board is Raspberry Pi Pico 2 (RP2350) using 3.3V logic.

## Displays (x2)

- Controller: SSD1351 or compatible
- Interface: SPI
- Shared bus recommended: SCK/MOSI shared, per-display CS/DC/RES
- Power: 3.3V logic; check module for power requirements (often 3.3–5V tolerant on VCC)

Suggested wiring (example):

- SPI0 SCK -> GPIO18
- SPI0 MOSI -> GPIO19
- SPI0 MISO -> optional (not typically needed for OLED)
- Display L CS/DC/RES -> dedicated GPIOs
- Display R CS/DC/RES -> dedicated GPIOs

Note: Keep CS high when inactive; batch pixel transfers using a RAM buffer. Avoid per-pixel write loops.

## Audio (MAX98357A)

- Data: I2S (BCLK, LRCLK, DIN)
- Implementation: Pico SDK PIO-based I2S; 16-bit mono at 22.05 or 44.1 kHz
- Power: 3.3–5V input on module; logic is 3.3V tolerant

Suggested pins (example; to be finalized in a `boards/` header):

- BCLK -> GPIO10
- LRCLK -> GPIO11
- DIN -> GPIO12

## Power and grounding

- Ensure common ground between Pico, both displays, and the amplifier
- Budget current for OLED backlights and amplifier peaks; use decoupling near modules

## Pin mapping policy

- Centralize all pin definitions in a single header (e.g., `boards/pico2_pins.hpp`)
- Use `constexpr` pin IDs and group by function (SPI, displays L/R, I2S)
- Provide a simple `static_assert` guard to prevent duplicate pin assignment
