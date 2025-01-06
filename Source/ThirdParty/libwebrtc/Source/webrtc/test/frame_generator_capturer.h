/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_FRAME_GENERATOR_CAPTURER_H_
#define TEST_FRAME_GENERATOR_CAPTURER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/color_space.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "test/test_video_capturer.h"

namespace webrtc {
namespace test {

class FrameGeneratorCapturer : public TestVideoCapturer {
 public:
  class SinkWantsObserver {
   public:
    // OnSinkWantsChanged is called when FrameGeneratorCapturer::AddOrUpdateSink
    // is called.
    virtual void OnSinkWantsChanged(rtc::VideoSinkInterface<VideoFrame>* sink,
                                    const rtc::VideoSinkWants& wants) = 0;

   protected:
    virtual ~SinkWantsObserver() {}
  };

  FrameGeneratorCapturer(
      Clock* clock,
      std::unique_ptr<FrameGeneratorInterface> frame_generator,
      int target_fps,
      TaskQueueFactory& task_queue_factory);
  virtual ~FrameGeneratorCapturer();

  void Start() override;
  void Stop() override;
  void ChangeResolution(size_t width, size_t height);
  void ChangeFramerate(int target_framerate);

  int GetFrameWidth() const override;
  int GetFrameHeight() const override;

  struct Resolution {
    int width;
    int height;
  };
  std::optional<Resolution> GetResolution() const;

  void OnOutputFormatRequest(int width,
                             int height,
                             const std::optional<int>& max_fps);

  void SetSinkWantsObserver(SinkWantsObserver* observer);

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

  void ForceFrame();
  void SetFakeRotation(VideoRotation rotation);
  void SetFakeColorSpace(std::optional<ColorSpace> color_space);

  bool Init();

 private:
  void InsertFrame();
  static bool Run(void* obj);
  int GetCurrentConfiguredFramerate();

  Clock* const clock_;
  RepeatingTaskHandle frame_task_;
  bool sending_;
  SinkWantsObserver* sink_wants_observer_ RTC_GUARDED_BY(&lock_);

  Mutex lock_;
  std::unique_ptr<FrameGeneratorInterface> frame_generator_;

  int source_fps_ RTC_GUARDED_BY(&lock_);
  int target_capture_fps_ RTC_GUARDED_BY(&lock_);
  VideoRotation fake_rotation_ = kVideoRotation_0;
  std::optional<ColorSpace> fake_color_space_ RTC_GUARDED_BY(&lock_);

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_FRAME_GENERATOR_CAPTURER_H_
