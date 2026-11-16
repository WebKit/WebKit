/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/pendulum_frame_generator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/resolution.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/frame_utils.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {
namespace {

scoped_refptr<I420Buffer> ReadI420BufferFromFile(const std::string& filepath,
                                                 int width,
                                                 int height) {
  FILE* file = fopen(filepath.c_str(), "rb");
  if (file == nullptr) {
    return nullptr;
  }
  scoped_refptr<I420Buffer> buffer = ReadI420Buffer(width, height, file);
  fclose(file);
  return buffer;
}

class PendulumFrameReader : public FrameReader {
 public:
  explicit PendulumFrameReader(
      std::unique_ptr<PendulumFrameGenerator> generator)
      : generator_(std::move(generator)) {}

  scoped_refptr<I420Buffer> PullFrame() override {
    RTC_CHECK(generator_);
    ++frame_num_;
    return generator_->NextI420Frame();
  }

  scoped_refptr<I420Buffer> PullFrame(int* frame_num) override {
    *frame_num = frame_num_;
    return PullFrame();
  }

  scoped_refptr<I420Buffer> ReadFrame(int frame_num) override {
    return PullFrame();
  }

  scoped_refptr<I420Buffer> PullFrame(int* frame_num,
                                      Resolution resolution,
                                      Ratio framerate_scale) override {
    generator_->ChangeResolution(resolution.width, resolution.height);
    *frame_num = frame_num_;
    return PullFrame();
  }

  scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                      Resolution resolution) override {
    generator_->ChangeResolution(resolution.width, resolution.height);
    return PullFrame();
  }

  int num_frames() const override { return 1000000; }

 private:
  std::unique_ptr<PendulumFrameGenerator> generator_;
  int frame_num_ = 0;
};

}  // namespace

PendulumFrameGenerator::PendulumFrameGenerator(Config config)
    : config_(config) {
  Reset();
  LoadOrGenerateSourceImage();
}

void PendulumFrameGenerator::Reset() {
  th1_ = 0.5;
  th2_ = 0.5;
  w1_ = 0.0;
  w2_ = 0.0;
  sim_time_ = 0.0;
  zoom_phase_ = 0.0;
  prng_state_ = 123456789;
}

void PendulumFrameGenerator::ChangeResolution(size_t width, size_t height) {
  config_.target_resolution.width = static_cast<int>(width);
  config_.target_resolution.height = static_cast<int>(height);
}

FrameGeneratorInterface::Resolution PendulumFrameGenerator::GetResolution()
    const {
  return {.width = static_cast<size_t>(config_.target_resolution.width),
          .height = static_cast<size_t>(config_.target_resolution.height)};
}

void PendulumFrameGenerator::StepPhysics(double dt) {
  constexpr double g = 9.81;
  constexpr double gamma = 0.08;
  constexpr double drive1_amp = 5.0;
  constexpr double drive1_freq = 2.4;
  constexpr double drive2_amp = 3.5;
  constexpr double drive2_freq = 1.7;

  auto derivs = [&](double t1, double t2, double v1, double v2, double t,
                    double& dt1, double& dt2, double& dv1, double& dv2) {
    double delta = t1 - t2;
    double den = 2.0 - std::cos(2.0 * delta);
    double num1 = -g * 3.0 * std::sin(t1) - g * std::sin(t1 - 2.0 * t2) -
                  2.0 * std::sin(delta) * (v2 * v2 + v1 * v1 * std::cos(delta));
    double num2 =
        2.0 * std::sin(delta) *
        (2.0 * v1 * v1 + 2.0 * g * std::cos(t1) + v2 * v2 * std::cos(delta));
    dt1 = v1;
    dt2 = v2;
    dv1 = (num1 / den) - gamma * v1 + drive1_amp * std::cos(drive1_freq * t);
    dv2 = (num2 / den) - gamma * v2 + drive2_amp * std::cos(drive2_freq * t);
  };

  double k1_t1, k1_t2, k1_v1, k1_v2;
  derivs(th1_, th2_, w1_, w2_, sim_time_, k1_t1, k1_t2, k1_v1, k1_v2);

  double k2_t1, k2_t2, k2_v1, k2_v2;
  derivs(th1_ + 0.5 * dt * k1_t1, th2_ + 0.5 * dt * k1_t2,
         w1_ + 0.5 * dt * k1_v1, w2_ + 0.5 * dt * k1_v2, sim_time_ + 0.5 * dt,
         k2_t1, k2_t2, k2_v1, k2_v2);

  double k3_t1, k3_t2, k3_v1, k3_v2;
  derivs(th1_ + 0.5 * dt * k2_t1, th2_ + 0.5 * dt * k2_t2,
         w1_ + 0.5 * dt * k2_v1, w2_ + 0.5 * dt * k2_v2, sim_time_ + 0.5 * dt,
         k3_t1, k3_t2, k3_v1, k3_v2);

  double k4_t1, k4_t2, k4_v1, k4_v2;
  derivs(th1_ + dt * k3_t1, th2_ + dt * k3_t2, w1_ + dt * k3_v1,
         w2_ + dt * k3_v2, sim_time_ + dt, k4_t1, k4_t2, k4_v1, k4_v2);

  th1_ += (dt / 6.0) * (k1_t1 + 2.0 * k2_t1 + 2.0 * k3_t1 + k4_t1);
  th2_ += (dt / 6.0) * (k1_t2 + 2.0 * k2_t2 + 2.0 * k3_t2 + k4_t2);
  w1_ += (dt / 6.0) * (k1_v1 + 2.0 * k2_v1 + 2.0 * k3_v1 + k4_v1);
  w2_ += (dt / 6.0) * (k1_v2 + 2.0 * k2_v2 + 2.0 * k3_v2 + k4_v2);
  sim_time_ += dt;
}

