/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_FRAME_SELECTOR_H_
#define VIDEO_CORRUPTION_DETECTION_FRAME_SELECTOR_H_

#include <map>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/random.h"

namespace webrtc {

// Helper class for selecting frames to be used for corruption detection.
// Keyframes will always be selected. After that, the class will select the next
// frame based on if the timestamp falls within a given span:
// * Before the `lower_bound`, the frame will not be selected.
// * Between the `lower_bound` and `upper_bound`, a cutoff time is randomly
//   selected with an uniform distribution. If the timestamp falls within the
//   cutoff time, the frame will be selected.
// * After the `upper_bound`, the frame will be selected.
// State is kept on a per spatial/simulcast index basis.
// The class further supports distinguishing between low-overhead (meaning the
// pixel data can be directly used) and high-overhead (meaning that e.g. the
// has to be downloaded from GPU to main RAM, which causes delay an processing
// overhead).
// A scalability mode is present so that the selector knows if inter-layer
// dependency is used and can infer if a delta frame is part of a key
// superframe.
class FrameSelector {
 public:
  struct Timespan {
    TimeDelta lower_bound;
    TimeDelta upper_bound;
  };

  FrameSelector(ScalabilityMode scalability_mode,
                Timespan low_overhead_frame_span,
                Timespan high_overhead_frame_span);

  bool ShouldInstrumentFrame(const VideoFrame& raw_frame,
                             const EncodedImage& encoded_frame);

 private:
  const InterLayerPredMode inter_layer_pred_mode_;
  const Timespan low_overhead_frame_span_;
  const Timespan high_overhead_frame_span_;

  // Maps from simulcast index to the next timestamp cutoff threshold.
  // This means we assume that the next frames will be of the same type (high
  // vs low overhead) as the one we sampled and determined the next cutoff.
  std::map<int, Timestamp> next_timestamp_cutoff_thresholds_;

  Random random_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_FRAME_SELECTOR_H_
