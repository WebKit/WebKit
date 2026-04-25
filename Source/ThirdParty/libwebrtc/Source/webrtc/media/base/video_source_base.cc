/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/video_source_base.h"

#include <vector>

#include "absl/algorithm/container.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/checks.h"

namespace webrtc {

VideoSourceBase::VideoSourceBase() = default;
VideoSourceBase::~VideoSourceBase() = default;

void VideoSourceBase::AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                                      const VideoSinkWants& wants) {
  RTC_DCHECK(sink != nullptr);

  SinkPair* sink_pair = FindSinkPair(sink);
  if (!sink_pair) {
    sinks_.push_back(SinkPair(sink, wants));
  } else {
    sink_pair->wants = wants;
  }
}

void VideoSourceBase::RemoveSink(VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK(sink != nullptr);
  RTC_DCHECK(FindSinkPair(sink));
  std::erase_if(sinks_, [sink](const SinkPair& sink_pair) {
    return sink_pair.sink == sink;
  });
}

VideoSourceBase::SinkPair* VideoSourceBase::FindSinkPair(
    const VideoSinkInterface<VideoFrame>* sink) {
  auto sink_pair_it = absl::c_find_if(
      sinks_,
      [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });
  if (sink_pair_it != sinks_.end()) {
    return &*sink_pair_it;
  }
  return nullptr;
}

VideoSourceBaseGuarded::VideoSourceBaseGuarded() = default;
VideoSourceBaseGuarded::~VideoSourceBaseGuarded() = default;

void VideoSourceBaseGuarded::AddOrUpdateSink(
    VideoSinkInterface<VideoFrame>* sink,
    const VideoSinkWants& wants) {
  RTC_DCHECK_RUN_ON(&source_sequence_);
  RTC_DCHECK(sink != nullptr);

  SinkPair* sink_pair = FindSinkPair(sink);
  if (!sink_pair) {
    sinks_.push_back(SinkPair(sink, wants));
  } else {
    sink_pair->wants = wants;
  }
}

void VideoSourceBaseGuarded::RemoveSink(VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(&source_sequence_);
  RTC_DCHECK(sink != nullptr);
  RTC_DCHECK(FindSinkPair(sink));
  std::erase_if(sinks_, [sink](const SinkPair& sink_pair) {
    return sink_pair.sink == sink;
  });
}

VideoSourceBaseGuarded::SinkPair* VideoSourceBaseGuarded::FindSinkPair(
    const VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(&source_sequence_);
  auto sink_pair_it = absl::c_find_if(
      sinks_,
      [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });
  if (sink_pair_it != sinks_.end()) {
    return &*sink_pair_it;
  }
  return nullptr;
}

const std::vector<VideoSourceBaseGuarded::SinkPair>&
VideoSourceBaseGuarded::sink_pairs() const {
  RTC_DCHECK_RUN_ON(&source_sequence_);
  return sinks_;
}

}  // namespace webrtc
