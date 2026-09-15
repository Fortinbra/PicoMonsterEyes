# Feature: Stereo I2S Audio Output

## Status

Implemented. `AudioOutput` now exposes a frame-based stereo contract (`write_frames`/`available_frames`/`underrun_count`), and `Max98357aI2sOutput` drives a PIO I2S transmitter (`drivers/audio_i2s.pio`) fed by a single DMA channel reading from a static ring buffer, with silence fallback and an underrun counter on buffer starvation. Hardware validation (channel orientation, sustained playback, 44.1kHz) is still outstanding.

## Goal

Play interleaved 16-bit stereo PCM through the amplifier's left and right channels while preserving the firmware's deterministic rendering loop. Audio transfers must not block display updates or perform work in an interrupt handler beyond keeping the output queue fed.

## Hardware Scope

- Board: Raspberry Pi Pico 2 (RP2350), using 3.3 V logic.
- Amplifier: Adafruit Stereo I2S 3W Amplifier Breakout - Dual MAX98357A.
- I2S signals: shared bit clock (BCLK), word-select/left-right clock (LRCLK), and serial data input (DIN).
- Audio format: I2S, 16-bit signed stereo PCM, interleaved left/right frames.
- Initial sample-rate target: 22,050 Hz. Support for 44,100 Hz is a follow-up once timing and DMA throughput are confirmed on hardware.

The final wiring must be recorded in `boards/pico2_pins.hpp` and `docs/hardware.md`. Confirm the breakout's exact signal labels, power requirements, shutdown behavior, and left/right channel configuration against its current Adafruit guide before connecting it. The Pico, amplifier, and both displays must share ground.

## Design Direction

- Keep `AudioOutput` as the application-facing abstraction, but make its PCM contract explicitly stereo or introduce a small format descriptor that can express channel count and sample rate.
- Replace the mono-only `write_samples(const int16_t*, size_t)` contract with a frame-based API. One stereo frame contains one left and one right `int16_t` sample.
- Update `Max98357aI2sOutput` to own a PIO I2S transmitter and a statically allocated DMA-friendly ring buffer.
- Feed PIO through DMA. The foreground producer writes complete frames; the DMA/IRQ path only advances buffer ownership and starts the next transfer.
- Keep pin assignments centralized in `boards/pico2_pins.hpp`; do not duplicate GPIO numbers in the driver or application.
- Avoid dynamic allocation and blocking waits in the steady-state audio path.

## Milestones

1. Validate the breakout wiring and document its final pins, power connection, speakers, and tested channel orientation.
2. Define the stereo PCM frame/API contract in `include/audio_output.hpp` and adapt call sites.
3. Implement PIO I2S framing for 16-bit interleaved stereo data.
4. Add a lock-free or IRQ-safe static ring buffer and DMA servicing to `Max98357aI2sOutput`.
5. Add a temporary deterministic stereo test tone: distinct tones on left and right, with a mono duplicate mode for speaker/wiring checks.
6. Integrate audio initialization and a producer into `App` without disrupting eye rendering.
7. Measure behavior at the initial sample rate and consider 44,100 Hz only after the lower rate is reliable.

## Acceptance Criteria

- The firmware builds with the VS Code `Compile Project` task.
- Both channels play the intended, distinct test signals with no channel swap.
- Continuous stereo playback runs alongside both eye displays without audible dropouts, periodic clicks, or visible display stalls during a 10-minute hardware run.
- Buffer underruns are observable through a lightweight counter or compile-time-selectable diagnostic, without logging from interrupts.
- Start and stop leave PIO, DMA, and GPIO resources in a defined state.
- The final wiring is documented in `docs/hardware.md` and matches `boards/pico2_pins.hpp`.

## Out of Scope

- Audio-file decoding, SD-card streaming, Bluetooth audio, mixing more than a small fixed number of PCM sources, and 44,100 Hz support are not required for the first implementation.
