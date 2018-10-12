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

#include <memory>
#include <string>

#include "api/video/video_frame.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/task_queue.h"
#include "test/frame_generator.h"
#include "test/test_video_capturer.h"

namespace webrtc {

namespace test {

class FrameGenerator;

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

  // |type| has the default value OutputType::I420. |num_squares| has the
  // default value 10.
  static FrameGeneratorCapturer* Create(
      int width,
      int height,
      absl::optional<FrameGenerator::OutputType> type,
      absl::optional<int> num_squares,
      int target_fps,
      Clock* clock);

  static FrameGeneratorCapturer* CreateFromYuvFile(const std::string& file_name,
                                                   size_t width,
                                                   size_t height,
                                                   int target_fps,
                                                   Clock* clock);

  static FrameGeneratorCapturer* CreateSlideGenerator(int width,
                                                      int height,
                                                      int frame_repeat_count,
                                                      int target_fps,
                                                      Clock* clock);
  virtual ~FrameGeneratorCapturer();

  void Start() override;
  void Stop() override;
  void ChangeResolution(size_t width, size_t height);
  void ChangeFramerate(int target_framerate);

  void SetSinkWantsObserver(SinkWantsObserver* observer);

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

  void ForceFrame();
  void SetFakeRotation(VideoRotation rotation);

  int64_t first_frame_capture_time() const { return first_frame_capture_time_; }

  FrameGeneratorCapturer(Clock* clock,
                         std::unique_ptr<FrameGenerator> frame_generator,
                         int target_fps);
  bool Init();

 private:
  class InsertFrameTask;

  void InsertFrame();
  static bool Run(void* obj);
  int GetCurrentConfiguredFramerate();

  Clock* const clock_;
  bool sending_;
  rtc::VideoSinkInterface<VideoFrame>* sink_ RTC_GUARDED_BY(&lock_);
  SinkWantsObserver* sink_wants_observer_ RTC_GUARDED_BY(&lock_);

  rtc::CriticalSection lock_;
  std::unique_ptr<FrameGenerator> frame_generator_;

  int source_fps_ RTC_GUARDED_BY(&lock_);
  int target_capture_fps_ RTC_GUARDED_BY(&lock_);
  absl::optional<int> wanted_fps_ RTC_GUARDED_BY(&lock_);
  VideoRotation fake_rotation_ = kVideoRotation_0;

  int64_t first_frame_capture_time_;
  // Must be the last field, so it will be deconstructed first as tasks
  // in the TaskQueue access other fields of the instance of this class.
  rtc::TaskQueue task_queue_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_FRAME_GENERATOR_CAPTURER_H_
