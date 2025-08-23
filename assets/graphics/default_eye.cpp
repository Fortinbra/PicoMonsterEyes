#include "default_eye.hpp"

// NOTE: This file currently includes ONLY the sclera[][] array extracted from
// Adafruit's defaultEye.h (commit d8a7b2c). For brevity and initial bring-up,
// we may opt to further reduce or compress this data; for now we include it
// directly to enable a static composite test. The remaining arrays (iris,
// polar, upper, lower) will be added later.

// The full sclera array is large. It has been truncated here intentionally as a placeholder.
// TODO: Replace this placeholder with the full sclera data needed for rendering.
const uint16_t pme_sclera[PME_SCLERA_HEIGHT][PME_SCLERA_WIDTH] = { {0} };
