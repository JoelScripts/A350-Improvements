#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../camera/body_pose_provider.hpp"

namespace taxi_camera::standalone {

// Protects the native Taxi Cam rendering path while the simulator's public
// camera makes a large external jump (for example a ChasePlane view change).
// This never changes the simulator camera and never destroys owned Taxi Cam
// views. It only requests a short, bounded pause in Taxi Cam rendering.
class CameraTransitionGuard {
 public:
  struct Delta {
    double position = 0.0;
    double target = 0.0;
    double pbh = 0.0;
    double fov = 0.0;
  };

  bool observe(const native_camera::PublicCameraSample& sample, std::uint64_t now) noexcept {
    timed_out_ = false;
    Delta delta{};
    if (sample.valid && previous_.valid && sample.sample_ms != previous_.sample_ms) {
      delta = calculate_delta(previous_, sample);
    }
    if (sample.valid)
      previous_ = sample;

    const bool jump = delta.position >= 0.75 || delta.target >= 1.0 || delta.pbh >= 0.10 || delta.fov >= 0.05;
    const bool movement = delta.position >= 0.10 || delta.target >= 0.15 || delta.pbh >= 0.015 || delta.fov >= 0.008;

    if (!active_) {
      if (!jump)
        return false;
      active_ = true;
      detected_ms_ = now;
      last_change_ms_ = now;
      stable_since_ms_ = 0;
      last_delta_ = delta;
      return true;
    }

    if (movement) {
      last_change_ms_ = now;
      stable_since_ms_ = 0;
      last_delta_ = delta;
    } else if (!stable_since_ms_) {
      stable_since_ms_ = now;
    }

    if (stable_since_ms_ && now >= stable_since_ms_ && now - stable_since_ms_ >= 150) {
      active_ = false;
      stable_since_ms_ = 0;
      return false;
    }

    if (now >= detected_ms_ && now - detected_ms_ >= 1500) {
      active_ = false;
      timed_out_ = true;
      stable_since_ms_ = 0;
      return false;
    }
    return true;
  }

  bool active() const noexcept { return active_; }
  bool timed_out() const noexcept { return timed_out_; }
  std::uint64_t detected_ms() const noexcept { return detected_ms_; }
  const Delta& last_delta() const noexcept { return last_delta_; }

  void reset() noexcept { *this = {}; }

 private:
  static double distance3(const double a[3], const double b[3]) noexcept {
    const double x = a[0] - b[0];
    const double y = a[1] - b[1];
    const double z = a[2] - b[2];
    return std::sqrt(x * x + y * y + z * z);
  }

  static double angle3(const float a[3], const float b[3]) noexcept {
    return std::max({std::abs(double(a[0]) - double(b[0])), std::abs(double(a[1]) - double(b[1])),
                     std::abs(double(a[2]) - double(b[2]))});
  }

  static Delta calculate_delta(const native_camera::PublicCameraSample& a,
                               const native_camera::PublicCameraSample& b) noexcept {
    Delta out{};
    out.position = distance3(a.position, b.position);
    out.target = distance3(a.target, b.target);
    out.pbh = angle3(a.pbh, b.pbh);
    out.fov = std::abs(a.fov - b.fov);
    return out;
  }

  bool active_ = false;
  bool timed_out_ = false;
  native_camera::PublicCameraSample previous_{};
  std::uint64_t detected_ms_ = 0;
  std::uint64_t last_change_ms_ = 0;
  std::uint64_t stable_since_ms_ = 0;
  Delta last_delta_{};
};

}  // namespace taxi_camera::standalone
