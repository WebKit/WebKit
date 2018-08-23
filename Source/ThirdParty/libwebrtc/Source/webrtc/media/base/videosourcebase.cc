/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/videosourcebase.h"

#include "rtc_base/checks.h"

namespace rtc {

VideoSourceBase::VideoSourceBase() {
  thread_checker_.DetachFromThread();
}
VideoSourceBase::~VideoSourceBase() = default;

void VideoSourceBase::AddOrUpdateSink(
    VideoSinkInterface<webrtc::VideoFrame>* sink,
    const VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);

  SinkPair* sink_pair = FindSinkPair(sink);
  if (!sink_pair) {
    sinks_.push_back(SinkPair(sink, wants));
  } else {
    sink_pair->wants = wants;
  }
}

void VideoSourceBase::RemoveSink(VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);
  RTC_DCHECK(FindSinkPair(sink));
  sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
                              [sink](const SinkPair& sink_pair) {
                                return sink_pair.sink == sink;
                              }),
               sinks_.end());
}

VideoSourceBase::SinkPair* VideoSourceBase::FindSinkPair(
    const VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  auto sink_pair_it = std::find_if(
      sinks_.begin(), sinks_.end(),
      [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });
  if (sink_pair_it != sinks_.end()) {
    return &*sink_pair_it;
  }
  return nullptr;
}

}  // namespace rtc
