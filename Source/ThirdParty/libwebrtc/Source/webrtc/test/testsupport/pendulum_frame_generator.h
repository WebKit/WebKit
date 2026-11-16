/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_PENDULUM_FRAME_GENERATOR_H_
#define TEST_TESTSUPPORT_PENDULUM_FRAME_GENERATOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/i420_buffer.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

// A synthetic frame generator that crops and scales a high-detail static image
// along a smooth, erratic, non-repeating trajectory governed by double-pendulum
// chaotic physics with dynamic zoom and optional luma noise. Designed as an
// absolute stress test for video encoders.
class PendulumFrameGenerator : public FrameGeneratorInterface {
 public:
  struct Config {
    // Path to an i420 static image file.
    std::string source_image_path = "resources/difficult_photo_1850_1110.yuv";
    Resolution source_resolution = {.width = 1850, .height = 1110};

    // Target output frame resolution.
    Resolution target_resolution = {.width = 1280, .height = 720};

    // Target framerate (used for physics time step integration).
    int fps = 30;

    // Zoom limits (1.0 = maximum crop area fitting aspect ratio).
    double min_zoom = 1.2;
    double max_zoom = 3.0;

    // Base zoom cycle frequency in cycles per second.
    double zoom_speed = 0.3;

    // Luma white noise amplitude (+/- noise_level added to pixel values).
    // Set to 0 to disable noise.
    int noise_level = 20;
  };

  explicit PendulumFrameGenerator(Config config);
  ~PendulumFrameGenerator() override = default;

  void ChangeResolution(size_t width, size_t height) override;
  VideoFrameData NextFrame() override;
  scoped_refptr<I420Buffer> NextI420Frame();
  FrameGeneratorInterface::Resolution GetResolution() const override;
  std::optional<int> fps() const override { return config_.fps; }

  void Reset();

 private:
  void StepPhysics(double dt);
  void LoadOrGenerateSourceImage();
  void ApplyNoise(I420Buffer* buffer);

  Config config_;
  scoped_refptr<I420Buffer> source_image_;

  // Double pendulum state: angles (rad), angular velocities (rad/s)
  double th1_ = 0.5;
  double th2_ = 0.5;
  double w1_ = 0.0;
  double w2_ = 0.0;
  double sim_time_ = 0.0;

  // Zoom oscillation phase (rad)
  double zoom_phase_ = 0.0;

  // Fast PRNG state for additive luma noise
  uint32_t prng_state_ = 123456789;
};

// Factory functions.
std::unique_ptr<PendulumFrameGenerator> CreatePendulumFrameGenerator(
    const PendulumFrameGenerator::Config& config);

std::unique_ptr<FrameReader> CreatePendulumFrameReader(
    const PendulumFrameGenerator::Config& config);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_PENDULUM_FRAME_GENERATOR_H_
