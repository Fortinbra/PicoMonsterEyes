// Eye rendering helper (static prototype with pupil dilation)
#pragma once
#include <cstdint>
#include "default_eye.hpp"

namespace eyes {

struct EyeRenderParams {
    int frame_w = 128;
    int frame_h = 128;
    int iris_center_x = 64;
    int iris_center_y = 64;
    float iris_radius = PME_IRIS_WIDTH * 0.5f;   // pixels
    float base_pupil_fraction = 0.30f;           // baseline fraction of iris radius
    float pupil_scale = 1.0f;                    // dynamic multiplier (animation/dilation)
    float eyelid_open = 1.0f;                    // 1 = fully open, 0 = fully closed
    uint8_t eyelid_edge_base = 2;                // thinner baseline eyelid edge when fully open
    float sclera_parallax = 0.0f;                // locked (no sclera motion)
    // Eyelid colors (RGB565). Both black per request.
    uint16_t eyelid_color_top = 0x0000;           // black
    uint16_t eyelid_color_bottom = 0x0000;        // black
    // Specular highlight (glint) parameters
    bool highlight_enabled = true;               // master toggle
    bool highlight_secondary = true;             // enable secondary small glint
    bool highlight_over_pupil = true;            // allow highlight to appear over pupil region
    float highlight_radius_frac = 0.18f;         // primary highlight radius as fraction of iris radius
    float highlight_offset_x_frac = -0.25f;      // relative offset (negative left)
    float highlight_offset_y_frac = -0.25f;      // relative offset (negative up)
    float highlight_strength = 1.0f;             // 0..1 blend factor
    uint16_t highlight_color = 0xFFFF;           // RGB565 white
    float highlight2_radius_frac = 0.06f;        // secondary highlight radius fraction
    float highlight2_offset_scale = 0.55f;       // secondary placed closer to center
    // Emotion / style tint (applied to sclera+iris before eyelids). strength 0..1
    bool tint_enabled = false;
    uint16_t tint_color = 0;                     // RGB565
    float tint_strength = 0.0f;                  // 0=no tint
    // Per-row eyelid shape adjustment (signed additive to cutoff). nullptr if unused.
    const int8_t* upper_shape_adjust = nullptr;  // length frame_h
    const int8_t* lower_shape_adjust = nullptr;  // length frame_h
    // Horizontal mirroring of eyelid masks (use for right eye so medial canthus faces center).
    bool mirror_eyelids = false;
};

// Renders sclera + iris + pupil (dilated) into frame (RGB565). frame length = frame_w*frame_h.
// pupil_scale in params modifies pupil radius: final_pupil_r = base_pupil_fraction * iris_radius * clamp(pupil_scale,0.1..2.0)
void render_eye(uint16_t *frame, const EyeRenderParams &params);

// Performance path: render everything EXCEPT eyelids into frame (sclera, iris, pupil, highlights, optional tint)
// Eyelids can then be applied differently per eye without re-doing base work.
void render_eye_base(uint16_t *frame, const EyeRenderParams &params);

// Apply only eyelids (uses eyelid_open, shape arrays, colors, mirror_eyelids). Leaves other pixels intact.
void apply_eyelids(uint16_t *frame, const EyeRenderParams &params);

} // namespace eyes
