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

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/test/create_frame_generator.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {
std::string TransformFilePath(std::string path) {
  static const std::string resource_prefix = "res://";
  int ext_pos = path.rfind('.');
  if (ext_pos < 0) {
    return test::ResourcePath(path, "yuv");
  } else if (absl::StartsWith(path, resource_prefix)) {
    std::string name = path.substr(resource_prefix.length(), ext_pos);
    std::string ext = path.substr(ext_pos, path.size());
    return test::ResourcePath(name, ext);
  }
  return path;
}
}  // namespace

FrameGeneratorCapturer::FrameGeneratorCapturer(
    Clock* clock,
    std::unique_ptr<FrameGeneratorInterface> frame_generator,
    int target_fps,
    TaskQueueFactory& task_queue_factory)
    : clock_(clock),
      sending_(true),
      sink_wants_observer_(nullptr),
      frame_generator_(std::move(frame_generator)),
      source_fps_(target_fps),
      target_capture_fps_(target_fps),
      first_frame_capture_time_(-1),
      task_queue_(task_queue_factory.CreateTaskQueue(
          "FrameGenCapQ",
          TaskQueueFactory::Priority::HIGH)) {
  RTC_DCHECK(frame_generator_);
  RTC_DCHECK_GT(target_fps, 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();
}

std::unique_ptr<FrameGeneratorCapturer> FrameGeneratorCapturer::Create(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::SquaresVideo config) {
  return std::make_unique<FrameGeneratorCapturer>(
      clock,
      CreateSquareFrameGenerator(config.width, config.height,
                                 config.pixel_format, config.num_squares),
      config.framerate, task_queue_factory);
}
std::unique_ptr<FrameGeneratorCapturer> FrameGeneratorCapturer::Create(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::SquareSlides config) {
  return std::make_unique<FrameGeneratorCapturer>(
      clock,
      CreateSlideFrameGenerator(
          config.width, config.height,
          /*frame_repeat_count*/ config.change_interval.seconds<double>() *
              config.framerate),
      config.framerate, task_queue_factory);
}
std::unique_ptr<FrameGeneratorCapturer> FrameGeneratorCapturer::Create(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::VideoFile config) {
  RTC_CHECK(config.width && config.height);
  return std::make_unique<FrameGeneratorCapturer>(
      clock,
      CreateFromYuvFileFrameGenerator({TransformFilePath(config.name)},
                                      config.width, config.height,
                                      /*frame_repeat_count*/ 1),
      config.framerate, task_queue_factory);
}

std::unique_ptr<FrameGeneratorCapturer> FrameGeneratorCapturer::Create(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::ImageSlides config) {
  std::unique_ptr<FrameGeneratorInterface> slides_generator;
  std::vector<std::string> paths = config.paths;
  for (std::string& path : paths)
    path = TransformFilePath(path);

  if (config.crop.width || config.crop.height) {
    TimeDelta pause_duration =
        config.change_interval - config.crop.scroll_duration;
    RTC_CHECK_GE(pause_duration, TimeDelta::Zero());
    int crop_width = config.crop.width.value_or(config.width);
    int crop_height = config.crop.height.value_or(config.height);
    RTC_CHECK_LE(crop_width, config.width);
    RTC_CHECK_LE(crop_height, config.height);
    slides_generator = CreateScrollingInputFromYuvFilesFrameGenerator(
        clock, paths, config.width, config.height, crop_width, crop_height,
        config.crop.scroll_duration.ms(), pause_duration.ms());
  } else {
    slides_generator = CreateFromYuvFileFrameGenerator(
        paths, config.width, config.height,
        /*frame_repeat_count*/ config.change_interval.seconds<double>() *
            config.framerate);
  }
  return std::make_unique<FrameGeneratorCapturer>(
      clock, std::move(slides_generator), config.framerate, task_queue_factory);
}

std::unique_ptr<FrameGeneratorCapturer> FrameGeneratorCapturer::Create(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    const FrameGeneratorCapturerConfig& config) {
  if (config.video_file) {
    return Create(clock, task_queue_factory, *config.video_file);
  } else if (config.image_slides) {
    return Create(clock, task_queue_factory, *config.image_slides);
  } else if (config.squares_slides) {
    return Create(clock, task_queue_factory, *config.squares_slides);
  } else {
    return Create(clock, task_queue_factory,
                  config.squares_video.value_or(
                      FrameGeneratorCapturerConfig::SquaresVideo()));
  }
}

void FrameGeneratorCapturer::SetFakeRotation(VideoRotation rotation) {
  MutexLock lock(&lock_);
  fake_rotation_ = rotation;
}

void FrameGeneratorCapturer::SetFakeColorSpace(
    absl::optional<ColorSpace> color_space) {
  MutexLock lock(&lock_);
  fake_color_space_ = color_space;
}

bool FrameGeneratorCapturer::Init() {
  // This check is added because frame_generator_ might be file based and should
  // not crash because a file moved.
  if (frame_generator_.get() == nullptr)
    return false;

  frame_task_ = RepeatingTaskHandle::DelayedStart(
      task_queue_.Get(),
      TimeDelta::Seconds(1) / GetCurrentConfiguredFramerate(), [this] {
        InsertFrame();
        return TimeDelta::Seconds(1) / GetCurrentConfiguredFramerate();
      });
  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  MutexLock lock(&lock_);
  if (sending_) {
    FrameGeneratorInterface::VideoFrameData frame_data =
        frame_generator_->NextFrame();
    // TODO(srte): Use more advanced frame rate control to allow arbritrary
    // fractions.
    int decimation =
        std::round(static_cast<double>(source_fps_) / target_capture_fps_);
    for (int i = 1; i < decimation; ++i)
      frame_data = frame_generator_->NextFrame();

    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(frame_data.buffer)
                           .set_rotation(fake_rotation_)
                           .set_timestamp_us(clock_->TimeInMicroseconds())
                           .set_ntp_time_ms(clock_->CurrentNtpInMilliseconds())
                           .set_update_rect(frame_data.update_rect)
                           .set_color_space(fake_color_space_)
                           .build();
    if (first_frame_capture_time_ == -1) {
      first_frame_capture_time_ = frame.ntp_time_ms();
    }

    TestVideoCapturer::OnFrame(frame);
  }
}

void FrameGeneratorCapturer::Start() {
  {
    MutexLock lock(&lock_);
    sending_ = true;
  }
  if (!frame_task_.Running()) {
    frame_task_ = RepeatingTaskHandle::Start(task_queue_.Get(), [this] {
      InsertFrame();
      return TimeDelta::Seconds(1) / GetCurrentConfiguredFramerate();
    });
  }
}

void FrameGeneratorCapturer::Stop() {
  MutexLock lock(&lock_);
  sending_ = false;
}

void FrameGeneratorCapturer::ChangeResolution(size_t width, size_t height) {
  MutexLock lock(&lock_);
  frame_generator_->ChangeResolution(width, height);
}

void FrameGeneratorCapturer::ChangeFramerate(int target_framerate) {
  MutexLock lock(&lock_);
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
  MutexLock lock(&lock_);
  RTC_DCHECK(!sink_wants_observer_);
  sink_wants_observer_ = observer;
}

void FrameGeneratorCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  TestVideoCapturer::AddOrUpdateSink(sink, wants);
  MutexLock lock(&lock_);
  if (sink_wants_observer_) {
    // Tests need to observe unmodified sink wants.
    sink_wants_observer_->OnSinkWantsChanged(sink, wants);
  }
  UpdateFps(GetSinkWants().max_framerate_fps);
}

void FrameGeneratorCapturer::RemoveSink(
    rtc::VideoSinkInterface<VideoFrame>* sink) {
  TestVideoCapturer::RemoveSink(sink);

  MutexLock lock(&lock_);
  UpdateFps(GetSinkWants().max_framerate_fps);
}

void FrameGeneratorCapturer::UpdateFps(int max_fps) {
  if (max_fps < target_capture_fps_) {
    wanted_fps_.emplace(max_fps);
  } else {
    wanted_fps_.reset();
  }
}

void FrameGeneratorCapturer::ForceFrame() {
  // One-time non-repeating task,
  task_queue_.PostTask([this] { InsertFrame(); });
}

int FrameGeneratorCapturer::GetCurrentConfiguredFramerate() {
  MutexLock lock(&lock_);
  if (wanted_fps_ && *wanted_fps_ < target_capture_fps_)
    return *wanted_fps_;
  return target_capture_fps_;
}

}  // namespace test
}  // namespace webrtc
