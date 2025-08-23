// Default eye graphics imported from Adafruit Uncanny Eyes
// Source: https://github.com/adafruit/uncanny_eyes/blob/d8a7b2c/uncannyEyes/graphics/defaultEye.h
// Commit: d8a7b2cab5f4c4c89e9a66f16e0291acc844e1ae
// License: MIT (Adafruit Industries). Original license header retained below if present.
//
// Notes:
//  - The original file defines large PROGMEM-style tables for Arduino. Here we keep
//    the same data layout but rename symbols with a project-local prefix to avoid
//    potential global collisions and to allow multiple eye asset sets in the future.
//  - Data are 16-bit RGB565 big-endian values (as used by the original project).
//  - Only a subset (sclera array) is imported now for initial static rendering.
//  - Additional arrays (iris, upper/lower eyelid threshold maps, polar map) will
//    be imported when implementing dynamic iris scaling & blinking.
//
// TODO: Import the remaining arrays: iris, polar, upper, lower, along with the
//       IRIS_* constants and any required dimension macros.
//
#pragma once
#include <cstdint>

// Dimensions from original file
#define PME_SCLERA_WIDTH  200
#define PME_SCLERA_HEIGHT 200

extern const uint16_t pme_sclera[PME_SCLERA_HEIGHT][PME_SCLERA_WIDTH];

// Truncated data declaration: full data will be provided in a dedicated .cpp to
// keep compile units smaller and avoid huge header parses.
