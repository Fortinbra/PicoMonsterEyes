# PicoMonsterEyes

Firmware for a Frankenstein's Monster animatronic head. Drives two WaveShare 1.5" RGB OLED displays (128×128, SPI) as animated eyes and outputs audio through a MAX98357A I2S amplifier. Built on Raspberry Pi Pico SDK 2.2.0 in C++17 with CMake + Ninja.

## Features

- Dual 128×128 RGB OLED eyes over shared SPI with per-eye CS/DC/RES
- Configurable rendering primitives (planned): fill, blit, simple shapes
- Audio output via MAX98357A (PIO-based I2S) with simple PCM streaming (planned)
- Clean C++ architecture following SOLID principles

## Hardware

- MCU: Raspberry Pi Pico 2 (RP2350) by default (`PICO_BOARD=pico2`)
- Displays: 2× WaveShare 1.5" RGB OLED, 65K colors, 128×128, SPI (SSD1351-compatible)
- Audio: MAX98357A I2S Class-D amplifier (mono)

See `docs/hardware.md` for wiring and pin mapping guidance.

## Building and flashing

This is bare-metal firmware. Do not run locally. Use the VS Code tasks only:

- Build: Task "Compile Project"
- Flash via USB bootloader: Task "Run Project" (picotool)
- Flash via SWD: Task "Flash" (OpenOCD)

The tasks are preconfigured by the Raspberry Pi Pico VS Code extension and use the Ninja generator. Avoid running raw `cmake`/`ninja` commands.

## Repository layout

- `main.cpp` — entry point (kept thin)
- `CMakeLists.txt` — build configuration
- `docs/` — project documentation
  - `hardware.md` — wiring, pin maps, power notes
  - `architecture.md` — code structure, interfaces, and responsibilities
- `drivers/`, `src/`, `include/` — planned structure for drivers and application logic
- `assets/` — place for eye asset headers/data (attribution preserved)

## Assets

Original eye images used by many variants (sclera, iris, eyelids) originate from Adafruit’s Uncanny Eyes and forks. See `docs/assets.md` for where to obtain and how to integrate them. When reusing, keep the original MIT license headers and attribution in each file.

## Development guidelines

- Follow SOLID. Keep hardware-specific code in drivers; keep application logic in `src/`.
- Use RAII to manage peripheral init/teardown. Prefer `constexpr` for pin IDs and speeds.
- Minimize dynamic allocation; avoid heavy STL in hot paths.
- Keep ISRs minimal; use `pico_time` for timing.

## Status

Initial scaffolding present. Drivers and higher-level modules (displays, audio, eye animations) to be added incrementally.

## License

MIT (or project default).

## Attribution

This project builds on prior work and ideas from the following open-source projects:

- CreeperEyes (tooluser/CreeperEyes): [github.com/tooluser/CreeperEyes](https://github.com/tooluser/CreeperEyes)
  - Provides eye rendering assets and logic adapted for SSD1351 displays on ESP32.
  - Licensed under MIT. We will retain the original file headers and attribution when reusing any assets.

- Adafruit Uncanny Eyes: [github.com/adafruit/Uncanny_Eyes](https://github.com/adafruit/Uncanny_Eyes)
  - Original Uncanny Eyes implementation by Phil Burgess (Paint Your Dragon) for Adafruit Industries.
  - Licensed under MIT. We include attribution consistent with the original header comments and preserve license notices in any derived files.

We acknowledge and thank the authors and maintainers of these projects. Any imported source or asset will keep its original attribution and license header.
