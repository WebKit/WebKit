/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/adaptedvideotracksource.h"

#include "webrtc/api/video/i420_buffer.h"

namespace rtc {

AdaptedVideoTrackSource::AdaptedVideoTrackSource() {
  thread_checker_.DetachFromThread();
}

AdaptedVideoTrackSource::AdaptedVideoTrackSource(int required_alignment)
    : video_adapter_(required_alignment) {
  thread_checker_.DetachFromThread();
}

bool AdaptedVideoTrackSource::GetStats(Stats* stats) {
  rtc::CritScope lock(&stats_crit_);

  if (!stats_) {
    return false;
  }

  *stats = *stats_;
  return true;
}

void AdaptedVideoTrackSource::OnFrame(const webrtc::VideoFrame& frame) {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      frame.video_frame_buffer());
  /* Note that this is a "best effort" approach to
     wants.rotation_applied; apply_rotation_ can change from false to
     true between the check of apply_rotation() and the call to
     broadcaster_.OnFrame(), in which case we generate a frame with
     pending rotation despite some sink with wants.rotation_applied ==
     true was just added. The VideoBroadcaster enforces
     synchronization for us in this case, by not passing the frame on
     to sinks which don't want it. */
  if (apply_rotation() &&
      frame.rotation() != webrtc::kVideoRotation_0 &&
      !buffer->native_handle()) {
    /* Apply pending rotation. */
    broadcaster_.OnFrame(webrtc::VideoFrame(
        webrtc::I420Buffer::Rotate(*buffer, frame.rotation()),
        webrtc::kVideoRotation_0, frame.timestamp_us()));
  } else {
    broadcaster_.OnFrame(frame);
  }
}

void AdaptedVideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  broadcaster_.AddOrUpdateSink(sink, wants);
  OnSinkWantsChanged(broadcaster_.wants());
}

void AdaptedVideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  broadcaster_.RemoveSink(sink);
  OnSinkWantsChanged(broadcaster_.wants());
}

bool AdaptedVideoTrackSource::apply_rotation() {
  return broadcaster_.wants().rotation_applied;
}

void AdaptedVideoTrackSource::OnSinkWantsChanged(
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  video_adapter_.OnResolutionRequest(wants.target_pixel_count,
                                     wants.max_pixel_count);
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
    rtc::CritScope lock(&stats_crit_);
    stats_ = rtc::Optional<Stats>({width, height});
  }

  if (!broadcaster_.frame_wanted()) {
    return false;
  }

  if (!video_adapter_.AdaptFrameResolution(
          width, height, time_us * rtc::kNumNanosecsPerMicrosec,
          crop_width, crop_height, out_width, out_height)) {
    // VideoAdapter dropped the frame.
    return false;
  }

  *crop_x = (width - *crop_width) / 2;
  *crop_y = (height - *crop_height) / 2;
  return true;
}

}  // namespace rtc
