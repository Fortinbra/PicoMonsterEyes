# Eye assets plan

The CreeperEyes repo embeds eye images as C/C++ header arrays (`defaultEye.h`, `newtEye.h`) used by the Adafruit SSD1351 renderer. These include maps for sclera, iris (polar map), eyelid thresholds, etc., not simple PNGs. We'll extract or reinterpret those into formats suited for Pico SDK and our SSD1351 driver.

Options:

1) Use original header arrays as-is (license-permitting) and adapt our renderer to their layout (sclera/iris/upper/lower maps). This yields minimal conversion but requires porting drawEye logic.

2) Convert to static 128×128 RGB565 eye frames for an initial bring-up, then incrementally add procedural motion.

Recommended path:

- Start with (2) for a quick splash image: generate a single 128×128 RGB565 buffer.
- Later, implement (1) to support eyelids/pupil motion using the existing map structures.

Licensing: CreeperEyes cites MIT license; confirm in the repo LICENSE before copying assets. Preserve attribution.