void PendulumFrameGenerator::LoadOrGenerateSourceImage() {
  const std::string& path =
      config_.source_image_path.empty()
          ? ResourcePath("difficult_photo_1850_1110", "yuv")
          : config_.source_image_path;

  FrameGeneratorInterface::Resolution res = config_.source_resolution;
  if (std::optional<webrtc::Resolution> parsed_res =
          ParseResolutionFromFileName(path)) {
    res.width = static_cast<size_t>(parsed_res->width);
    res.height = static_cast<size_t>(parsed_res->height);
  }

  source_image_ = ReadI420BufferFromFile(path, static_cast<int>(res.width),
                                         static_cast<int>(res.height));

  if (!source_image_) {
    RTC_LOG(LS_WARNING) << "Failed to read " << path
                        << ", trying photo_1850_1110.yuv";
    const std::string fallback_path = ResourcePath("photo_1850_1110", "yuv");
    const std::optional<webrtc::Resolution> fallback_res =
        ParseResolutionFromFileName(fallback_path);
    const int fallback_width = fallback_res ? fallback_res->width : 1850;
    const int fallback_height = fallback_res ? fallback_res->height : 1110;
    source_image_ =
        ReadI420BufferFromFile(fallback_path, fallback_width, fallback_height);
  }

  if (!source_image_) {
    RTC_LOG(LS_WARNING)
        << "Generating synthetic test pattern for pendulum generator";
    const int w = res.width > 0 ? static_cast<int>(res.width) : 1850;
    const int h = res.height > 0 ? static_cast<int>(res.height) : 1110;
    source_image_ = I420Buffer::Create(w, h);
    std::span<uint8_t> py(source_image_->MutableDataY(),
                          static_cast<size_t>(h * source_image_->StrideY()));
    for (int y = 0; y < h; ++y) {
      const int row = y * source_image_->StrideY();
      for (int x = 0; x < w; ++x) {
        const int pattern = ((x / 16) ^ (y / 16)) & 1 ? 200 : 40;
        const int fine_pattern = ((x / 2) ^ (y / 2)) & 1 ? 20 : -20;
        py[row + x] = static_cast<uint8_t>(
            std::clamp(pattern + fine_pattern + (x % 64), 16, 235));
      }
    }
    std::span<uint8_t> pu(
        source_image_->MutableDataU(),
        static_cast<size_t>((h / 2) * source_image_->StrideU()));
    std::span<uint8_t> pv(
        source_image_->MutableDataV(),
        static_cast<size_t>((h / 2) * source_image_->StrideV()));
    for (int y = 0; y < h / 2; ++y) {
      const int row_u = y * source_image_->StrideU();
      const int row_v = y * source_image_->StrideV();
      for (int x = 0; x < w / 2; ++x) {
        pu[row_u + x] = static_cast<uint8_t>(128 + ((x * 4) % 100) - 50);
        pv[row_v + x] = static_cast<uint8_t>(128 + ((y * 4) % 100) - 50);
      }
    }
  }
}

