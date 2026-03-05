/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/decodability_tracker.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/inlined_vector.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/timing/simulator/assembler.h"

namespace webrtc::video_timing_simulator {

// `FrameBuffer` configuration.
// Default values taken from video_stream_buffer_controller.cc.
constexpr int kMaxFrameBufferSize = 800;
constexpr int kMaxFrameBufferHistory = 1 << 13;

DecodabilityTracker::DecodabilityTracker(const Environment& env,
                                         DecodabilityTrackerEvents* absl_nonnull
                                             observer)
    : env_(env),
      frame_buffer_(kMaxFrameBufferSize,
                    kMaxFrameBufferHistory,
                    env.field_trials()),
      observer_(*observer),
      decoded_frame_id_cb_(nullptr) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

DecodabilityTracker::~DecodabilityTracker() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

void DecodabilityTracker::SetDecodedFrameIdCallback(
    DecodedFrameIdCallback* absl_nonnull decoded_frame_id_cb) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  decoded_frame_id_cb_ = decoded_frame_id_cb;
}

void DecodabilityTracker::OnAssembledFrame(
    std::unique_ptr<EncodedFrame> assembled_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_CHECK(decoded_frame_id_cb_) << "Callback must be set before running";
  if (!frame_buffer_.InsertFrame(std::move(assembled_frame))) {
    RTC_LOG(LS_ERROR) << "FrameBuffer insertion error";
  }
  RTC_DCHECK_EQ(frame_buffer_.GetTotalNumberOfDroppedFrames(), 0)
      << "The FrameBuffer should never drop frames when used by the "
         "DecodabilityTracker";
  // The insertion of `assembled_frame` may have made one or many frames
  // "continuous" (indirectly decodable). Iterate through all of these to get
  // get all decodable frames out of the buffer.
  // TODO: b/423646186 - Consider handling reordered higher temporal layers
  // better (right now they would be fast-forwarded over). This would likely
  // be done by introducing a lag between insertion and extraction, where the
  // lag duration is set as a (large) multiple of some typical network RTT.
  while (frame_buffer_.DecodableTemporalUnitsInfo()) {
    absl::InlinedVector<std::unique_ptr<webrtc::EncodedFrame>, 4>
        next_decodable_frames =
            frame_buffer_.ExtractNextDecodableTemporalUnit();
    // TODO: b/423646186 - Improve the handling of inter-layer predicted frames
    // here. See `CombineAndDeleteFrames` in frame_helpers.cc.
    for (std::unique_ptr<EncodedFrame>& encoded_frame : next_decodable_frames) {
      observer_.OnDecodableFrame(*encoded_frame);
      decoded_frame_id_cb_->OnDecodedFrameId(encoded_frame->Id());
      encoded_frame.reset();  // Just to be explicit.
    }
  }
}

}  // namespace webrtc::video_timing_simulator
