/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/frame_generator_capturer.h"

#include <utility>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

class FrameGeneratorCapturer::InsertFrameTask : public rtc::QueuedTask {
 public:
  // Repeats in |repeat_interval_ms|. One-time if |repeat_interval_ms| == 0.
  InsertFrameTask(
      webrtc::test::FrameGeneratorCapturer* frame_generator_capturer,
      uint32_t repeat_interval_ms)
      : frame_generator_capturer_(frame_generator_capturer),
        repeat_interval_ms_(repeat_interval_ms),
        intended_run_time_ms_(-1) {}

 private:
  bool Run() override {
    bool task_completed = true;
    if (repeat_interval_ms_ > 0) {
      // This is not a one-off frame. Check if the frame interval for this
      // task queue is the same same as the current configured frame rate.
      uint32_t current_interval_ms =
          1000 / frame_generator_capturer_->GetCurrentConfiguredFramerate();
      if (repeat_interval_ms_ != current_interval_ms) {
        // Frame rate has changed since task was started, create a new instance.
        rtc::TaskQueue::Current()->PostDelayedTask(
            std::unique_ptr<rtc::QueuedTask>(new InsertFrameTask(
                frame_generator_capturer_, current_interval_ms)),
            current_interval_ms);
      } else {
        // Schedule the next frame capture event to happen at approximately the
        // correct absolute time point.
        int64_t delay_ms;
        int64_t time_now_ms = rtc::TimeMillis();
        if (intended_run_time_ms_ > 0) {
          delay_ms = time_now_ms - intended_run_time_ms_;
        } else {
          delay_ms = 0;
          intended_run_time_ms_ = time_now_ms;
        }
        intended_run_time_ms_ += repeat_interval_ms_;
        if (delay_ms < repeat_interval_ms_) {
          rtc::TaskQueue::Current()->PostDelayedTask(
              std::unique_ptr<rtc::QueuedTask>(this),
              repeat_interval_ms_ - delay_ms);
        } else {
          rtc::TaskQueue::Current()->PostDelayedTask(
              std::unique_ptr<rtc::QueuedTask>(this), 0);
          LOG(LS_ERROR)
              << "Frame Generator Capturer can't keep up with requested fps";
        }
        // Repost of this instance, make sure it is not deleted.
        task_completed = false;
      }
    }
    frame_generator_capturer_->InsertFrame();
    // Task should be deleted only if it's not repeating.
    return task_completed;
  }

  webrtc::test::FrameGeneratorCapturer* const frame_generator_capturer_;
  const uint32_t repeat_interval_ms_;
  int64_t intended_run_time_ms_;
};

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(int width,
                                                       int height,
                                                       int target_fps,
                                                       Clock* clock) {
  std::unique_ptr<FrameGeneratorCapturer> capturer(new FrameGeneratorCapturer(
      clock, FrameGenerator::CreateSquareGenerator(width, height), target_fps));
  if (!capturer->Init())
    return nullptr;

  return capturer.release();
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateFromYuvFile(
    const std::string& file_name,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  std::unique_ptr<FrameGeneratorCapturer> capturer(new FrameGeneratorCapturer(
      clock,
      FrameGenerator::CreateFromYuvFile(std::vector<std::string>(1, file_name),
                                        width, height, 1),
      target_fps));
  if (!capturer->Init())
    return nullptr;

  return capturer.release();
}

FrameGeneratorCapturer::FrameGeneratorCapturer(
    Clock* clock,
    std::unique_ptr<FrameGenerator> frame_generator,
    int target_fps)
    : clock_(clock),
      sending_(false),
      sink_(nullptr),
      sink_wants_observer_(nullptr),
      frame_generator_(std::move(frame_generator)),
      target_fps_(target_fps),
      first_frame_capture_time_(-1),
      task_queue_("FrameGenCapQ",
                  rtc::TaskQueue::Priority::HIGH) {
  RTC_DCHECK(frame_generator_);
  RTC_DCHECK_GT(target_fps, 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();
}

void FrameGeneratorCapturer::SetFakeRotation(VideoRotation rotation) {
  rtc::CritScope cs(&lock_);
  fake_rotation_ = rotation;
}

bool FrameGeneratorCapturer::Init() {
  // This check is added because frame_generator_ might be file based and should
  // not crash because a file moved.
  if (frame_generator_.get() == nullptr)
    return false;

  int framerate_fps = GetCurrentConfiguredFramerate();
  task_queue_.PostDelayedTask(
      std::unique_ptr<rtc::QueuedTask>(
          new InsertFrameTask(this, 1000 / framerate_fps)),
      1000 / framerate_fps);

  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  rtc::CritScope cs(&lock_);
  if (sending_) {
    VideoFrame* frame = frame_generator_->NextFrame();
    frame->set_timestamp_us(clock_->TimeInMicroseconds());
    frame->set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
    frame->set_rotation(fake_rotation_);
    if (first_frame_capture_time_ == -1) {
      first_frame_capture_time_ = frame->ntp_time_ms();
    }

    if (sink_) {
      rtc::Optional<VideoFrame> out_frame = AdaptFrame(*frame);
      if (out_frame)
        sink_->OnFrame(*out_frame);
    }
  }
}

void FrameGeneratorCapturer::Start() {
  rtc::CritScope cs(&lock_);
  sending_ = true;
}

void FrameGeneratorCapturer::Stop() {
  rtc::CritScope cs(&lock_);
  sending_ = false;
}

void FrameGeneratorCapturer::ChangeResolution(size_t width, size_t height) {
  rtc::CritScope cs(&lock_);
  frame_generator_->ChangeResolution(width, height);
}

void FrameGeneratorCapturer::SetSinkWantsObserver(SinkWantsObserver* observer) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(!sink_wants_observer_);
  sink_wants_observer_ = observer;
}

void FrameGeneratorCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(!sink_ || sink_ == sink);
  sink_ = sink;
  if (sink_wants_observer_)
    sink_wants_observer_->OnSinkWantsChanged(sink, wants);

  // Handle framerate within this class, just pass on resolution for possible
  // adaptation.
  rtc::VideoSinkWants resolution_wants = wants;
  resolution_wants.max_framerate_fps = std::numeric_limits<int>::max();
  VideoCapturer::AddOrUpdateSink(sink, resolution_wants);

  // Ignore any requests for framerate higher than initially configured.
  if (wants.max_framerate_fps < target_fps_) {
    wanted_fps_.emplace(wants.max_framerate_fps);
  } else {
    wanted_fps_.reset();
  }
}

void FrameGeneratorCapturer::RemoveSink(
    rtc::VideoSinkInterface<VideoFrame>* sink) {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(sink_ == sink);
  sink_ = nullptr;
}

void FrameGeneratorCapturer::ForceFrame() {
  // One-time non-repeating task,
  // therefore repeat_interval_ms is 0 in InsertFrameTask()
  task_queue_.PostTask(
      std::unique_ptr<rtc::QueuedTask>(new InsertFrameTask(this, 0)));
}

int FrameGeneratorCapturer::GetCurrentConfiguredFramerate() {
  rtc::CritScope cs(&lock_);
  if (wanted_fps_ && *wanted_fps_ < target_fps_)
    return *wanted_fps_;
  return target_fps_;
}

}  // namespace test
}  // namespace webrtc
