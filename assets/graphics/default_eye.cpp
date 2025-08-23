// Sclera asset wrapper: alias to Adafruit Uncanny Eyes defaultEye sclera table (MIT License). (touched)
// Source: external/Uncanny_Eyes/uncannyEyes/graphics/defaultEye.h
// We intentionally avoid duplicating the ~200x200 uint16_t array to keep the repository
// lean and compilation fast. Other assets (iris, polar map, eyelid maps) will be
// wrapped similarly in their own translation units.

#include "default_eye.hpp"
#include "../../external/Uncanny_Eyes/uncannyEyes/graphics/defaultEye.h"

static_assert(PME_SCLERA_WIDTH == SCLERA_WIDTH && PME_SCLERA_HEIGHT == SCLERA_HEIGHT,
              "Sclera dimension mismatch; adjust PME_SCLERA_* if upstream asset changes");
static_assert(PME_IRIS_MAP_WIDTH == IRIS_MAP_WIDTH && PME_IRIS_MAP_HEIGHT == IRIS_MAP_HEIGHT,
              "Iris map dimension mismatch; adjust PME_IRIS_MAP_* if upstream asset changes");
static_assert(PME_EYELID_WIDTH == SCREEN_WIDTH && PME_EYELID_HEIGHT == SCREEN_HEIGHT,
              "Eyelid dimension mismatch; adjust PME_EYELID_* if upstream asset changes");

// Accessor returns reference to underlying asset data (no copy)
const uint16_t (&get_sclera())[PME_SCLERA_HEIGHT][PME_SCLERA_WIDTH] {
    return sclera;
}

// Iris map accessor
const uint16_t (&get_iris_map())[PME_IRIS_MAP_HEIGHT][PME_IRIS_MAP_WIDTH] {
    return iris;
}

// Eyelid maps
const uint8_t (&get_upper_eyelid())[PME_EYELID_HEIGHT][PME_EYELID_WIDTH] { return upper; }
const uint8_t (&get_lower_eyelid())[PME_EYELID_HEIGHT][PME_EYELID_WIDTH] { return lower; }

// Clean up imported macros to avoid accidental downstream reliance.
#undef SCLERA_WIDTH
#undef SCLERA_HEIGHT
#undef IRIS_MAP_WIDTH
#undef IRIS_MAP_HEIGHT
#undef IRIS_WIDTH
#undef IRIS_HEIGHT
#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT
