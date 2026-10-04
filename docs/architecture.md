# Architecture

The firmware follows SOLID principles and separates hardware drivers from application logic. The main loop stays lean; peripherals are wrapped in small C++ classes that manage lifetimes (RAII) and expose minimal, testable interfaces.

## High-level components

- App — orchestrates two Eye controllers, display manager, and audio output
- DisplayManager — owns two display instances and shared SPI bus
- Display (interface) — abstract drawing API (init, fill, blit, rect)
- Ssd1351Display — SPI SSD1351 driver; batches transfers; no per-pixel calls
- AudioOutput (interface) — push PCM frames, start/stop
- Max98357aI2sOutput — PIO-based I2S transmitter with IRQ-safe ring buffer
- Eye — animation state and rendering for one eye (blink, look, idle)

## Data flow

- App ticks Eye instances on a timer; Eye renders into a small RGB565 buffer
- DisplayManager blits buffers to each display via shared SPI (separate CS/DC)
- AudioOutput consumes PCM from a producer (e.g., sound effects queue)

## Error handling

- Prefer status enums/booleans over exceptions in hot paths
- Validate pin maps and SPI frequencies at init with `assert`/`static_assert`

## Timing

- Use `pico_time` alarms for periodic tasks
- Keep IRQ/PIO handlers minimal; move work to foreground

Gaze starts with a six-second centered stare, then alternates eased movements
(0.9-1.8 seconds) with randomized 6-10 second fixations. Emotion modifiers may
lengthen pauses or slow movement, but cannot make either faster than this calm
baseline. Blinks, rendering, and audio continue independently during fixations.

## Directory layout (planned)

- include/
  - display.hpp, audio_output.hpp, eye.hpp
- drivers/
  - ssd1351_display.cpp/.hpp, max98357a_i2s_output.cpp/.hpp, spi_bus.cpp/.hpp
- src/
  - app.cpp/.hpp, display_manager.cpp/.hpp, main.cpp (thin)
- boards/
  - pico2_pins.hpp (central pin map)

## Build

- CMake + Ninja via VS Code tasks only
- Add new sources with `target_sources(PicoMonsterEyes PRIVATE ...)`
- Link Pico SDK libs through `target_link_libraries`
