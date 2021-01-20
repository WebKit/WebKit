/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/video_frame_matcher.h"

#include <utility>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kThumbWidth = 96;
constexpr int kThumbHeight = 96;
}  // namespace

VideoFrameMatcher::VideoFrameMatcher(
    std::vector<std::function<void(const VideoFramePair&)> >
        frame_pair_handlers)
    : frame_pair_handlers_(std::move(frame_pair_handlers)),
      task_queue_("VideoAnalyzer") {}

VideoFrameMatcher::~VideoFrameMatcher() {
  task_queue_.SendTask([this] { Finalize(); }, RTC_FROM_HERE);
}

void VideoFrameMatcher::RegisterLayer(int layer_id) {
  task_queue_.PostTask([this, layer_id] { layers_[layer_id] = VideoLayer(); });
}

void VideoFrameMatcher::OnCapturedFrame(const VideoFrame& frame,
                                        Timestamp at_time) {
  CapturedFrame captured;
  captured.id = next_capture_id_++;
  captured.capture_time = at_time;
  captured.frame = frame.video_frame_buffer();
  captured.thumb = ScaleVideoFrameBuffer(*frame.video_frame_buffer()->ToI420(),
                                         kThumbWidth, kThumbHeight),
  task_queue_.PostTask([this, captured]() {
    for (auto& layer : layers_) {
      CapturedFrame copy = captured;
      if (layer.second.last_decode &&
          layer.second.last_decode->frame->width() <= captured.frame->width()) {
        copy.best_score = I420SSE(*captured.thumb->GetI420(),
                                  *layer.second.last_decode->thumb->GetI420());
        copy.best_decode = layer.second.last_decode;
      }
      layer.second.captured_frames.push_back(std::move(copy));
    }
  });
}

void VideoFrameMatcher::OnDecodedFrame(const VideoFrame& frame,
                                       int layer_id,
                                       Timestamp render_time,
                                       Timestamp at_time) {
  rtc::scoped_refptr<DecodedFrame> decoded(new DecodedFrame{});
  decoded->decoded_time = at_time;
  decoded->render_time = render_time;
  decoded->frame = frame.video_frame_buffer();
  decoded->thumb = ScaleVideoFrameBuffer(*frame.video_frame_buffer()->ToI420(),
                                         kThumbWidth, kThumbHeight);

  task_queue_.PostTask([this, decoded, layer_id] {
    auto& layer = layers_[layer_id];
    decoded->id = layer.next_decoded_id++;
    layer.last_decode = decoded;
    for (auto& captured : layer.captured_frames) {
      // We can't match with a smaller capture.
      if (captured.frame->width() < decoded->frame->width()) {
        captured.matched = true;
        continue;
      }
      double score =
          I420SSE(*captured.thumb->GetI420(), *decoded->thumb->GetI420());
      if (score < captured.best_score) {
        captured.best_score = score;
        captured.best_decode = decoded;
        captured.matched = false;
      } else {
        captured.matched = true;
      }
    }
    while (!layer.captured_frames.empty() &&
           layer.captured_frames.front().matched) {
      HandleMatch(std::move(layer.captured_frames.front()), layer_id);
      layer.captured_frames.pop_front();
    }
  });
}

bool VideoFrameMatcher::Active() const {
  return !frame_pair_handlers_.empty();
}

void VideoFrameMatcher::HandleMatch(VideoFrameMatcher::CapturedFrame captured,
                                    int layer_id) {
  VideoFramePair frame_pair;
  frame_pair.layer_id = layer_id;
  frame_pair.captured = captured.frame;
  frame_pair.capture_id = captured.id;
  frame_pair.capture_time = captured.capture_time;
  if (captured.best_decode) {
    frame_pair.decode_id = captured.best_decode->id;
    frame_pair.decoded = captured.best_decode->frame;
    frame_pair.decoded_time = captured.best_decode->decoded_time;
    // We can't render frames before they have been decoded.
    frame_pair.render_time = std::max(captured.best_decode->render_time,
                                      captured.best_decode->decoded_time);
    frame_pair.repeated = captured.best_decode->repeat_count++;
  }
  for (auto& handler : frame_pair_handlers_)
    handler(frame_pair);
}

void VideoFrameMatcher::Finalize() {
  for (auto& layer : layers_) {
    while (!layer.second.captured_frames.empty()) {
      HandleMatch(std::move(layer.second.captured_frames.front()), layer.first);
      layer.second.captured_frames.pop_front();
    }
  }
}

CapturedFrameTap::CapturedFrameTap(Clock* clock, VideoFrameMatcher* matcher)
    : clock_(clock), matcher_(matcher) {}

void CapturedFrameTap::OnFrame(const VideoFrame& frame) {
  matcher_->OnCapturedFrame(frame, clock_->CurrentTime());
}
void CapturedFrameTap::OnDiscardedFrame() {
  discarded_count_++;
}

ForwardingCapturedFrameTap::ForwardingCapturedFrameTap(
    Clock* clock,
    VideoFrameMatcher* matcher,
    rtc::VideoSourceInterface<VideoFrame>* source)
    : clock_(clock), matcher_(matcher), source_(source) {}

void ForwardingCapturedFrameTap::OnFrame(const VideoFrame& frame) {
  RTC_CHECK(sink_);
  matcher_->OnCapturedFrame(frame, clock_->CurrentTime());
  sink_->OnFrame(frame);
}
void ForwardingCapturedFrameTap::OnDiscardedFrame() {
  RTC_CHECK(sink_);
  discarded_count_++;
  sink_->OnDiscardedFrame();
}

void ForwardingCapturedFrameTap::AddOrUpdateSink(
    VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  if (!sink_)
    sink_ = sink;
  RTC_DCHECK_EQ(sink_, sink);
  source_->AddOrUpdateSink(this, wants);
}
void ForwardingCapturedFrameTap::RemoveSink(
    VideoSinkInterface<VideoFrame>* sink) {
  source_->RemoveSink(this);
  sink_ = nullptr;
}

DecodedFrameTap::DecodedFrameTap(Clock* clock,
                                 VideoFrameMatcher* matcher,
                                 int layer_id)
    : clock_(clock), matcher_(matcher), layer_id_(layer_id) {
  matcher_->RegisterLayer(layer_id_);
}

void DecodedFrameTap::OnFrame(const VideoFrame& frame) {
  matcher_->OnDecodedFrame(frame, layer_id_,
                           Timestamp::Millis(frame.render_time_ms()),
                           clock_->CurrentTime());
}

}  // namespace test
}  // namespace webrtc
