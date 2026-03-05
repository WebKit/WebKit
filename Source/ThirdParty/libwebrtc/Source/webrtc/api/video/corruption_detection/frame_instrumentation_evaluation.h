/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_EVALUATION_H_
#define API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_EVALUATION_H_

#include <memory>

#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"

namespace webrtc {

class CorruptionScoreObserver {
 public:
  CorruptionScoreObserver() = default;
  virtual ~CorruptionScoreObserver() = default;

  // Results of corruption detection for a single frame, with a likelihood score
  // in the range [0.0, 1.0].
  virtual void OnCorruptionScore(double corruption_score,
                                 VideoContentType content_type) = 0;
};

// Interface for a class that takes received frame instrumentation data
// (i.e. corruption detection metadata) and validates that a given raw video
// frame closely matches that metadata. The result is given by a callback to a
// `CorruptionScoreObserver`, and that callback may by asynchronous so all
// implementation need to be thread safe.
// Note that calls to `OnInstrumentedFrame()` may happen on any thread - but
// concurrent calls are not allowed; only one decode callback at a time.
class FrameInstrumentationEvaluation {
 public:
  static std::unique_ptr<FrameInstrumentationEvaluation> Create(
      CorruptionScoreObserver* observer);

  virtual ~FrameInstrumentationEvaluation() = default;
  virtual void OnInstrumentedFrame(const FrameInstrumentationData& data,
                                   const VideoFrame& frame,
                                   VideoContentType frame_type) = 0;

 protected:
  FrameInstrumentationEvaluation() = default;
};

}  // namespace webrtc

#endif  // API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_EVALUATION_H_
