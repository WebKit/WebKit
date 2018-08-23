/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/incoming_video_stream.h"

#include <memory>

#include "common_video/video_render_frames.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"

namespace webrtc {
namespace {
const char kIncomingQueueName[] = "IncomingVideoStream";
}

// Capture by moving (std::move) into a lambda isn't possible in C++11
// (supported in C++14). This class provides the functionality of what would be
// something like (inside OnFrame):
// VideoFrame frame(video_frame);
// incoming_render_queue_.PostTask([this, frame = std::move(frame)](){
//   if (render_buffers_.AddFrame(std::move(frame)) == 1)
//     Dequeue();
// });
class IncomingVideoStream::NewFrameTask : public rtc::QueuedTask {
 public:
  NewFrameTask(IncomingVideoStream* stream, VideoFrame frame)
      : stream_(stream), frame_(std::move(frame)) {}

 private:
  bool Run() override {
    RTC_DCHECK(stream_->incoming_render_queue_.IsCurrent());
    if (stream_->render_buffers_.AddFrame(std::move(frame_)) == 1)
      stream_->Dequeue();
    return true;
  }

  IncomingVideoStream* stream_;
  VideoFrame frame_;
};

IncomingVideoStream::IncomingVideoStream(
    int32_t delay_ms,
    rtc::VideoSinkInterface<VideoFrame>* callback)
    : render_buffers_(delay_ms),
      callback_(callback),
      incoming_render_queue_(kIncomingQueueName,
                             rtc::TaskQueue::Priority::HIGH) {}

IncomingVideoStream::~IncomingVideoStream() {
  RTC_DCHECK(main_thread_checker_.CalledOnValidThread());
}

void IncomingVideoStream::OnFrame(const VideoFrame& video_frame) {
  TRACE_EVENT0("webrtc", "IncomingVideoStream::OnFrame");
  RTC_CHECK_RUNS_SERIALIZED(&decoder_race_checker_);
  RTC_DCHECK(!incoming_render_queue_.IsCurrent());
  incoming_render_queue_.PostTask(
      std::unique_ptr<rtc::QueuedTask>(new NewFrameTask(this, video_frame)));
}

void IncomingVideoStream::Dequeue() {
  TRACE_EVENT0("webrtc", "IncomingVideoStream::Dequeue");
  RTC_DCHECK(incoming_render_queue_.IsCurrent());
  absl::optional<VideoFrame> frame_to_render = render_buffers_.FrameToRender();
  if (frame_to_render)
    callback_->OnFrame(*frame_to_render);

  if (render_buffers_.HasPendingFrames()) {
    uint32_t wait_time = render_buffers_.TimeToNextFrameRelease();
    incoming_render_queue_.PostDelayedTask([this]() { Dequeue(); }, wait_time);
  }
}

}  // namespace webrtc
