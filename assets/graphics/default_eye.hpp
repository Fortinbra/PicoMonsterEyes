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
//  - Initially only the sclera was wrapped. The iris map is now also exposed.
//  - Remaining arrays (polar distortion map, eyelid threshold maps) will follow
//    once dynamic motion & blinking are implemented.
//
#pragma once
#include <cstdint>

// Dimensions from original file
#define PME_SCLERA_WIDTH      200
#define PME_SCLERA_HEIGHT     200

// Iris source map (polar/radial color data) dimensions from upstream file.
// The upstream code maps an (IRIS_WIDTH x IRIS_HEIGHT) circle using this
// unwrapped iris color table. We will perform a simple polar lookup for now.
#define PME_IRIS_MAP_WIDTH    256
#define PME_IRIS_MAP_HEIGHT   64

// Final rendered iris bounding box (a circle inscribed in this square).
#define PME_IRIS_WIDTH        80
#define PME_IRIS_HEIGHT       80

// Eyelid mask dimensions (128x128 screen). Values are 0-255 threshold maps.
#define PME_EYELID_WIDTH      128
#define PME_EYELID_HEIGHT     128

// Accessor to underlying sclera image (returns a reference to 2D array)
const uint16_t (&get_sclera())[PME_SCLERA_HEIGHT][PME_SCLERA_WIDTH];

// Accessor to underlying iris color map (radial rows * angular columns)
const uint16_t (&get_iris_map())[PME_IRIS_MAP_HEIGHT][PME_IRIS_MAP_WIDTH];

// Eyelid threshold maps (0 transparent -> 255 fully covered). Upper covers from top downward,
// lower covers from bottom upward. Taken from upstream 'upper' and 'lower' arrays when
// SYMMETRICAL_EYELID is defined.
const uint8_t (&get_upper_eyelid())[PME_EYELID_HEIGHT][PME_EYELID_WIDTH];
const uint8_t (&get_lower_eyelid())[PME_EYELID_HEIGHT][PME_EYELID_WIDTH];

// Large data are defined in default_eye.cpp via upstream include; we only expose accessors here.
