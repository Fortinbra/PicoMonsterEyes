#pragma once

#include <cstdint>
#include "eye_renderer.hpp" // EyeRenderParams

namespace eyes {

class App {
public:
    bool init();
    void loop();
private:
    enum class Emotion { Neutral, Sad, Fear, Anger, Disgust, COUNT };
    // Framebuffer and render state
    static constexpr int kFrameW = 128;
    static constexpr int kFrameH = 128;
    uint16_t frame_[kFrameW * kFrameH]{};
    // Separate buffer for base (sclera/iris/highlights) so we can apply eyelids twice
    uint16_t base_frame_[kFrameW * kFrameH]{};
    // Displays (constructed in init)
    class Ssd1351Display* left_ = nullptr;
    class Ssd1351Display* right_ = nullptr;
    // Eye parameters (animated pupil)
    EyeRenderParams params_left_{}; // left eye params
    EyeRenderParams params_right_{}; // right eye params
    float t_ = 0.f;
    // Saccade / fixation state
    float gaze_cx_ = kFrameW * 0.5f; // current (float for interpolation)
    float gaze_cy_ = kFrameH * 0.5f;
    float gaze_sx_ = gaze_cx_;       // saccade start position
    float gaze_sy_ = gaze_cy_;
    float gaze_tx_ = gaze_cx_;       // target position
    float gaze_ty_ = gaze_cy_;
    float fixation_timer_ = 0.f;
    float next_fixation_duration_ = 1.0f; // seconds
    float saccade_timer_ = 0.f;
    float saccade_duration_ = 0.f;
    uint32_t rng_state_ = 0x12345678u;
    // Pupil dilation state (scaled multiplier applied to base_pupil_fraction)
    float pupil_scale_cur_ = 1.0f;
    float pupil_scale_target_ = 1.0f;
    float pupil_breath_phase_ = 0.f; // low amplitude in-fixation fluctuation
    // Motion activity & adaptive blink
    float activity_level_ = 0.f;      // 0 calm .. 1 very active
    float prev_gaze_cx_ = kFrameW * 0.5f;
    float prev_gaze_cy_ = kFrameH * 0.5f;
    // Emotion system
    Emotion emotion_ = Emotion::Neutral;
    float emotion_timer_ = 0.f;           // elapsed time in current emotion
    float emotion_cycle_len_ = 12.0f;     // seconds per emotion phase (temporary cycling)
    // Cross-fade
    Emotion prev_emotion_ = Emotion::Neutral;
    float emotion_fade_ = 0.f;            // 0..1 blend (0=prev,1=current)
    float emotion_fade_duration_ = 1.2f;  // seconds for visual fade

    // Blink state machine (slow natural blinks with slight random period jitter)
    enum class BlinkState { Idle, Closing, Hold, Opening };
    BlinkState blink_state_ = BlinkState::Idle;
    float blink_timer_ = 0.f;          // time inside current blink phase
    float next_blink_time_ = 0.f;      // absolute t_ when next blink should start
    // Base durations (can be tuned per emotion later if desired)
    float blink_close_dur_ = 0.12f;
    float blink_hold_dur_  = 0.08f;
    float blink_open_dur_  = 0.16f;
    float blink_period_base_ = 5.5f;   // average seconds between blinks
    float blink_period_jitter_ = 0.9f; // added *uniform*[0,1) * jitter

    // FPS tracking (updated once per second)
    uint32_t fps_frame_counter_ = 0;
    float fps_accum_time_ = 0.f;
    uint8_t fps_value_ = 0; // 0..99 shown
    uint64_t fps_last_sample_us_ = 0; // real-time sampling baseline

    float rand01() {
        rng_state_ = rng_state_ * 1664525u + 1013904223u; // LCG
        return (rng_state_ >> 8) * (1.0f / 16777216.0f);  // 24-bit to [0,1)
    }
    void choose_new_target();
    void advance_emotion();
};

} // namespace eyes
