/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/frame_forwarder.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

FrameForwarder::FrameForwarder() : sink_(nullptr) {}
FrameForwarder::~FrameForwarder() {}

void FrameForwarder::IncomingCapturedFrame(const VideoFrame& video_frame) {
  MutexLock lock(&mutex_);
  if (sink_)
    sink_->OnFrame(video_frame);
}

void FrameForwarder::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                                     const rtc::VideoSinkWants& wants) {
  MutexLock lock(&mutex_);
  AddOrUpdateSinkLocked(sink, wants);
}

void FrameForwarder::AddOrUpdateSinkLocked(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(!sink_ || sink_ == sink);
  sink_ = sink;
  sink_wants_ = wants;
}

void FrameForwarder::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  MutexLock lock(&mutex_);
  RTC_DCHECK_EQ(sink, sink_);
  sink_ = nullptr;
}

rtc::VideoSinkWants FrameForwarder::sink_wants() const {
  MutexLock lock(&mutex_);
  return sink_wants_;
}

rtc::VideoSinkWants FrameForwarder::sink_wants_locked() const {
  return sink_wants_;
}

bool FrameForwarder::has_sinks() const {
  MutexLock lock(&mutex_);
  return sink_ != nullptr;
}

}  // namespace test
}  // namespace webrtc
