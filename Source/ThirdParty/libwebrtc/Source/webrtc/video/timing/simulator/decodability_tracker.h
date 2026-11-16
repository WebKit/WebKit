/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_DECODABILITY_TRACKER_H_
#define VIDEO_TIMING_SIMULATOR_DECODABILITY_TRACKER_H_

#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_frame.h"
#include "api/video/frame_buffer.h"
#include "rtc_base/thread_annotations.h"
#include "video/timing/simulator/assembler.h"

namespace webrtc::video_timing_simulator {

// Callback for observer events. Implemented by the metadata collector.
class DecodabilityTrackerEvents {
 public:
  virtual ~DecodabilityTrackerEvents() = default;
  virtual void OnDecodableFrame(const EncodedFrame& decodable_frame) = 0;
};

// The `DecodabilityTracker` takes a sequence of assembled `EncodedFrame`s
// belonging to the same stream and produces a sequence of decodable
// `EncodedFrame`s. The work is delegated to the `FrameBuffer`.
// Note that this class intentionally performs NO jitter buffering or other
// timing.
class DecodabilityTracker : public AssembledFrameCallback {
 public:
  // All members of the config should be explicitly set by the user.
  struct Config {
    uint32_t ssrc = 0;
  };

  DecodabilityTracker(const Environment& env,
                      const Config& config,
                      DecodabilityTrackerEvents* absl_nonnull observer);
  ~DecodabilityTracker() override;

  DecodabilityTracker(const DecodabilityTracker&) = delete;
  DecodabilityTracker& operator=(const DecodabilityTracker&) = delete;

  void SetDecodedFrameIdCallback(
      DecodedFrameIdCallback* absl_nonnull decoded_id_cb);

  // Implements `AssembledFrameCallback`.
  // Inserts `assembled_frame` into the `FrameBuffer` and logs any decodable
  // frames to the `observer_`.
  void OnAssembledFrame(std::unique_ptr<EncodedFrame> assembled_frame) override;

 private:
  // Environment.
  SequenceChecker sequence_checker_;
  const Environment env_;
  const Config config_;

  // Worker object.
  FrameBuffer frame_buffer_ RTC_GUARDED_BY(sequence_checker_);

  // Outputs.
  DecodabilityTrackerEvents& observer_;
  DecodedFrameIdCallback* absl_nullable decoded_frame_id_cb_
      RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_DECODABILITY_TRACKER_H_
