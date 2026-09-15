# Feature: Bluetooth Speaker Input

## Status

Implemented in firmware; physical hardware acceptance testing remains. A phone can discover the Pico 2 W as `Pico Monster Eyes`, pair using Bluetooth Classic A2DP, and play SBC-decoded stereo audio through the Adafruit Stereo I2S 3W Amplifier Breakout - Dual MAX98357A.

The firmware targets `pico2_w`, and the installed Pico SDK includes `pico_btstack` and `pico_cyw43_arch`. The official upstream `pico-examples` repository provides `a2dp_sink_demo` ("A2DP Sink - Receive Audio Stream and Control Playback") as the implementation baseline. The upstream Pico audio backend requires `pico-extras`, but this project replaces that backend with its existing I2S driver.

This implementation reuses the project's DMA-backed I2S output instead of `pico-extras`. CMake fetches BTstack commit `eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec`, the exact gitlink pinned by Pico SDK 2.3.1. The A2DP sink application comes from that dependency's `example/a2dp_sink_demo.c`; review its license before commercial distribution.

## Goal

Advertise a friendly speaker name to Bluetooth-capable phones, accept a Bluetooth Classic A2DP audio connection, decode its mandatory SBC audio stream, and deliver stereo PCM to the non-blocking I2S output pipeline without disrupting eye rendering.

## Protocol Requirements

- Bluetooth transport: Bluetooth Classic, not Bluetooth Low Energy.
- Audio profile: A2DP Sink, so the Pico receives audio from the phone.
- Codec: SBC is mandatory for A2DP and is the first supported codec.
- Media controls: AVRCP playback controls are optional for the first release.
- Output: decoded interleaved 16-bit stereo PCM frames supplied to `AudioOutput`.

BLE audio advertising alone cannot make the board behave as a conventional phone speaker. The implementation must provide Bluetooth Classic discovery, pairing, A2DP capability negotiation, media transport, and SBC decoding.

## Prerequisite

Complete [Stereo I2S Audio Output](feature-stereo-i2s-audio.md) first. The I2S output must accept continuous stereo PCM through a DMA-backed static ring buffer, with measured underrun behavior, before a Bluetooth source is connected.

## Reference Implementation And Feasibility Spike

Before integrating application code, build and flash the upstream `a2dp_sink_demo` baseline for Pico 2 W with its required `pico-extras` audio support. Preserve its applicable notices and record the exact `pico-examples`, Pico SDK, BTstack, and `pico-extras` revisions. Then adapt the proof of concept to feed the project's stereo I2S output. It must demonstrate all of the following on actual hardware:

1. The phone discovers the board as a Bluetooth Classic audio device.
2. Pairing and reconnection work after a power cycle.
3. An A2DP source stream reaches an SBC decoder.
4. Decoded PCM is produced at a stable rate suitable for the I2S output.
5. The combined wireless stack, decoder, frame buffers, and eye renderer fit the RP2350's available flash and RAM.

If the selected compatible revisions cannot build and run the A2DP Sink baseline on the onboard CYW43439 radio, stop before application integration and evaluate one of these alternatives:

- A supported external Bluetooth Classic audio receiver that exposes I2S PCM.
- A separate Bluetooth audio coprocessor with I2S output.
- A documented firmware stack upgrade or maintained third-party implementation with compatible licensing and an active test path.

## Design Direction

- Keep Bluetooth transport, A2DP session handling, SBC decoding, and I2S output in separate components.
- Introduce a `BluetoothAudioSource`-style producer that writes decoded PCM frames to `AudioOutput`; `App` coordinates lifecycle and user-facing state only.
- Use static, bounded queues between the Bluetooth callback context, decoder, and I2S output. Define overflow and underrun policies before implementation.
- Keep Bluetooth callbacks short. No display writes, heap allocation, blocking I2S writes, or logging from time-sensitive callbacks.
- Reserve a dedicated core only after measurement justifies it. Any inter-core queue must be fixed-size and IRQ-safe.
- Preserve the centralized GPIO policy. Bluetooth uses the onboard radio, so the only audio GPIOs remain the I2S pins documented in `boards/pico2_pins.hpp`.
- Choose the Bluetooth device name, pairing policy, and reset/forget behavior as explicit configuration rather than hard-coding them in protocol code.

## Milestones

1. Complete and hardware-test the stereo I2S output feature.
2. Build and flash the `a2dp_sink_demo` baseline, then record its pinned dependency revisions, license notices, memory use, and phone test matrix.
3. Add a small Bluetooth radio/session abstraction that owns initialization, discoverability, pairing, and reconnection state.
4. Implement A2DP capability negotiation and SBC decode into fixed-size PCM blocks.
5. Bridge decoded PCM to the stereo `AudioOutput` ring buffer with measured latency and underrun counters.
6. Add explicit connection states for idle, discoverable, paired, streaming, and recovery after disconnect.
7. Test concurrent audio playback and dual-eye rendering, then tune buffering and task/core placement from measured results.
8. Document the supported phones, pairing/reset workflow, device name, and final test evidence.

## Acceptance Criteria

- The Pico 2 W is discoverable by a current phone as a Bluetooth speaker using Bluetooth Classic.
- A paired phone reconnects after the board is restarted, according to the chosen pairing policy.
- Stereo channel orientation is correct at the Adafruit I2S amplifier.
- SBC audio plays continuously for 10 minutes without audible dropouts, periodic clicks, or display stalls while both eyes animate.
- Disconnect, source pause, and reconnect recover without a firmware reset or resource leak.
- Buffer underruns, queue overflows, connection failures, and decode failures are observable through lightweight counters or a compile-time-selectable diagnostic.
- The firmware builds with the VS Code `Compile Project` task and is tested on physical hardware using the `Run Project` or `Flash` task.

## Out of Scope

- BLE Audio/LE Audio, microphone input, hands-free calling, multi-phone streaming, audio-file playback, equalization, AVRCP controls, and codecs beyond SBC are deferred from the first release.
