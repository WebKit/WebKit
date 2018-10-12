/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/frame_generator_capturer.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "call/video_send_stream.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class FrameGeneratorCapturer::InsertFrameTask : public rtc::QueuedTask {
 public:
  explicit InsertFrameTask(FrameGeneratorCapturer* frame_generator_capturer)
      : frame_generator_capturer_(frame_generator_capturer),
        repeat_interval_ms_(-1),
        next_run_time_ms_(-1) {}

 private:
  bool Run() override {
    // Check if the frame interval for this
    // task queue is the same same as the current configured frame rate.
    int interval_ms =
        1000 / frame_generator_capturer_->GetCurrentConfiguredFramerate();
    if (repeat_interval_ms_ != interval_ms) {
      // Restart the timer if frame rate has changed since task was started.
      next_run_time_ms_ = rtc::TimeMillis();
      repeat_interval_ms_ = interval_ms;
    }
    // Schedule the next frame capture event to happen at approximately the
    // correct absolute time point.
    next_run_time_ms_ += interval_ms;

    frame_generator_capturer_->InsertFrame();

    int64_t now_ms = rtc::TimeMillis();
    if (next_run_time_ms_ < now_ms) {
      RTC_LOG(LS_ERROR) << "Frame Generator Capturer can't keep up with "
                           "requested fps.";
      rtc::TaskQueue::Current()->PostTask(absl::WrapUnique(this));
    } else {
      int64_t delay_ms = next_run_time_ms_ - now_ms;
      RTC_DCHECK_GE(delay_ms, 0);
      RTC_DCHECK_LE(delay_ms, interval_ms);
      rtc::TaskQueue::Current()->PostDelayedTask(absl::WrapUnique(this),
                                                 delay_ms);
    }
    return false;
  }

  webrtc::test::FrameGeneratorCapturer* const frame_generator_capturer_;
  int repeat_interval_ms_;
  int64_t next_run_time_ms_;
};

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(
    int width,
    int height,
    absl::optional<FrameGenerator::OutputType> type,
    absl::optional<int> num_squares,
    int target_fps,
    Clock* clock) {
  auto capturer = absl::make_unique<FrameGeneratorCapturer>(
      clock,
      FrameGenerator::CreateSquareGenerator(width, height, type, num_squares),
      target_fps);
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
  auto capturer = absl::make_unique<FrameGeneratorCapturer>(
      clock,
      FrameGenerator::CreateFromYuvFile(std::vector<std::string>(1, file_name),
                                        width, height, 1),
      target_fps);
  if (!capturer->Init())
    return nullptr;

  return capturer.release();
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateSlideGenerator(
    int width,
    int height,
    int frame_repeat_count,
    int target_fps,
    Clock* clock) {
  auto capturer = absl::make_unique<FrameGeneratorCapturer>(
      clock,
      FrameGenerator::CreateSlideGenerator(width, height, frame_repeat_count),
      target_fps);
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
      source_fps_(target_fps),
      target_capture_fps_(target_fps),
      first_frame_capture_time_(-1),
      task_queue_("FrameGenCapQ", rtc::TaskQueue::Priority::HIGH) {
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
  task_queue_.PostDelayedTask(absl::make_unique<InsertFrameTask>(this),
                              1000 / framerate_fps);

  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  rtc::CritScope cs(&lock_);
  if (sending_) {
    VideoFrame* frame = frame_generator_->NextFrame();
    // TODO(srte): Use more advanced frame rate control to allow arbritrary
    // fractions.
    int decimation =
        std::round(static_cast<double>(source_fps_) / target_capture_fps_);
    for (int i = 1; i < decimation; ++i)
      frame = frame_generator_->NextFrame();
    frame->set_timestamp_us(clock_->TimeInMicroseconds());
    frame->set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
    frame->set_rotation(fake_rotation_);
    if (first_frame_capture_time_ == -1) {
      first_frame_capture_time_ = frame->ntp_time_ms();
    }

    if (sink_) {
      absl::optional<VideoFrame> out_frame = AdaptFrame(*frame);
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

void FrameGeneratorCapturer::ChangeFramerate(int target_framerate) {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(target_capture_fps_ > 0);
  if (target_framerate > source_fps_)
    RTC_LOG(LS_WARNING) << "Target framerate clamped from " << target_framerate
                        << " to " << source_fps_;
  if (source_fps_ % target_capture_fps_ != 0) {
    int decimation =
        std::round(static_cast<double>(source_fps_) / target_capture_fps_);
    int effective_rate = target_capture_fps_ / decimation;
    RTC_LOG(LS_WARNING) << "Target framerate, " << target_framerate
                        << ", is an uneven fraction of the source rate, "
                        << source_fps_
                        << ". The framerate will be :" << effective_rate;
  }
  target_capture_fps_ = std::min(source_fps_, target_framerate);
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
  TestVideoCapturer::AddOrUpdateSink(sink, resolution_wants);

  // Ignore any requests for framerate higher than initially configured.
  if (wants.max_framerate_fps < target_capture_fps_) {
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
  task_queue_.PostTask([this] { InsertFrame(); });
}

int FrameGeneratorCapturer::GetCurrentConfiguredFramerate() {
  rtc::CritScope cs(&lock_);
  if (wanted_fps_ && *wanted_fps_ < target_capture_fps_)
    return *wanted_fps_;
  return target_capture_fps_;
}

}  // namespace test
}  // namespace webrtc
