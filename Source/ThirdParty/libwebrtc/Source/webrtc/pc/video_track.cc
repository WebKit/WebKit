/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_track.h"

#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_stream_track.h"
#include "api/notifier.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "pc/video_track_source_proxy.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"

namespace webrtc {

VideoTrack::VideoTrack(
    absl::string_view label,
    scoped_refptr<VideoTrackSourceProxyWithInternal<VideoTrackSourceInterface>>
        source,
    Thread* worker_thread)
    : MediaStreamTrack<VideoTrackInterface>(label),
      worker_thread_(worker_thread),
      video_source_(std::move(source)),
      content_hint_(ContentHint::kNone) {
  RTC_DCHECK_RUN_ON(&signaling_thread_);
  video_source_->RegisterObserver(this);
}

VideoTrack::~VideoTrack() {
  RTC_DCHECK_RUN_ON(&signaling_thread_);
  video_source_->UnregisterObserver(this);
}

std::string VideoTrack::kind() const {
  return kVideoKind;
}

// AddOrUpdateSink and RemoveSink should be called on the worker
// thread.
void VideoTrack::AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                                 const VideoSinkWants& wants) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(sink != nullptr);
  SinkPair* sink_pair = FindSinkPair(sink);
  if (!sink_pair) {
    sinks_.push_back(SinkPair(sink, wants));
  } else {
    sink_pair->wants = wants;
  }
  VideoSinkWants modified_wants = wants;
  modified_wants.black_frames = !enabled_w_;
  video_source_->internal()->AddOrUpdateSink(sink, modified_wants);
}

void VideoTrack::RemoveSink(VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(sink != nullptr);
  RTC_DCHECK(FindSinkPair(sink));
  std::erase_if(sinks_, [sink](const SinkPair& sink_pair) {
    return sink_pair.sink == sink;
  });
  video_source_->internal()->RemoveSink(sink);
}

void VideoTrack::RequestRefreshFrame() {
  RTC_DCHECK_RUN_ON(worker_thread_);
  video_source_->internal()->RequestRefreshFrame();
}

VideoTrackSourceInterface* VideoTrack::GetSource() const {
  // Callable from any thread.
  return video_source_.get();
}

VideoTrackSourceInterface* VideoTrack::GetSourceInternal() const {
  return video_source_->internal();
}

VideoTrackInterface::ContentHint VideoTrack::content_hint() const {
  RTC_DCHECK_RUN_ON(&signaling_thread_);
  return content_hint_;
}

void VideoTrack::set_content_hint(ContentHint hint) {
  RTC_DCHECK_RUN_ON(&signaling_thread_);
  if (content_hint_ == hint)
    return;
  content_hint_ = hint;
  Notifier<VideoTrackInterface>::FireOnChanged();
}

bool VideoTrack::set_enabled(bool enable) {
  RTC_DCHECK_RUN_ON(&signaling_thread_);

  bool ret = MediaStreamTrack<VideoTrackInterface>::set_enabled(enable);

  worker_thread_->BlockingCall([&]() {
    RTC_DCHECK_RUN_ON(worker_thread_);
    enabled_w_ = enable;
    for (auto& sink_pair : sinks_) {
      VideoSinkWants modified_wants = sink_pair.wants;
      modified_wants.black_frames = !enable;
      video_source_->AddOrUpdateSink(sink_pair.sink, modified_wants);
    }
  });

  return ret;
}

bool VideoTrack::enabled() const {
  if (worker_thread_->IsCurrent()) {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return enabled_w_;
  }
  RTC_DCHECK_RUN_ON(&signaling_thread_);
  return MediaStreamTrack<VideoTrackInterface>::enabled();
}

MediaStreamTrackInterface::TrackState VideoTrack::state() const {
  RTC_DCHECK_RUN_ON(worker_thread_);
  return MediaStreamTrack<VideoTrackInterface>::state();
}

void VideoTrack::OnChanged() {
  RTC_DCHECK_RUN_ON(&signaling_thread_);
  Thread::ScopedDisallowBlockingCalls no_blocking_calls;
  MediaSourceInterface::SourceState state = video_source_->state();
  set_state(state == MediaSourceInterface::kEnded ? kEnded : kLive);
}

scoped_refptr<VideoTrack> VideoTrack::Create(
    absl::string_view id,
    scoped_refptr<VideoTrackSourceInterface> source,
    Thread* worker_thread) {
  scoped_refptr<VideoTrackSourceProxyWithInternal<VideoTrackSourceInterface>>
      source_proxy = VideoTrackSourceProxy::Create(
          Thread::Current(), worker_thread, std::move(source));

  return make_ref_counted<VideoTrack>(id, std::move(source_proxy),
                                      worker_thread);
}

VideoTrack::SinkPair* VideoTrack::FindSinkPair(
    const VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto sink_pair_it = absl::c_find_if(
      sinks_,
      [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });
  if (sink_pair_it != sinks_.end()) {
    return &*sink_pair_it;
  }
  return nullptr;
}

}  // namespace webrtc
