// Eye rendering implementation
#include "eye_renderer.hpp"
#include <algorithm>
#include <cmath>

namespace eyes {

static inline float fast_clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Internal helpers forward
static void render_eye_base_impl(uint16_t *frame, const EyeRenderParams &p);
static void apply_eyelids_impl(uint16_t *frame, const EyeRenderParams &p);

void render_eye_base(uint16_t *frame, const EyeRenderParams &p) { render_eye_base_impl(frame,p); }
void apply_eyelids(uint16_t *frame, const EyeRenderParams &p) { apply_eyelids_impl(frame,p); }

void render_eye(uint16_t *frame, const EyeRenderParams &p) {
    render_eye_base_impl(frame,p);
    apply_eyelids_impl(frame,p);
}

// Precomputed LUT containers (persist across frames)
namespace {
    // Max supported iris radius (fits inside 128x128) safeguard
    constexpr int kMaxIrisR = 64; // since PME_IRIS_WIDTH=80 we only need ~40, keep some headroom
    // rsq -> row index
    static uint8_t g_rsq_to_row[(kMaxIrisR+1)*(kMaxIrisR+1)+1];
    static float   g_last_iris_r = -1.f;
    static int     g_last_r_int = -1;
    // Angle: we quantize atan2 to PME_IRIS_MAP_WIDTH columns using octant symmetry.
    // We'll build a dy* (2R+1) table for current radius to avoid atan2 inside loop.
    static uint16_t g_angle_col[ (kMaxIrisR*2+1) * (kMaxIrisR*2+1) ]; // store column index or 0xFFFF if outside circle
    static float g_angle_last_r = -1.f;
    // Highlight falloff LUT (0..1 distance fraction -> blend factor) 256 entries
    static bool g_highlight_lut_init = false;
    static uint8_t g_highlight_primary_lut[256]; // value scaled 0..255
    static uint8_t g_highlight_secondary_lut[256];
    // Distance^2 -> primary/secondary blend (0..255) for current radii (avoids per-pixel sqrt)
    static float g_last_hR = -1.f;
    static float g_last_sR = -1.f;
    static int g_hR_int_sq = 0; // last primary radius int squared
    static int g_sR_int_sq = 0; // last secondary radius int squared
    static uint8_t g_highlight_primary_rsq[(kMaxIrisR+1)*(kMaxIrisR+1)+1];
    static uint8_t g_highlight_secondary_rsq[(kMaxIrisR+1)*(kMaxIrisR+1)+1];
}

static inline void ensure_highlight_luts() {
    if (g_highlight_lut_init) return;
    g_highlight_lut_init = true;
    for (int i=0;i<256;++i) {
        float t = (float)i / 255.f; // t = distance fraction (0 center ->1 edge)
        float fall = 1.f - t;
        if (fall < 0.f) fall = 0.f; else if (fall > 1.f) fall = 1.f;
        float sm = fall*fall*(3-2*fall); // smoothstep-ish
        float sm2 = fall*fall; // sharper for secondary
        g_highlight_primary_lut[i] = (uint8_t)(sm * 255.f + 0.5f);
        g_highlight_secondary_lut[i] = (uint8_t)(sm2 * 255.f + 0.5f);
    }
}

static inline void build_radius_lut(float iris_r) {
    int r_int = (int)(iris_r + 0.5f);
    if (r_int > kMaxIrisR) r_int = kMaxIrisR;
    if (iris_r == g_last_iris_r) return;
    g_last_iris_r = iris_r; g_last_r_int = r_int;
    float inv_r = 1.f / iris_r;
    for (int rsq=0; rsq <= r_int*r_int; ++rsq) {
        float r = std::sqrt((float)rsq) * inv_r;
        if (r>1.f) r=1.f;
        int row = (int)(r * (PME_IRIS_MAP_HEIGHT - 1) + 0.5f);
        if (row<0) row=0; else if (row>PME_IRIS_MAP_HEIGHT-1) row = PME_IRIS_MAP_HEIGHT-1;
        g_rsq_to_row[rsq] = (uint8_t)row;
    }
}

static inline void build_angle_lut(float iris_r) {
    if (iris_r == g_angle_last_r) return;
    g_angle_last_r = iris_r;
    int r_int = (int)(iris_r + 0.5f); if (r_int > kMaxIrisR) r_int = kMaxIrisR;
    int dim = r_int*2 + 1;
    for (int y=-r_int; y<=r_int; ++y) {
        for (int x=-r_int; x<=r_int; ++x) {
            int idx = (y + r_int) * (kMaxIrisR*2+1) + (x + r_int);
            int rsq = x*x + y*y;
            if (rsq > r_int*r_int) { g_angle_col[idx] = 0xFFFF; continue; }
            float ang = std::atan2((float)y,(float)x); // executed once per lattice build (rare)
            float ang_norm = (ang + 3.14159265358979323846f) * (1.f / (2.f * 3.14159265358979323846f));
            int col = (int)(ang_norm * (PME_IRIS_MAP_WIDTH - 1) + 0.5f);
            if (col<0) col=0; else if (col>PME_IRIS_MAP_WIDTH-1) col=PME_IRIS_MAP_WIDTH-1;
            g_angle_col[idx] = (uint16_t)col;
        }
    }
}

static inline void build_highlight_rsq_luts(float hR, float sR) {
    // Clamp radii into supported range
    if (hR < 0.f) hR = 0.f; if (sR < 0.f) sR = 0.f;
    if (hR > kMaxIrisR) hR = kMaxIrisR; if (sR > kMaxIrisR) sR = kMaxIrisR;
    if (hR == g_last_hR && sR == g_last_sR) return;
    g_last_hR = hR; g_last_sR = sR;
    int hRi = (int)(hR + 0.5f);
    int sRi = (int)(sR + 0.5f);
    g_hR_int_sq = hRi * hRi;
    g_sR_int_sq = sRi * sRi;
    if (hRi > 0) {
        float inv_hR = hR > 0.f ? (1.f / hR) : 0.f;
        for (int rsq = 0; rsq <= g_hR_int_sq; ++rsq) {
            float d = std::sqrt((float)rsq) * inv_hR; if (d>1.f) d=1.f; if (d<0.f) d=0.f;
            int li = (int)(d * 255.f + 0.5f); if (li>255) li=255; if (li<0) li=0;
            g_highlight_primary_rsq[rsq] = g_highlight_primary_lut[li];
        }
    }
    if (sRi > 0) {
        float inv_sR = sR > 0.f ? (1.f / sR) : 0.f;
        for (int rsq = 0; rsq <= g_sR_int_sq; ++rsq) {
            float d = std::sqrt((float)rsq) * inv_sR; if (d>1.f) d=1.f; if (d<0.f) d=0.f;
            int li = (int)(d * 255.f + 0.5f); if (li>255) li=255; if (li<0) li=0;
            g_highlight_secondary_rsq[rsq] = g_highlight_secondary_lut[li];
        }
    }
}

static void render_eye_base_impl(uint16_t *frame, const EyeRenderParams &p) {
    auto &sclera = get_sclera();
    // Sclera parallax: ensure sclera texture tracks WITH iris motion (rigid eyeball feel).
    const int marginX = (PME_SCLERA_WIDTH - p.frame_w) / 2; // e.g. 36
    const int marginY = (PME_SCLERA_HEIGHT - p.frame_h) / 2; // e.g. 36
    int relX = p.iris_center_x - (p.frame_w / 2); // positive when iris right
    int relY = p.iris_center_y - (p.frame_h / 2);
    float parallax = p.sclera_parallax;
    if (parallax < 0.f) parallax = 0.f; else if (parallax > 1.f) parallax = 1.f;
    // Move sclera in opposite direction to iris movement for natural eyeball rotation illusion (uncomment next line to move same direction)
    // parallax = -parallax;
    // Previous implementation moved in the opposite perceived direction; invert sign so texture tracks iris.
    float offXf = -(float)relX * parallax;
    float offYf = -(float)relY * parallax;
    int x0 = marginX + (int)std::lround(offXf);
    int y0 = marginY + (int)std::lround(offYf);
    if (x0 < 0) x0 = 0; else if (x0 > marginX * 2) x0 = marginX * 2;
    if (y0 < 0) y0 = 0; else if (y0 > marginY * 2) y0 = marginY * 2;
    for (int y = 0; y < p.frame_h; ++y) {
        const uint16_t *srcRow = &sclera[y0 + y][x0];
        uint16_t *dst = frame + y * p.frame_w;
        for (int x = 0; x < p.frame_w; ++x) dst[x] = srcRow[x];
    }

    // Iris + pupil + highlights + optional tint (integrated)
    auto &irisMap = get_iris_map();
    const float iris_r = p.iris_radius;
    build_radius_lut(iris_r);
    build_angle_lut(iris_r);
    ensure_highlight_luts();
    float pupil_r = p.base_pupil_fraction * iris_r * fast_clamp(p.pupil_scale, 0.1f, 2.0f);
    if (pupil_r < 0.f) pupil_r = 0.f;
    float pupil_r_sq = pupil_r * pupil_r;
    int r_int = g_last_r_int;
    float ts = (p.tint_enabled && p.tint_strength > 0.f) ? fast_clamp(p.tint_strength, 0.f, 1.f) : 0.f;
    int tr5 = (p.tint_color >> 11) & 0x1F;
    int tg6 = (p.tint_color >> 5) & 0x3F;
    int tb5 = p.tint_color & 0x1F;
    // Precompute highlight rsq LUTs once per radius change
    float hR = p.highlight_radius_frac * iris_r;
    float sR = p.highlight2_radius_frac * iris_r;
    build_highlight_rsq_luts(hR, sR);
    float hR2 = hR*hR;
    float sR2 = sR*sR;
    float hx = p.highlight_offset_x_frac * iris_r;
    float hy = p.highlight_offset_y_frac * iris_r;
    float sx = p.highlight_offset_x_frac * p.highlight2_offset_scale * iris_r;
    float sy = p.highlight_offset_y_frac * p.highlight2_offset_scale * iris_r;
    bool do_highlight = p.highlight_enabled && (p.highlight_strength > 0.f);
    bool do_secondary = do_highlight && p.highlight_secondary;
    int hr5 = (p.highlight_color >> 11) & 0x1F;
    int hg6 = (p.highlight_color >> 5) & 0x3F;
    int hb5 = p.highlight_color & 0x1F;

    for (int dy=-r_int; dy<=r_int; ++dy) {
        int fy = p.iris_center_y + dy; if ((unsigned)fy >= (unsigned)p.frame_h) continue;
        for (int dx=-r_int; dx<=r_int; ++dx) {
            int fx = p.iris_center_x + dx; if ((unsigned)fx >= (unsigned)p.frame_w) continue;
            int rsq = dx*dx + dy*dy; if (rsq > r_int*r_int) continue;
            bool inPupil = (float)rsq <= pupil_r_sq;
            uint16_t color;
            if (inPupil) {
                color = 0x0000;
            } else {
                int iris_row = g_rsq_to_row[rsq];
                int aidx = (dy + r_int) * (kMaxIrisR*2+1) + (dx + r_int);
                uint16_t colIdx = g_angle_col[aidx];
                if (colIdx == 0xFFFF) continue; // outside
                color = irisMap[iris_row][colIdx];
            }
            // Highlights
            if (do_highlight && (p.highlight_over_pupil || !inPupil)) {
                float hdx = dx - hx; float hdy = dy - hy;
                float distP2 = hdx*hdx + hdy*hdy; float blend = 0.f;
                if (distP2 < hR2 && hR2 > 0.f) {
                    int rsqi = (int)(distP2 + 0.5f); if (rsqi < 0) rsqi = 0; if (rsqi > g_hR_int_sq) rsqi = g_hR_int_sq;
                    blend = (g_highlight_primary_rsq[rsqi] / 255.f) * p.highlight_strength;
                }
                if (do_secondary) {
                    float sdx = dx - sx; float sdy = dy - sy; float distS2 = sdx*sdx + sdy*sdy;
                    if (distS2 < sR2 && sR2 > 0.f) {
                        int rsqi = (int)(distS2 + 0.5f); if (rsqi < 0) rsqi = 0; if (rsqi > g_sR_int_sq) rsqi = g_sR_int_sq;
                        float b2 = (g_highlight_secondary_rsq[rsqi] / 255.f) * p.highlight_strength;
                        if (b2 > blend) blend = b2;
                    }
                }
                if (blend > 0.f) {
                    int r5 = (color >> 11) & 0x1F;
                    int g6 = (color >> 5) & 0x3F;
                    int b5 = color & 0x1F;
                    r5 = (int)(r5 + (hr5 - r5) * blend + 0.5f);
                    g6 = (int)(g6 + (hg6 - g6) * blend + 0.5f);
                    b5 = (int)(b5 + (hb5 - b5) * blend + 0.5f);
                    color = (uint16_t)((r5<<11)|(g6<<5)|b5);
                }
            }
            // Tint
            if (ts > 0.f) {
                int r5 = (color >> 11) & 0x1F; int g6 = (color >> 5) & 0x3F; int b5 = color & 0x1F;
                r5 = (int)(r5 + (tr5 - r5) * ts + 0.5f);
                g6 = (int)(g6 + (tg6 - g6) * ts + 0.5f);
                b5 = (int)(b5 + (tb5 - b5) * ts + 0.5f);
                color = (uint16_t)((r5<<11)|(g6<<5)|b5);
            }
            frame[fy * p.frame_w + fx] = color;
        }
    }
}

static void apply_eyelids_impl(uint16_t *frame, const EyeRenderParams &p) {
    float open = fast_clamp(p.eyelid_open, 0.f, 1.f);
    auto &upperMap = get_upper_eyelid();
    auto &lowerMap = get_lower_eyelid();
    float base_edge = (float)p.eyelid_edge_base;
    float cutoff = base_edge + (1.f - open) * (255.f - base_edge);
    uint16_t topColor = p.eyelid_color_top;
    uint16_t botColor = p.eyelid_color_bottom;
    for (int y = 0; y < p.frame_h; ++y) {
        uint16_t *row = frame + y * p.frame_w;
        float row_adjust = 0.f;
        if (p.upper_shape_adjust || p.lower_shape_adjust) {
            if (p.upper_shape_adjust) row_adjust = (float)p.upper_shape_adjust[y];
            if (p.lower_shape_adjust) row_adjust += (float)p.lower_shape_adjust[y];
        }
        float row_cutoff = cutoff + row_adjust;
        if (row_cutoff < 0.f) row_cutoff = 0.f; else if (row_cutoff > 255.f) row_cutoff = 255.f;
        if (!p.mirror_eyelids) {
            for (int x = 0; x < p.frame_w; ++x) {
                uint8_t u = upperMap[y][x];
                uint8_t l = lowerMap[y][x];
                bool coverTop = u <= row_cutoff;
                bool coverBottom = l <= row_cutoff;
                if (coverTop || coverBottom) {
                    row[x] = (coverTop && coverBottom) ? botColor : (coverTop ? topColor : botColor);
                }
            }
        } else {
            for (int x = 0; x < p.frame_w; ++x) {
                int mx = p.frame_w - 1 - x;
                uint8_t u = upperMap[y][mx];
                uint8_t l = lowerMap[y][mx];
                bool coverTop = u <= row_cutoff;
                bool coverBottom = l <= row_cutoff;
                if (coverTop || coverBottom) {
                    row[x] = (coverTop && coverBottom) ? botColor : (coverTop ? topColor : botColor);
                }
            }
        }
    }
}

} // namespace eyes
