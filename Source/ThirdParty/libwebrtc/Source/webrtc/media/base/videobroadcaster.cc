/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/videobroadcaster.h"

#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace rtc {

VideoBroadcaster::VideoBroadcaster() {
  thread_checker_.DetachFromThread();
}

void VideoBroadcaster::AddOrUpdateSink(
    VideoSinkInterface<webrtc::VideoFrame>* sink,
    const VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);
  rtc::CritScope cs(&sinks_and_wants_lock_);
  VideoSourceBase::AddOrUpdateSink(sink, wants);
  UpdateWants();
}

void VideoBroadcaster::RemoveSink(
    VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);
  rtc::CritScope cs(&sinks_and_wants_lock_);
  VideoSourceBase::RemoveSink(sink);
  UpdateWants();
}

bool VideoBroadcaster::frame_wanted() const {
  rtc::CritScope cs(&sinks_and_wants_lock_);
  return !sink_pairs().empty();
}

VideoSinkWants VideoBroadcaster::wants() const {
  rtc::CritScope cs(&sinks_and_wants_lock_);
  return current_wants_;
}

void VideoBroadcaster::OnFrame(const webrtc::VideoFrame& frame) {
  rtc::CritScope cs(&sinks_and_wants_lock_);
  for (auto& sink_pair : sink_pairs()) {
    if (sink_pair.wants.rotation_applied &&
        frame.rotation() != webrtc::kVideoRotation_0) {
      // Calls to OnFrame are not synchronized with changes to the sink wants.
      // When rotation_applied is set to true, one or a few frames may get here
      // with rotation still pending. Protect sinks that don't expect any
      // pending rotation.
      LOG(LS_VERBOSE) << "Discarding frame with unexpected rotation.";
      continue;
    }
    if (sink_pair.wants.black_frames) {
      sink_pair.sink->OnFrame(webrtc::VideoFrame(
          GetBlackFrameBuffer(frame.width(), frame.height()), frame.rotation(),
          frame.timestamp_us()));
    } else {
      sink_pair.sink->OnFrame(frame);
    }
  }
}

void VideoBroadcaster::UpdateWants() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  VideoSinkWants wants;
  wants.rotation_applied = false;
  for (auto& sink : sink_pairs()) {
    // wants.rotation_applied == ANY(sink.wants.rotation_applied)
    if (sink.wants.rotation_applied) {
      wants.rotation_applied = true;
    }
    // wants.max_pixel_count == MIN(sink.wants.max_pixel_count)
    if (sink.wants.max_pixel_count &&
        (!wants.max_pixel_count ||
         (*sink.wants.max_pixel_count < *wants.max_pixel_count))) {
      wants.max_pixel_count = sink.wants.max_pixel_count;
    }
    // wants.max_pixel_count_step_up == MIN(sink.wants.max_pixel_count_step_up)
    if (sink.wants.max_pixel_count_step_up &&
        (!wants.max_pixel_count_step_up ||
         (*sink.wants.max_pixel_count_step_up <
          *wants.max_pixel_count_step_up))) {
      wants.max_pixel_count_step_up = sink.wants.max_pixel_count_step_up;
    }
  }

  if (wants.max_pixel_count && wants.max_pixel_count_step_up &&
      *wants.max_pixel_count_step_up >= *wants.max_pixel_count) {
    wants.max_pixel_count_step_up = Optional<int>();
  }
  current_wants_ = wants;
}

const rtc::scoped_refptr<webrtc::VideoFrameBuffer>&
VideoBroadcaster::GetBlackFrameBuffer(int width, int height) {
  if (!black_frame_buffer_ || black_frame_buffer_->width() != width ||
      black_frame_buffer_->height() != height) {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        new RefCountedObject<webrtc::I420Buffer>(width, height);
    buffer->SetToBlack();
    black_frame_buffer_ = buffer;
  }

  return black_frame_buffer_;
}

}  // namespace rtc