void PendulumFrameGenerator::ApplyNoise(I420Buffer* buffer) {
  const int width = buffer->width();
  const int height = buffer->height();
  const int stride_y = buffer->StrideY();
  std::span<uint8_t> plane(buffer->MutableDataY(),
                           static_cast<size_t>(stride_y * height));
  const int noise_range = 2 * config_.noise_level + 1;

  for (int y = 0; y < height; ++y) {
    const int row_offset = y * stride_y;
    for (int x = 0; x < width; ++x) {
      prng_state_ ^= prng_state_ << 13;
      prng_state_ ^= prng_state_ >> 17;
      prng_state_ ^= prng_state_ << 5;
      const int noise =
          static_cast<int>(prng_state_ % noise_range) - config_.noise_level;
      plane[row_offset + x] = static_cast<uint8_t>(
          std::clamp(static_cast<int>(plane[row_offset + x]) + noise, 0, 255));
    }
  }
}

FrameGeneratorInterface::VideoFrameData PendulumFrameGenerator::NextFrame() {
  return VideoFrameData(NextI420Frame(), std::nullopt);
}

scoped_refptr<I420Buffer> PendulumFrameGenerator::NextI420Frame() {
  RTC_CHECK(source_image_);
  const double dt = 1.0 / std::max(1, config_.fps);
  for (int i = 0; i < 4; ++i) {
    StepPhysics(dt / 4.0);
  }

  // Smoothly map angles to [0, 1] normalized coordinates
  const double norm_x = 0.5 + 0.5 * std::clamp(std::sin(th1_), -1.0, 1.0);
  const double norm_y = 0.5 + 0.5 * std::clamp(std::sin(th2_), -1.0, 1.0);

  // Dynamic zoom factor
  zoom_phase_ += (2.0 * std::numbers::pi * config_.zoom_speed * dt) +
                 0.02 * (std::abs(w1_) + std::abs(w2_));
  const double norm_z = 0.5 + 0.5 * std::sin(zoom_phase_);
  double zoom =
      config_.min_zoom + norm_z * (config_.max_zoom - config_.min_zoom);
  zoom = std::max(1.0, zoom);

  const int src_w = source_image_->width();
  const int src_h = source_image_->height();
  const int tgt_w = config_.target_resolution.width;
  const int tgt_h = config_.target_resolution.height;

  int max_crop_w, max_crop_h;
  if (static_cast<int64_t>(src_w) * tgt_h >
      static_cast<int64_t>(src_h) * tgt_w) {
    max_crop_h = src_h;
    max_crop_w =
        static_cast<int>((static_cast<int64_t>(src_h) * tgt_w) / tgt_h);
  } else {
    max_crop_w = src_w;
    max_crop_h =
        static_cast<int>((static_cast<int64_t>(src_w) * tgt_h) / tgt_w);
  }

  int crop_w = static_cast<int>(max_crop_w / zoom);
  int crop_h = static_cast<int>(max_crop_h / zoom);
  crop_w = std::clamp(crop_w & ~1, 2, src_w & ~1);
  crop_h = std::clamp(crop_h & ~1, 2, src_h & ~1);

  const int max_offset_x = (src_w - crop_w) & ~1;
  const int max_offset_y = (src_h - crop_h) & ~1;

  const int offset_x =
      std::clamp(static_cast<int>(norm_x * max_offset_x) & ~1, 0, max_offset_x);
  const int offset_y =
      std::clamp(static_cast<int>(norm_y * max_offset_y) & ~1, 0, max_offset_y);

  scoped_refptr<I420Buffer> target_buffer = I420Buffer::Create(tgt_w, tgt_h);
  target_buffer->CropAndScaleFrom(*source_image_, offset_x, offset_y, crop_w,
                                  crop_h);

  if (config_.noise_level > 0) {
    ApplyNoise(target_buffer.get());
  }

  return target_buffer;
}

std::unique_ptr<PendulumFrameGenerator> CreatePendulumFrameGenerator(
    const PendulumFrameGenerator::Config& config) {
  return std::make_unique<PendulumFrameGenerator>(config);
}

std::unique_ptr<FrameReader> CreatePendulumFrameReader(
    const PendulumFrameGenerator::Config& config) {
  return std::make_unique<PendulumFrameReader>(
      CreatePendulumFrameGenerator(config));
}

}  // namespace test
}  // namespace webrtc
