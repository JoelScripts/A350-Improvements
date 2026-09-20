#pragma once

#include <cstdint>

namespace taxi_camera {

// A native view can stay ready while an unobserved submission invalidates its
// GPU state. Detect that specific stall without admitting an unknown state as RT.
class CaptureProgress {
 public:
  bool observe(std::uint64_t now, bool eligible, std::uint64_t frames, std::uint64_t draws, bool unknown_state) noexcept {
    if (!eligible || now < since_) {
      watching_ = false;
      stalled_ = false;
      frames_ = frames;
      draws_ = draws;
      since_ = now;
      last_recovery_ = 0;
      return false;
    }

    // A successful frame immediately clears a previous stall. This is important
    // when an upscaler/frame-generation layer briefly reports an indeterminate
    // state and then resumes normal submissions.
    if (frames != frames_) {
      watching_ = false;
      stalled_ = false;
      frames_ = frames;
      draws_ = draws;
      since_ = now;
      return false;
    }

    frames_ = frames;
    if (!watching_) {
      watching_ = true;
      since_ = now;
      draws_ = draws;
      return false;
    }

    // The original watchdog only recovered when the hook explicitly classified
    // the state as unknown. Some ReShade/DLSS/FG combinations leave the state
    // known while the camera source simply stops producing frames. Treat a
    // sustained zero-progress interval as recoverable too, but rate-limit the
    // rearm so we never spin on the render thread.
    const auto elapsed = now - since_;
    const bool source_active = draws > draws_;
    const bool no_frame_progress = elapsed >= 2500;
    const bool recoverable = unknown_state || no_frame_progress || source_active;
    if (!recoverable || elapsed < 2000 || (last_recovery_ && now - last_recovery_ < 2500))
      return false;

    watching_ = false;
    stalled_ = unknown_state || no_frame_progress;
    last_recovery_ = now;
    since_ = now;
    draws_ = draws;
    return true;
  }

  bool stalled() const noexcept { return stalled_; }
  void reset() noexcept { *this = {}; }

 private:
  bool watching_ = false, stalled_ = false;
  std::uint64_t frames_ = 0, draws_ = 0, since_ = 0, last_recovery_ = 0;
};

}  // namespace taxi_camera
