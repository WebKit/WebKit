/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_TEST_ENCODED_FRAME_GENERATORS_H_
#define VIDEO_TIMING_SIMULATOR_TEST_ENCODED_FRAME_GENERATORS_H_

#include <cstdint>
#include <memory>

#include "api/environment/environment.h"
#include "api/video/encoded_frame.h"
#include "test/fake_encoded_frame.h"

namespace webrtc::video_timing_simulator {

// Provides `FakeFrameBuilder`s without references set.
class EncodedFrameBuilderGenerator {
 public:
  static constexpr uint32_t kSsrc = 123456;

  struct BuilderWithFrameId {
    test::FakeFrameBuilder builder;
    int64_t frame_id;
  };

  explicit EncodedFrameBuilderGenerator(const Environment& env);
  ~EncodedFrameBuilderGenerator();

  BuilderWithFrameId NextEncodedFrameBuilder();

 private:
  const Environment env_;

  uint32_t rtp_timestamp_ = 0;
  int64_t frame_id_ = 0;
};

// Simulates https://www.w3.org/TR/webrtc-svc/#L1T1*.
//
//   TL0 |     [F0] ---> [F1] ---> [F2] ---> [F3] ---> [F4] ---> [F5]
//       +----------------------------------------------------------> Time
// Frame:      F0        F1        F2        F3        F4        F5
// Index:      0         1         2         3         4         5
class SingleLayerEncodedFrameGenerator {
 public:
  explicit SingleLayerEncodedFrameGenerator(const Environment& env);
  ~SingleLayerEncodedFrameGenerator();

  std::unique_ptr<EncodedFrame> NextEncodedFrame();

 private:
  EncodedFrameBuilderGenerator builder_generator_;
};

// Simulates https://www.w3.org/TR/webrtc-svc/#L1T3*.
//
//   TL2 |         [TL2a]     [TL2b]         [TL2a]
//       |          /          /             /
//       |         /          /             /
//   TL1 |        /       [TL1]            /
//       |       /          /             /
//       |      /          /             /
//   TL0 |     [K]----------------------[TL0]
//       +-------------------------------------------> Time
// Frame:      K   TL2a    TL1 TL2b      TL0 TL2a
// Index:      0   1       2   3         4   5
class TemporalLayersEncodedFrameGenerator {
 public:
  static constexpr int kNumTemporalLayers = 4;

  explicit TemporalLayersEncodedFrameGenerator(const Environment& env);
  ~TemporalLayersEncodedFrameGenerator();

  std::unique_ptr<EncodedFrame> NextEncodedFrame();

 private:
  EncodedFrameBuilderGenerator builder_generator_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_TEST_ENCODED_FRAME_GENERATORS_H_
