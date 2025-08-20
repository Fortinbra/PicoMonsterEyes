# Copilot Instructions for PicoMonsterEyes

This repository is a C++ firmware project targeting Raspberry Pi Pico boards using the Raspberry Pi Pico SDK v2.2.0, built with CMake and Ninja. The firmware drives a Frankenstein's Monster animatronic's "eyes": two WaveShare 1.5" RGB OLED 128×128 SPI displays (65K colors, SSD1351-class) and provides audio output via a MAX98357A I2S amplifier. Use these rules when proposing code, tasks, or commands.

## Project facts
- Language: C++17 (see `CMakeLists.txt`: `set(CMAKE_CXX_STANDARD 17)`).
- SDK: Raspberry Pi Pico SDK 2.2.0 (via `pico_sdk_import.cmake`).
- Tooling: CMake + Ninja generator; tasks are preconfigured to build/flash.
- Target: `PicoMonsterEyes` executable with `pico_stdlib`, `hardware_spi`, and `hardware_i2c`.
- Board: `PICO_BOARD` is `pico2` by default.

## Hardware scope
- Displays: 2× WaveShare 1.5" RGB OLED, 128×128, 65K colors, SPI (SSD1351-compatible). Shared SPI bus recommended with per-display CS/DC/RES.
- Audio: MAX98357A Class-D I2S amplifier (Mono). Use PIO-based I2S on Pico SDK (BCLK, LRCLK, DIN) with 3.3V logic.
- Keep pin mappings centralized (e.g., in `boards/` or a single header). Prefer `constexpr` pin constants and avoid scattering raw GPIO numbers.

## Build and flash policy
- Always build via VS Code tasks. Do not invoke raw build commands directly.
  - Build: Use the task labeled "Compile Project".
  - Never attempt to "run" the firmware as a host app; it's bare-metal.
  - Flash: Use the provided task(s):
    - "Run Project" (uses `picotool load ... -fx`) to load over USB bootloader.
    - "Flash" (OpenOCD) for SWD flashing and verify.
    - Rescue tasks exist for recovery only.
- Never suggest executing the binary locally.

When proposing any build/flash guidance, always reference these tasks only. Do not emit raw `cmake`/`ninja` commands.

## Coding guidelines
Follow SOLID principles and modern C++ best practices suitable for embedded firmware:
- Single-responsibility: split hardware drivers, application logic, and boot/startup.
- Open/closed: prefer interfaces/abstract base classes for components (e.g., display, sensors), inject concrete hardware implementations.
- Liskov substitution: interfaces must be substitutable; avoid surprising preconditions.
- Interface segregation: small, purpose-fit interfaces over god-classes.
- Dependency inversion: depend on abstractions; use constructor injection for hardware handles or wrapper types, not globals.

C++ practices for Pico SDK:
- Use RAII for peripheral setup/teardown guards when feasible; encapsulate initialization sequences (SPI/I2C/UART) in classes.
- Prefer `constexpr`, `enum class`, and strong types for pins, speeds, and addresses.
- Avoid dynamic allocation in steady-state; allocate statically or on stack. If needed, centralize allocation and check failures.
- Keep ISRs minimal; mark with `__isr` as needed and avoid heavy work.
- Use `pico_time` for timing instead of ad-hoc busy loops when practical.
- Hide Pico SDK C APIs behind thin C++ wrappers to keep call sites clean and testable.
- Provide lightweight seams for unit-like tests on host when possible by stubbing HW abstractions.

Display and audio specifics:
- Displays: Create a `Display` interface with concrete `Ssd1351Display` drivers. Support two instances on a shared SPI bus with separate CS/DC/RES. Batch SPI transfers; avoid per-pixel calls. Provide simple draw APIs (fill, blit, rect) and coordinate system abstractions.
- Audio: Create an `AudioOutput` interface with a `Max98357aI2sOutput` that uses PIO-based I2S. Use a lock-free or IRQ-safe ring buffer for PCM samples; target 16-bit mono at a modest sample rate (e.g., 22.05/44.1 kHz as feasible). Keep ISR/PIO callbacks lean.

## CMake/Ninja guidance
- Keep `CMakeLists.txt` the single source of truth; add new source files to `add_executable` or use `target_sources`.
- Link against Pico SDK libs via `target_link_libraries` only; avoid manual include paths for SDK internals.
- Use `target_include_directories(PicoMonsterEyes PRIVATE <paths>)` for local headers.
- If you add libraries/components, express them as separate targets with clear public/private include usage.
- For drivers (SSD1351, MAX98357A), prefer separate translation units in `drivers/` and expose small public headers in `include/` or `drivers/`.

## File/structure conventions
- Source under the repo root for now. If the project grows, prefer:
  - `src/` for application logic
  - `include/` for public headers
  - `drivers/` or `hal/` for hardware wrappers
  - `boards/` for board-specific config
  - Keep `main.cpp` thin; delegate to an `App` type coordinating subsystems.

Suggested high-level types:
- `App` orchestrates two `Eye` controllers, a `DisplayManager` (two displays), and an `AudioOutput`.
- `SpiBus` thin wrapper, `GpioPin` helpers, `Display` interface, `Ssd1351Display` concrete class.
- `AudioOutput` interface, `Max98357aI2sOutput` implementation, with a small audio mixer if needed.

## Style
- Consistent naming, small functions, and explicit lifetimes.
- Use `printf`/`uart_puts` only at the edges; prefer a lightweight logging abstraction that can be compiled out.
- Document hardware pin mappings next to code and guard with `static_asserts` where appropriate.
- Keep configuration (pins, SPI speed, display orientation) centralized and constexpr.

## Pull requests and reviews
- Include a brief description of changes, affected modules, and how it was tested on hardware.
- Ensure the project builds with the "Compile Project" task before requesting review.
- If flashing is needed, use the "Run Project" or "Flash" tasks accordingly and note which path was used.

## What to avoid
- Do not suggest running the program on the host machine.
- Do not bypass VS Code tasks to build/flash.
- Do not introduce heavy STL or exceptions in hot paths; be mindful of binary size and determinism.
- Do not hard-code duplicated pin numbers across files; avoid per-pixel SPI writes; avoid blocking audio paths.

## Quick references
- Main sources: `main.cpp`, `CMakeLists.txt`.
- Tasks exist for: build (Compile Project), load via USB bootloader (Run Project), SWD flash (Flash), rescue resets.

## Notes for suggestions
- Treat the WaveShare OLED as SSD1351-compatible unless the controller is explicitly changed.
- For MAX98357A, use PIO-based I2S from Pico SDK examples as a baseline; expose pins (BCLK, LRCLK, DIN) via config.
- If proposing new files, follow the structure above and update `CMakeLists.txt` with `target_sources` accordingly.

These instructions are binding for Copilot suggestions in this repository. Keep edits aligned with Pico SDK 2.2.0 and Ninja-based builds initiated via tasks only.
