/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/adapted_video_track_source.h"

#include <cstdint>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_track_source_constraints.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

AdaptedVideoTrackSource::AdaptedVideoTrackSource() = default;

AdaptedVideoTrackSource::AdaptedVideoTrackSource(int required_alignment)
    : video_adapter_(required_alignment) {}

AdaptedVideoTrackSource::~AdaptedVideoTrackSource() = default;

bool AdaptedVideoTrackSource::GetStats(Stats* stats) {
  MutexLock lock(&stats_mutex_);

  if (!stats_) {
    return false;
  }

  *stats = *stats_;
  return true;
}

void AdaptedVideoTrackSource::OnFrame(const VideoFrame& frame) {
  scoped_refptr<VideoFrameBuffer> buffer(frame.video_frame_buffer());
  /* Note that this is a "best effort" approach to
     wants.rotation_applied; apply_rotation_ can change from false to
     true between the check of apply_rotation() and the call to
     broadcaster_.OnFrame(), in which case we generate a frame with
     pending rotation despite some sink with wants.rotation_applied ==
     true was just added. The VideoBroadcaster enforces
     synchronization for us in this case, by not passing the frame on
     to sinks which don't want it. */
  if (apply_rotation() && frame.rotation() != kVideoRotation_0 &&
      buffer->type() == VideoFrameBuffer::Type::kI420) {
    /* Apply pending rotation. */
    VideoFrame rotated_frame(frame);
    rotated_frame.set_video_frame_buffer(
        I420Buffer::Rotate(*buffer->GetI420(), frame.rotation()));
    rotated_frame.set_rotation(kVideoRotation_0);
    broadcaster_.OnFrame(rotated_frame);
  } else {
    broadcaster_.OnFrame(frame);
  }
}

void AdaptedVideoTrackSource::OnFrameDropped() {
  broadcaster_.OnDiscardedFrame();
}

void AdaptedVideoTrackSource::AddOrUpdateSink(
    VideoSinkInterface<VideoFrame>* sink,
    const VideoSinkWants& wants) {
  broadcaster_.AddOrUpdateSink(sink, wants);
  OnSinkWantsChanged(broadcaster_.wants());
}

void AdaptedVideoTrackSource::RemoveSink(VideoSinkInterface<VideoFrame>* sink) {
  broadcaster_.RemoveSink(sink);
  OnSinkWantsChanged(broadcaster_.wants());
}

bool AdaptedVideoTrackSource::apply_rotation() {
  return broadcaster_.wants().rotation_applied;
}

void AdaptedVideoTrackSource::OnSinkWantsChanged(const VideoSinkWants& wants) {
  video_adapter_.OnSinkWants(wants);
}

bool AdaptedVideoTrackSource::AdaptFrame(int width,
                                         int height,
                                         int64_t time_us,
                                         int* out_width,
                                         int* out_height,
                                         int* crop_width,
                                         int* crop_height,
                                         int* crop_x,
                                         int* crop_y) {
  {
    MutexLock lock(&stats_mutex_);
    stats_ = Stats{.input_width = width, .input_height = height};
  }

  if (!broadcaster_.frame_wanted()) {
    return false;
  }

  if (!video_adapter_.AdaptFrameResolution(
          width, height, time_us * kNumNanosecsPerMicrosec, crop_width,
          crop_height, out_width, out_height)) {
    broadcaster_.OnDiscardedFrame();
    // VideoAdapter dropped the frame.
    return false;
  }

  *crop_x = (width - *crop_width) / 2;
  *crop_y = (height - *crop_height) / 2;
  return true;
}

void AdaptedVideoTrackSource::ProcessConstraints(
    const VideoTrackSourceConstraints& constraints) {
  broadcaster_.ProcessConstraints(constraints);
}

}  // namespace webrtc
