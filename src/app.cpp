#include "app.hpp"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "boards/pico2_pins.hpp"
#include "drivers/spi_bus.hpp"
#include "drivers/ssd1351_display.hpp"
#include "default_eye.hpp"
#include "eye_renderer.hpp"
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstring>

namespace eyes {

// Persistent hardware singletons inside this translation unit
namespace {
    // Provide local clamp fallback in case toolchain lacks std::clamp (even though not used now)
    template<typename T>
    static inline T clamp_fallback(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
    SpiBus* g_spi = nullptr;
    Ssd1351Display* g_left = nullptr;
    Ssd1351Display* g_right = nullptr;

    // Per-emotion eyelid shape adjustment arrays (int8 per row, 0 = no change).
    // Positive values LOWER upper lid (more closed) and RAISE lower lid (more closed)
    // because they increase the coverage threshold; negatives do the opposite (more open).
    static int8_t upper_neutral[128];
    static int8_t lower_neutral[128];
    static int8_t upper_sad[128];
    static int8_t lower_sad[128];
    static int8_t upper_fear[128];
    static int8_t lower_fear[128];
    static int8_t upper_anger[128];
    static int8_t lower_anger[128];
    static int8_t upper_disgust[128];
    static int8_t lower_disgust[128];

    bool shapes_inited = false;

    void init_emotion_shapes() {
        if (shapes_inited) return;
        shapes_inited = true;
        for (int y = 0; y < 128; ++y) {
            // Neutral all zero
            upper_neutral[y] = 0; lower_neutral[y] = 0;
            // Base ramps 0..1 top->mid and bottom->mid
            float topFrac = (y < 64) ? (1.f - (float)y / 64.f) : 0.f; // 1 at row0 -> 0 at 64
            float botFrac = (y >= 64) ? ((float)(y - 64) / 64.f) : 0.f; // 0 at 64 ->1 at 127

            // Sad: drooped upper lid moderately (+), slight raise lower (+ small)
            upper_sad[y] = (int8_t)(topFrac * 12.f); // up to +12 at top
            lower_sad[y] = (int8_t)(botFrac * 4.f);  // up to +4 at bottom

            // Fear: retracted upper (negative), retracted lower (negative)
            upper_fear[y] = (int8_t)(-topFrac * 15.f); // -15 to 0
            lower_fear[y] = (int8_t)(-botFrac * 10.f); // -10 to 0

            // Anger: lowered upper strongly (+), lower near neutral slight raise (+ small)
            upper_anger[y] = (int8_t)(topFrac * 18.f); // +18 top
            lower_anger[y] = (int8_t)(botFrac * 3.f);  // +3 bottom

            // Disgust: slight upper raise (negative small), lower raise (positive moderate)
            upper_disgust[y] = (int8_t)(-topFrac * 6.f);
            lower_disgust[y] = (int8_t)(botFrac * 8.f);
        }
    }
}

bool App::init() {
    // SPI pin mux
    gpio_set_function(pins::spi0_sck,  GPIO_FUNC_SPI);
    gpio_set_function(pins::spi0_mosi, GPIO_FUNC_SPI);
    if (pins::spi0_miso != 0xFF) gpio_set_function(pins::spi0_miso, GPIO_FUNC_SPI);

    static SpiBus spi(spi0, 16 * 1000 * 1000);
    spi.init();
    // Try boosting SPI clock (panel often tolerates >16MHz). Step up to 30MHz.
    spi.set_frequency(30 * 1000 * 1000);
    g_spi = &spi;

    static Ssd1351Display left(spi, 128, 128, pins::left_cs,  pins::left_dc,  pins::left_res);
    static Ssd1351Display right(spi,128, 128, pins::right_cs, pins::right_dc, pins::right_res);
    if (!left.init() || !right.init()) return false;
    g_left = &left;
    g_right = &right;
    left_ = g_left;
    right_ = g_right;

    Rect full{0,0,kFrameW,kFrameH};
    params_left_ = EyeRenderParams{};
    params_right_ = EyeRenderParams{};
    // Mirror eyelids for LEFT eye so medial canthus (already on left side of mask) faces inward between displays.
    params_left_.mirror_eyelids = true;
    params_right_.mirror_eyelids = false;
    init_emotion_shapes();
    render_eye(frame_, params_left_);
    left_->blit(frame_, full);
    render_eye(frame_, params_right_);
    right_->blit(frame_, full);
    return true;
}

void App::choose_new_target() {
    // Constrain target so full iris stays on screen.
    int minC = (int)params_left_.iris_radius;
    int maxC = kFrameW - minC;
    // Biased sampling: favor central region slightly.
    auto sample_axis = [&](int minV, int maxV) {
        float r = rand01();
        // Smoothstep bias toward 0.5
        float b = r*r*(3 - 2*r);
        return (float)minV + b * (float)(maxV - minV);
    };
    gaze_sx_ = gaze_cx_;
    gaze_sy_ = gaze_cy_;
    gaze_tx_ = sample_axis(minC, maxC);
    gaze_ty_ = sample_axis(minC, maxC);
    // Saccade duration: small angle -> shorter jump.
    float dx = gaze_tx_ - gaze_sx_;
    float dy = gaze_ty_ - gaze_sy_;
    float dist = std::sqrt(dx*dx + dy*dy);
    saccade_duration_ = 0.04f + 0.06f * (dist / 24.f); // 40-100ms typical
    if (saccade_duration_ > 0.12f) saccade_duration_ = 0.12f;
    saccade_timer_ = 0.f;
}

void App::advance_emotion() {
    prev_emotion_ = emotion_;
    int idx = static_cast<int>(emotion_);
    idx = (idx + 1) % static_cast<int>(Emotion::COUNT);
    emotion_ = static_cast<Emotion>(idx);
    emotion_timer_ = 0.f;
    emotion_fade_ = 0.f; // restart fade
}

void App::loop() {
    Rect full{0,0,kFrameW,kFrameH};
    while (true) {
    // Real time delta using hardware timer
    if (!last_time_us_) last_time_us_ = time_us_64();
    uint64_t now_us = time_us_64();
    float dt = (now_us - last_time_us_) * 1e-6f;
        if (dt <= 0.f) dt = 0.0005f; // guard
        t_ += dt;
    last_time_us_ = now_us;
        // Emotion cycling timer
        emotion_timer_ += 0.02f;
        if (emotion_timer_ >= emotion_cycle_len_) {
            advance_emotion();
        }

        // Advance cross-fade
        if (emotion_fade_ < 1.f) {
            emotion_fade_ += 0.02f / emotion_fade_duration_;
            if (emotion_fade_ > 1.f) emotion_fade_ = 1.f;
        }

        // Emotion influences (modulate parameters heuristically) computed for both prev and current to blend:
        // Sad: slower saccades, longer fixations, narrower pupil, half-lidded
        // Fear: rapid small saccades, shorter fixations, dilated pupil, eyelids more open (wider)
        // Anger: focused shorter fixations, medium-fast saccades, slight constrict, upper lid lowered
        // Disgust: biased upward gaze, moderate speed, some constrict, slight upper lid raise and lower lid raise.
        struct EmoParams { float fix_scale, sacc_scale, pupil_bias, eyelid_bias, gaze_bx, gaze_by; uint16_t tint_col; float tint_strength; int8_t* upper; int8_t* lower; bool tint_on; };
        auto compute = [&](Emotion e){
            EmoParams ep{1.f,1.f,0.f,0.f,0.f,0.f,0,0.f, upper_neutral, lower_neutral,false};
            switch(e){
                case Emotion::Sad:
                    ep.fix_scale=1.6f; ep.sacc_scale=0.6f; ep.pupil_bias=-0.1f; ep.eyelid_bias=-0.25f; ep.gaze_by=4.f; ep.tint_on=true; ep.tint_col=0x4210; ep.tint_strength=0.15f; ep.upper=upper_sad; ep.lower=lower_sad; break;
                case Emotion::Fear:
                    ep.fix_scale=0.6f; ep.sacc_scale=1.4f; ep.pupil_bias=+0.18f; ep.eyelid_bias=+0.15f; ep.gaze_by=-3.f; ep.tint_on=true; ep.tint_col=0x57FF; ep.tint_strength=0.18f; ep.upper=upper_fear; ep.lower=lower_fear; break;
                case Emotion::Anger:
                    ep.fix_scale=0.8f; ep.sacc_scale=1.2f; ep.pupil_bias=-0.05f; ep.eyelid_bias=-0.10f; ep.gaze_bx=+2.f; ep.tint_on=true; ep.tint_col=0xF880; ep.tint_strength=0.22f; ep.upper=upper_anger; ep.lower=lower_anger; break;
                case Emotion::Disgust:
                    ep.fix_scale=1.1f; ep.sacc_scale=0.9f; ep.pupil_bias=-0.07f; ep.eyelid_bias=-0.05f; ep.gaze_by=-4.f; ep.tint_on=true; ep.tint_col=0x07E0; ep.tint_strength=0.20f; ep.upper=upper_disgust; ep.lower=lower_disgust; break;
                case Emotion::Neutral: default:
                    break;
            }
            return ep;
        };
        EmoParams prevp = compute(prev_emotion_);
        EmoParams curp  = compute(emotion_);
        // Apply smootherstep easing to emotion fade for more natural transitions
        float f = emotion_fade_;
        {
            float x = f; // smootherstep (quintic) 6x^5 -15x^4 +10x^3
            f = x * x * x * (x * (x * 6.f - 15.f) + 10.f);
        }
        auto lerp = [&](float a,float b){return a + (b-a)*f;};
        float emotion_fixation_scale = lerp(prevp.fix_scale, curp.fix_scale);
        float emotion_saccade_speed_scale = lerp(prevp.sacc_scale, curp.sacc_scale);
        float emotion_pupil_bias = lerp(prevp.pupil_bias, curp.pupil_bias);
        float eyelid_open_bias = lerp(prevp.eyelid_bias, curp.eyelid_bias);
        float gaze_bias_x = lerp(prevp.gaze_bx, curp.gaze_bx);
        float gaze_bias_y = lerp(prevp.gaze_by, curp.gaze_by);
        // Blend tint: if either has tint, enable and blend color in RGB565 space component-wise.
        if (prevp.tint_on || curp.tint_on) {
            params_left_.tint_enabled = true; params_right_.tint_enabled = true;
            // Extract components
            int pr = (prevp.tint_col >> 11) & 0x1F; int pg = (prevp.tint_col >> 5) & 0x3F; int pb = prevp.tint_col & 0x1F;
            int cr = (curp.tint_col >> 11) & 0x1F; int cg = (curp.tint_col >> 5) & 0x3F; int cb = curp.tint_col & 0x1F;
            int r = (int)(pr + (cr - pr) * f + 0.5f);
            int g = (int)(pg + (cg - pg) * f + 0.5f);
            int b = (int)(pb + (cb - pb) * f + 0.5f);
            if (r<0) r=0; if(r>31) r=31; if(g<0) g=0; if(g>63) g=63; if(b<0) b=0; if(b>31) b=31;
            uint16_t blend_col = (uint16_t)((r<<11)|(g<<5)|b);
            float blend_str = lerp(prevp.tint_strength, curp.tint_strength);
            params_left_.tint_color = blend_col; params_right_.tint_color = blend_col;
            params_left_.tint_strength = blend_str; params_right_.tint_strength = blend_str;
        } else {
            params_left_.tint_enabled = false; params_right_.tint_enabled = false; params_left_.tint_strength = 0.f; params_right_.tint_strength = 0.f;
        }
        // Shape blend: create temp blended arrays (static to avoid stack) and point to them.
        static int8_t upper_blend[128];
        static int8_t lower_blend[128];
        for (int y=0;y<128;++y){
            float u = prevp.upper[y] + (curp.upper[y]-prevp.upper[y])*f;
            float l = prevp.lower[y] + (curp.lower[y]-prevp.lower[y])*f;
            if (u < -128.f) u = -128.f; if (u > 127.f) u = 127.f;
            if (l < -128.f) l = -128.f; if (l > 127.f) l = 127.f;
            upper_blend[y] = (int8_t) (int) std::lround(u);
            lower_blend[y] = (int8_t) (int) std::lround(l);
        }
    params_left_.upper_shape_adjust = upper_blend; params_right_.upper_shape_adjust = upper_blend;
    params_left_.lower_shape_adjust = lower_blend; params_right_.lower_shape_adjust = lower_blend;
        // Gaze state machine: fixation -> saccade
        if (saccade_duration_ <= 0.f && fixation_timer_ <= 0.f) {
            // Initialize first fixation interval
            fixation_timer_ = 0.f;
            next_fixation_duration_ = 0.8f + rand01() * 1.4f; // 0.8 - 2.2s
            choose_new_target(); // sets target & saccade params (not yet moving)
        }
        if (saccade_duration_ > 0.f && saccade_timer_ < saccade_duration_) {
            // In saccade (ballistic interpolation with ease-in/out to avoid stepping artifacts visually)
            saccade_timer_ += 0.02f * emotion_saccade_speed_scale; // speed scale
            float k = saccade_timer_ / saccade_duration_;
            if (k > 1.f) k = 1.f;
            // Fast accel/decel curve approximating main-sequence velocity profile
            float ease = k * k * (3 - 2*k);
            gaze_cx_ = gaze_sx_ + (gaze_tx_ - gaze_sx_) * ease;
            gaze_cy_ = gaze_sy_ + (gaze_ty_ - gaze_sy_) * ease;
            if (k >= 1.f) {
                // Start fixation
                fixation_timer_ = 0.f;
                next_fixation_duration_ = (0.8f + rand01() * 1.4f) * emotion_fixation_scale;
                // Choose new pupil dilation target proportional to upcoming fixation length
                float lenNorm = (next_fixation_duration_ - 0.8f) / 1.4f; // 0..1
                float base = 0.9f + lenNorm * 0.3f; // 0.9 .. 1.2
                base *= (0.95f + rand01() * 0.10f); // +/-5%
                if (base < 0.75f) base = 0.75f; else if (base > 1.25f) base = 1.25f;
                pupil_scale_target_ = base + emotion_pupil_bias;
                saccade_duration_ = 0.f;
            }
        } else {
            // In fixation
            fixation_timer_ += 0.02f;
            // Small tremor / drift noise
            float microX = (rand01() - 0.5f) * 0.6f; // +/-0.3 px
            float microY = (rand01() - 0.5f) * 0.6f;
            gaze_cx_ += (microX * 0.15f); // integrate tiny noise for subtle motion
            gaze_cy_ += (microY * 0.15f);
            // Clamp to valid region
            int minC = (int)params_left_.iris_radius;
            int maxC = kFrameW - minC;
            if (gaze_cx_ < minC) gaze_cx_ = (float)minC; else if (gaze_cx_ > maxC) gaze_cx_ = (float)maxC;
            if (gaze_cy_ < minC) gaze_cy_ = (float)minC; else if (gaze_cy_ > maxC) gaze_cy_ = (float)maxC;
            if (fixation_timer_ >= next_fixation_duration_) {
                choose_new_target(); // defines new target & saccade
            }
        }

    // Pupil dilation update
        if (saccade_duration_ <= 0.f || saccade_timer_ >= saccade_duration_) {
            float diff = pupil_scale_target_ - pupil_scale_cur_;
            pupil_scale_cur_ += diff * 0.05f; // approach target smoothly
        }
        pupil_breath_phase_ += 0.02f * 0.6f; // slow breathing phase
        float breath = std::sin(pupil_breath_phase_) * 0.02f; // +/-2%
    float pupil_final = pupil_scale_cur_ + breath + emotion_pupil_bias * 0.3f; // soften bias into final (cross-faded)
        if (pupil_final < 0.6f) pupil_final = 0.6f; else if (pupil_final > 1.4f) pupil_final = 1.4f;

    int iris_cx = (int)std::lround(gaze_cx_ + gaze_bias_x);
    int iris_cy = (int)std::lround(gaze_cy_ + gaze_bias_y);
        params_left_.iris_center_x = iris_cx; params_right_.iris_center_x = iris_cx;
        params_left_.iris_center_y = iris_cy; params_right_.iris_center_y = iris_cy;
        params_left_.pupil_scale = pupil_final; params_right_.pupil_scale = pupil_final;
        params_left_.sclera_parallax = 1.0f; params_right_.sclera_parallax = 1.0f;

        // Motion activity metric (EMA of gaze velocity)
        float vx = (gaze_cx_ - prev_gaze_cx_); // px per frame (20ms)
        float vy = (gaze_cy_ - prev_gaze_cy_);
        float inst_speed = std::sqrt(vx*vx + vy*vy); // px / frame
        prev_gaze_cx_ = gaze_cx_;
        prev_gaze_cy_ = gaze_cy_;
        // Convert to approx px/sec (frame dt=0.02)
        float inst_speed_ps = inst_speed * 50.f;
        // Normalize: assume 0..500 px/sec typical range, clamp
        float norm = inst_speed_ps / 500.f;
        if (norm > 1.f) norm = 1.f;
        // Exponential moving average toward norm
        activity_level_ += (norm - activity_level_) * 0.08f;

        // Randomized blink scheduling state machine
        float open;
        if (t_ >= next_blink_time_ && blink_state_ == BlinkState::Idle) {
            blink_state_ = BlinkState::Closing;
            blink_timer_ = 0.f;
        }
        switch (blink_state_) {
            case BlinkState::Idle:
                open = 1.f; break;
            case BlinkState::Closing: {
                blink_timer_ += 0.02f;
                float k = blink_timer_ / blink_close_dur_;
                if (k > 1.f) { k = 1.f; blink_state_ = BlinkState::Hold; blink_timer_ = 0.f; }
                k = k*k*(3-2*k);
                open = 1.f - k;
            } break;
            case BlinkState::Hold: {
                blink_timer_ += 0.02f;
                open = 0.f;
                if (blink_timer_ >= blink_hold_dur_) { blink_state_ = BlinkState::Opening; blink_timer_ = 0.f; }
            } break;
            case BlinkState::Opening: {
                blink_timer_ += 0.02f;
                float k = blink_timer_ / blink_open_dur_;
                if (k > 1.f) { k = 1.f; blink_state_ = BlinkState::Idle; blink_timer_ = 0.f; 
                    // Schedule next blink with jitter
                    float interval = blink_period_base_ + rand01() * blink_period_jitter_;
                    next_blink_time_ = t_ + interval; }
                k = k*k*(3-2*k);
                open = k;
            } break;
        }
        if (blink_state_ == BlinkState::Idle && next_blink_time_ == 0.f) {
            // Initialize first schedule
            float interval = blink_period_base_ + rand01() * blink_period_jitter_;
            next_blink_time_ = t_ + interval;
            open = 1.f;
        }
    // Apply emotion eyelid bias and clamp (manual clamp; legacy std::clamp removed)
        {
            float eo = open + eyelid_open_bias;
            if (eo < 0.f) eo = 0.f; else if (eo > 1.f) eo = 1.f;
            params_left_.eyelid_open = eo; params_right_.eyelid_open = eo;
        }
        // Subtle hue/brightness modulation for lids
    // Use fixed dark colours for eyelids (set once in init). No per-frame color modulation.
    // Base render once (common iris/sclera); eyelids differ only by mirroring.
    // Use left params (mirror flag ignored in base) then apply eyelids for each eye.
    // Draw function (white)
    // Overlay pixel draw (invert Y to match physical panel orientation)
    // FPS overlay removed per request.
    // Render base once into base_frame_ using left params (mirror flag ignored for base content)
    render_eye_base(base_frame_, params_left_);
    auto draw_overlays = [&](){ /* no-op */ };
    // LEFT EYE: copy base, apply left eyelids, overlays, blit
    std::memcpy(frame_, base_frame_, sizeof(frame_));
    apply_eyelids(frame_, params_left_);
    draw_overlays();
    if (left_) left_->blit(frame_, full);
    // RIGHT EYE: copy same base, apply right eyelids (mirrored), overlays, blit
    std::memcpy(frame_, base_frame_, sizeof(frame_));
    apply_eyelids(frame_, params_right_);
    draw_overlays();
    if (right_) right_->blit(frame_, full);
        tight_loop_contents();
    }
}

} // namespace eyes
