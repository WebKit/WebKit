/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/frame_buffer2.h"

#include <algorithm>
#include <cstring>
#include <queue>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/jitter_estimator.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {
namespace video_coding {

namespace {
// Max number of frames the buffer will hold.
constexpr int kMaxFramesBuffered = 600;

// Max number of decoded frame info that will be saved.
constexpr int kMaxFramesHistory = 50;
}  // namespace

FrameBuffer::FrameBuffer(Clock* clock,
                         VCMJitterEstimator* jitter_estimator,
                         VCMTiming* timing,
                         VCMReceiveStatisticsCallback* stats_callback)
    : clock_(clock),
      new_continuous_frame_event_(false, false),
      jitter_estimator_(jitter_estimator),
      timing_(timing),
      inter_frame_delay_(clock_->TimeInMilliseconds()),
      last_decoded_frame_timestamp_(0),
      last_decoded_frame_it_(frames_.end()),
      last_continuous_frame_it_(frames_.end()),
      num_frames_history_(0),
      num_frames_buffered_(0),
      stopped_(false),
      protection_mode_(kProtectionNack),
      stats_callback_(stats_callback) {}

FrameBuffer::~FrameBuffer() {}

FrameBuffer::ReturnReason FrameBuffer::NextFrame(
    int64_t max_wait_time_ms,
    std::unique_ptr<FrameObject>* frame_out) {
  TRACE_EVENT0("webrtc", "FrameBuffer::NextFrame");
  int64_t latest_return_time_ms =
      clock_->TimeInMilliseconds() + max_wait_time_ms;
  int64_t wait_ms = max_wait_time_ms;
  int64_t now_ms = 0;

  do {
    now_ms = clock_->TimeInMilliseconds();
    {
      rtc::CritScope lock(&crit_);
      new_continuous_frame_event_.Reset();
      if (stopped_)
        return kStopped;

      wait_ms = max_wait_time_ms;

      // Need to hold |crit_| in order to use |frames_|, therefore we
      // set it here in the loop instead of outside the loop in order to not
      // acquire the lock unnecesserily.
      next_frame_it_ = frames_.end();

      // |frame_it| points to the first frame after the
      // |last_decoded_frame_it_|.
      auto frame_it = frames_.end();
      if (last_decoded_frame_it_ == frames_.end()) {
        frame_it = frames_.begin();
      } else {
        frame_it = last_decoded_frame_it_;
        ++frame_it;
      }

      // |continuous_end_it| points to the first frame after the
      // |last_continuous_frame_it_|.
      auto continuous_end_it = last_continuous_frame_it_;
      if (continuous_end_it != frames_.end())
        ++continuous_end_it;

      for (; frame_it != continuous_end_it && frame_it != frames_.end();
           ++frame_it) {
        if (!frame_it->second.continuous ||
            frame_it->second.num_missing_decodable > 0) {
          continue;
        }

        FrameObject* frame = frame_it->second.frame.get();
        next_frame_it_ = frame_it;
        if (frame->RenderTime() == -1)
          frame->SetRenderTime(timing_->RenderTimeMs(frame->timestamp, now_ms));
        wait_ms = timing_->MaxWaitingTime(frame->RenderTime(), now_ms);

        // This will cause the frame buffer to prefer high framerate rather
        // than high resolution in the case of the decoder not decoding fast
        // enough and the stream has multiple spatial and temporal layers.
        if (wait_ms == 0)
          continue;

        break;
      }
    }  // rtc::Critscope lock(&crit_);

    wait_ms = std::min<int64_t>(wait_ms, latest_return_time_ms - now_ms);
    wait_ms = std::max<int64_t>(wait_ms, 0);
  } while (new_continuous_frame_event_.Wait(wait_ms));

  {
    rtc::CritScope lock(&crit_);
    now_ms = clock_->TimeInMilliseconds();
    if (next_frame_it_ != frames_.end()) {
      std::unique_ptr<FrameObject> frame =
          std::move(next_frame_it_->second.frame);

      if (!frame->delayed_by_retransmission()) {
        int64_t frame_delay;

        if (inter_frame_delay_.CalculateDelay(frame->timestamp, &frame_delay,
                                              frame->ReceivedTime())) {
          jitter_estimator_->UpdateEstimate(frame_delay, frame->size());
        }

        float rtt_mult = protection_mode_ == kProtectionNackFEC ? 0.0 : 1.0;
        timing_->SetJitterDelay(jitter_estimator_->GetJitterEstimate(rtt_mult));
        timing_->UpdateCurrentDelay(frame->RenderTime(), now_ms);
      }

      // Gracefully handle bad RTP timestamps and render time issues.
      if (HasBadRenderTiming(*frame, now_ms)) {
        jitter_estimator_->Reset();
        timing_->Reset();
        frame->SetRenderTime(timing_->RenderTimeMs(frame->timestamp, now_ms));
      }

      UpdateJitterDelay();
      PropagateDecodability(next_frame_it_->second);

      // Sanity check for RTP timestamp monotonicity.
      if (last_decoded_frame_it_ != frames_.end()) {
        const FrameKey& last_decoded_frame_key = last_decoded_frame_it_->first;
        const FrameKey& frame_key = next_frame_it_->first;

        const bool frame_is_higher_spatial_layer_of_last_decoded_frame =
            last_decoded_frame_timestamp_ == frame->timestamp &&
            last_decoded_frame_key.picture_id == frame_key.picture_id &&
            last_decoded_frame_key.spatial_layer < frame_key.spatial_layer;

        if (AheadOrAt(last_decoded_frame_timestamp_, frame->timestamp) &&
            !frame_is_higher_spatial_layer_of_last_decoded_frame) {
          // TODO(brandtr): Consider clearing the entire buffer when we hit
          // these conditions.
          LOG(LS_WARNING) << "Frame with (timestamp:picture_id:spatial_id) ("
                          << frame->timestamp << ":" << frame->picture_id << ":"
                          << static_cast<int>(frame->spatial_layer) << ")"
                          << " sent to decoder after frame with"
                          << " (timestamp:picture_id:spatial_id) ("
                          << last_decoded_frame_timestamp_ << ":"
                          << last_decoded_frame_key.picture_id << ":"
                          << static_cast<int>(
                                 last_decoded_frame_key.spatial_layer)
                          << ").";
        }
      }

      AdvanceLastDecodedFrame(next_frame_it_);
      last_decoded_frame_timestamp_ = frame->timestamp;
      *frame_out = std::move(frame);
      return kFrameFound;
    }
  }

  if (latest_return_time_ms - now_ms > 0) {
    // If |next_frame_it_ == frames_.end()| and there is still time left, it
    // means that the frame buffer was cleared as the thread in this function
    // was waiting to acquire |crit_| in order to return. Wait for the
    // remaining time and then return.
    return NextFrame(latest_return_time_ms - now_ms, frame_out);
  }

  return kTimeout;
}

bool FrameBuffer::HasBadRenderTiming(const FrameObject& frame, int64_t now_ms) {
  // Assume that render timing errors are due to changes in the video stream.
  int64_t render_time_ms = frame.RenderTimeMs();
  const int64_t kMaxVideoDelayMs = 10000;
  if (render_time_ms < 0) {
    return true;
  }
  if (std::abs(render_time_ms - now_ms) > kMaxVideoDelayMs) {
    int frame_delay = static_cast<int>(std::abs(render_time_ms - now_ms));
    LOG(LS_WARNING) << "A frame about to be decoded is out of the configured "
                    << "delay bounds (" << frame_delay << " > "
                    << kMaxVideoDelayMs
                    << "). Resetting the video jitter buffer.";
    return true;
  }
  if (static_cast<int>(timing_->TargetVideoDelay()) > kMaxVideoDelayMs) {
    LOG(LS_WARNING) << "The video target delay has grown larger than "
                    << kMaxVideoDelayMs << " ms.";
    return true;
  }
  return false;
}

void FrameBuffer::SetProtectionMode(VCMVideoProtection mode) {
  TRACE_EVENT0("webrtc", "FrameBuffer::SetProtectionMode");
  rtc::CritScope lock(&crit_);
  protection_mode_ = mode;
}

void FrameBuffer::Start() {
  TRACE_EVENT0("webrtc", "FrameBuffer::Start");
  rtc::CritScope lock(&crit_);
  stopped_ = false;
}

void FrameBuffer::Stop() {
  TRACE_EVENT0("webrtc", "FrameBuffer::Stop");
  rtc::CritScope lock(&crit_);
  stopped_ = true;
  new_continuous_frame_event_.Set();
}

bool FrameBuffer::ValidReferences(const FrameObject& frame) const {
  for (size_t i = 0; i < frame.num_references; ++i) {
    if (AheadOrAt(frame.references[i], frame.picture_id))
      return false;
    for (size_t j = i + 1; j < frame.num_references; ++j) {
      if (frame.references[i] == frame.references[j])
        return false;
    }
  }

  if (frame.inter_layer_predicted && frame.spatial_layer == 0)
    return false;

  return true;
}

void FrameBuffer::UpdatePlayoutDelays(const FrameObject& frame) {
  TRACE_EVENT0("webrtc", "FrameBuffer::UpdatePlayoutDelays");
  PlayoutDelay playout_delay = frame.EncodedImage().playout_delay_;
  if (playout_delay.min_ms >= 0)
    timing_->set_min_playout_delay(playout_delay.min_ms);

  if (playout_delay.max_ms >= 0)
    timing_->set_max_playout_delay(playout_delay.max_ms);
}

int FrameBuffer::InsertFrame(std::unique_ptr<FrameObject> frame) {
  TRACE_EVENT0("webrtc", "FrameBuffer::InsertFrame");
  RTC_DCHECK(frame);
  if (stats_callback_)
    stats_callback_->OnCompleteFrame(frame->num_references == 0, frame->size());
  FrameKey key(frame->picture_id, frame->spatial_layer);

  rtc::CritScope lock(&crit_);

  int last_continuous_picture_id =
      last_continuous_frame_it_ == frames_.end()
          ? -1
          : last_continuous_frame_it_->first.picture_id;

  if (!ValidReferences(*frame)) {
    LOG(LS_WARNING) << "Frame with (picture_id:spatial_id) (" << key.picture_id
                    << ":" << static_cast<int>(key.spatial_layer)
                    << ") has invalid frame references, dropping frame.";
    return last_continuous_picture_id;
  }

  if (num_frames_buffered_ >= kMaxFramesBuffered) {
    LOG(LS_WARNING) << "Frame with (picture_id:spatial_id) (" << key.picture_id
                    << ":" << static_cast<int>(key.spatial_layer)
                    << ") could not be inserted due to the frame "
                    << "buffer being full, dropping frame.";
    return last_continuous_picture_id;
  }

  if (last_decoded_frame_it_ != frames_.end() &&
      key <= last_decoded_frame_it_->first) {
    if (AheadOf(frame->timestamp, last_decoded_frame_timestamp_) &&
        frame->num_references == 0) {
      // If this frame has a newer timestamp but an earlier picture id then we
      // assume there has been a jump in the picture id due to some encoder
      // reconfiguration or some other reason. Even though this is not according
      // to spec we can still continue to decode from this frame if it is a
      // keyframe.
      LOG(LS_WARNING) << "A jump in picture id was detected, clearing buffer.";
      ClearFramesAndHistory();
      last_continuous_picture_id = -1;
    } else {
      LOG(LS_WARNING) << "Frame with (picture_id:spatial_id) ("
                      << key.picture_id << ":"
                      << static_cast<int>(key.spatial_layer)
                      << ") inserted after frame ("
                      << last_decoded_frame_it_->first.picture_id << ":"
                      << static_cast<int>(
                             last_decoded_frame_it_->first.spatial_layer)
                      << ") was handed off for decoding, dropping frame.";
      return last_continuous_picture_id;
    }
  }

  // Test if inserting this frame would cause the order of the frames to become
  // ambiguous (covering more than half the interval of 2^16). This can happen
  // when the picture id make large jumps mid stream.
  if (!frames_.empty() &&
      key < frames_.begin()->first &&
      frames_.rbegin()->first < key) {
    LOG(LS_WARNING) << "A jump in picture id was detected, clearing buffer.";
    ClearFramesAndHistory();
    last_continuous_picture_id = -1;
  }

  auto info = frames_.insert(std::make_pair(key, FrameInfo())).first;

  if (info->second.frame) {
    LOG(LS_WARNING) << "Frame with (picture_id:spatial_id) (" << key.picture_id
                    << ":" << static_cast<int>(key.spatial_layer)
                    << ") already inserted, dropping frame.";
    return last_continuous_picture_id;
  }

  if (!UpdateFrameInfoWithIncomingFrame(*frame, info))
    return last_continuous_picture_id;
  UpdatePlayoutDelays(*frame);
  info->second.frame = std::move(frame);
  ++num_frames_buffered_;

  if (info->second.num_missing_continuous == 0) {
    info->second.continuous = true;
    PropagateContinuity(info);
    last_continuous_picture_id = last_continuous_frame_it_->first.picture_id;

    // Since we now have new continuous frames there might be a better frame
    // to return from NextFrame. Signal that thread so that it again can choose
    // which frame to return.
    new_continuous_frame_event_.Set();
  }

  return last_continuous_picture_id;
}

void FrameBuffer::PropagateContinuity(FrameMap::iterator start) {
  TRACE_EVENT0("webrtc", "FrameBuffer::PropagateContinuity");
  RTC_DCHECK(start->second.continuous);
  if (last_continuous_frame_it_ == frames_.end())
    last_continuous_frame_it_ = start;

  std::queue<FrameMap::iterator> continuous_frames;
  continuous_frames.push(start);

  // A simple BFS to traverse continuous frames.
  while (!continuous_frames.empty()) {
    auto frame = continuous_frames.front();
    continuous_frames.pop();

    if (last_continuous_frame_it_->first < frame->first)
      last_continuous_frame_it_ = frame;

    // Loop through all dependent frames, and if that frame no longer has
    // any unfulfilled dependencies then that frame is continuous as well.
    for (size_t d = 0; d < frame->second.num_dependent_frames; ++d) {
      auto frame_ref = frames_.find(frame->second.dependent_frames[d]);
      RTC_DCHECK(frame_ref != frames_.end());

      // TODO(philipel): Look into why we've seen this happen.
      if (frame_ref != frames_.end()) {
        --frame_ref->second.num_missing_continuous;
        if (frame_ref->second.num_missing_continuous == 0) {
          frame_ref->second.continuous = true;
          continuous_frames.push(frame_ref);
        }
      }
    }
  }
}

void FrameBuffer::PropagateDecodability(const FrameInfo& info) {
  TRACE_EVENT0("webrtc", "FrameBuffer::PropagateDecodability");
  RTC_CHECK(info.num_dependent_frames < FrameInfo::kMaxNumDependentFrames);
  for (size_t d = 0; d < info.num_dependent_frames; ++d) {
    auto ref_info = frames_.find(info.dependent_frames[d]);
    RTC_DCHECK(ref_info != frames_.end());
    // TODO(philipel): Look into why we've seen this happen.
    if (ref_info != frames_.end()) {
      RTC_DCHECK_GT(ref_info->second.num_missing_decodable, 0U);
      --ref_info->second.num_missing_decodable;
    }
  }
}

void FrameBuffer::AdvanceLastDecodedFrame(FrameMap::iterator decoded) {
  TRACE_EVENT0("webrtc", "FrameBuffer::AdvanceLastDecodedFrame");
  if (last_decoded_frame_it_ == frames_.end()) {
    last_decoded_frame_it_ = frames_.begin();
  } else {
    RTC_DCHECK(last_decoded_frame_it_->first < decoded->first);
    ++last_decoded_frame_it_;
  }
  --num_frames_buffered_;
  ++num_frames_history_;

  // First, delete non-decoded frames from the history.
  while (last_decoded_frame_it_ != decoded) {
    if (last_decoded_frame_it_->second.frame)
      --num_frames_buffered_;
    last_decoded_frame_it_ = frames_.erase(last_decoded_frame_it_);
  }

  // Then remove old history if we have too much history saved.
  if (num_frames_history_ > kMaxFramesHistory) {
    frames_.erase(frames_.begin());
    --num_frames_history_;
  }
}

bool FrameBuffer::UpdateFrameInfoWithIncomingFrame(const FrameObject& frame,
                                                   FrameMap::iterator info) {
  TRACE_EVENT0("webrtc", "FrameBuffer::UpdateFrameInfoWithIncomingFrame");
  FrameKey key(frame.picture_id, frame.spatial_layer);
  info->second.num_missing_continuous = frame.num_references;
  info->second.num_missing_decodable = frame.num_references;

  RTC_DCHECK(last_decoded_frame_it_ == frames_.end() ||
             last_decoded_frame_it_->first < info->first);

  // Check how many dependencies that have already been fulfilled.
  for (size_t i = 0; i < frame.num_references; ++i) {
    FrameKey ref_key(frame.references[i], frame.spatial_layer);
    auto ref_info = frames_.find(ref_key);

    // Does |frame| depend on a frame earlier than the last decoded frame?
    if (last_decoded_frame_it_ != frames_.end() &&
        ref_key <= last_decoded_frame_it_->first) {
      if (ref_info == frames_.end()) {
        LOG(LS_WARNING) << "Frame with (picture_id:spatial_id) ("
                        << key.picture_id << ":"
                        << static_cast<int>(key.spatial_layer)
                        << " depends on a non-decoded frame more previous than "
                        << "the last decoded frame, dropping frame.";
        return false;
      }

      --info->second.num_missing_continuous;
      --info->second.num_missing_decodable;
    } else {
      if (ref_info == frames_.end())
        ref_info = frames_.insert(std::make_pair(ref_key, FrameInfo())).first;

      if (ref_info->second.continuous)
        --info->second.num_missing_continuous;

      // Add backwards reference so |frame| can be updated when new
      // frames are inserted or decoded.
      ref_info->second.dependent_frames[ref_info->second.num_dependent_frames] =
          key;
      RTC_DCHECK_LT(ref_info->second.num_dependent_frames,
                    (FrameInfo::kMaxNumDependentFrames - 1));
      // TODO(philipel): Look into why this could happen and handle
      // appropriately.
      if (ref_info->second.num_dependent_frames <
          (FrameInfo::kMaxNumDependentFrames - 1)) {
        ++ref_info->second.num_dependent_frames;
      }
    }
    RTC_DCHECK_LE(ref_info->second.num_missing_continuous,
                  ref_info->second.num_missing_decodable);
  }

  // Check if we have the lower spatial layer frame.
  if (frame.inter_layer_predicted) {
    ++info->second.num_missing_continuous;
    ++info->second.num_missing_decodable;

    FrameKey ref_key(frame.picture_id, frame.spatial_layer - 1);
    // Gets or create the FrameInfo for the referenced frame.
    auto ref_info = frames_.insert(std::make_pair(ref_key, FrameInfo())).first;
    if (ref_info->second.continuous)
      --info->second.num_missing_continuous;

    if (ref_info == last_decoded_frame_it_) {
      --info->second.num_missing_decodable;
    } else {
      ref_info->second.dependent_frames[ref_info->second.num_dependent_frames] =
          key;
      ++ref_info->second.num_dependent_frames;
    }
    RTC_DCHECK_LE(ref_info->second.num_missing_continuous,
                  ref_info->second.num_missing_decodable);
  }

  RTC_DCHECK_LE(info->second.num_missing_continuous,
                info->second.num_missing_decodable);

  return true;
}

void FrameBuffer::UpdateJitterDelay() {
  TRACE_EVENT0("webrtc", "FrameBuffer::UpdateJitterDelay");
  if (!stats_callback_)
    return;

  int decode_ms;
  int max_decode_ms;
  int current_delay_ms;
  int target_delay_ms;
  int jitter_buffer_ms;
  int min_playout_delay_ms;
  int render_delay_ms;
  if (timing_->GetTimings(&decode_ms, &max_decode_ms, &current_delay_ms,
                          &target_delay_ms, &jitter_buffer_ms,
                          &min_playout_delay_ms, &render_delay_ms)) {
    stats_callback_->OnFrameBufferTimingsUpdated(
        decode_ms, max_decode_ms, current_delay_ms, target_delay_ms,
        jitter_buffer_ms, min_playout_delay_ms, render_delay_ms);
  }
}

void FrameBuffer::ClearFramesAndHistory() {
  TRACE_EVENT0("webrtc", "FrameBuffer::UpdateJitterDelay");
  frames_.clear();
  last_decoded_frame_it_ = frames_.end();
  last_continuous_frame_it_ = frames_.end();
  next_frame_it_ = frames_.end();
  num_frames_history_ = 0;
  num_frames_buffered_ = 0;
}

}  // namespace video_coding
}  // namespace webrtc
