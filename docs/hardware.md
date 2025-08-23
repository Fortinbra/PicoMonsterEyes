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

### Color‑coded wiring reference

Wire color legend (your stated scheme):

- Red = VCC (3V3)
- Black = GND
- Blue = DIN (MOSI)
- Yellow = CLK (SCK)
- Orange = CS
- Green = DC (D/C)
- White = RST (RESET)

Using the current default pin assignments from `boards/pico2_pins.hpp` (shared SPI0):

Shared signals (both displays)
- Yellow (SCK)  -> GP18
- Blue (DIN)    -> GP19
- (MISO not used; leave unconnected)
- Red (VCC)     -> 3V3 OUT
- Black (GND)   -> GND

Left display
- Orange (CS)   -> GP17
- Green (DC)    -> GP20
- White (RST)   -> GP21

Right display
- Orange (CS)   -> GP22
- Green (DC)    -> GP26
- White (RST)   -> GP27

Optional pin reductions:
- You may tie both RST lines together (e.g. to GP21) and/or both DC lines (e.g. to GP20) to save GPIOs; then adjust the second display’s pins in `pico2_pins.hpp`.
- Keep separate CS lines; they must remain distinct.

Practical tips:
- Route the shared SCK and DIN as a short trunk to both modules; branch near the displays to minimize skew.
- Keep the RESET (white) line pulled high by default; firmware will pulse it low on init.
- If you observe color channel swapping, adjust the SSD1351 remap setting in the driver.

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
