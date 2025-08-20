# Assets

Original eye assets (sclera, iris, eyelids, polar maps) are maintained by Adafruit in Uncanny Eyes and reused in community forks.

- Adafruit Uncanny Eyes: [github.com/adafruit/Uncanny_Eyes](https://github.com/adafruit/Uncanny_Eyes)
- CreeperEyes (ESP32 + SSD1351): [github.com/tooluser/CreeperEyes](https://github.com/tooluser/CreeperEyes)

Guidance:
 
- When importing any headers (e.g., `defaultEye.h`, `newtEye.h`), keep the original MIT license header and attribution at the top of each file.
- Place copied headers under `assets/` in this repo.
- Optionally add a `README` in `assets/` listing the source commit and link.

Conversion notes:

- The Adafruit/CreeperEyes renderer expects specific map shapes (sclera image, iris polar map, upper/lower eyelid masks). We can either:
  - Port the draw logic to our driver and use the maps directly, or
  - Precompose a 128×128 RGB565 frame as a splash image for bring-up.

We’ll start with a static splash to validate display IO, then port the full map-driven renderer.
