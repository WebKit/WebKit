/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_FRAME_GENERATOR_CAPTURER_H_
#define WEBRTC_TEST_FRAME_GENERATOR_CAPTURER_H_

#include <memory>
#include <string>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class EventTimerWrapper;

namespace test {

class FrameGenerator;

class FrameGeneratorCapturer : public VideoCapturer {
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

  static FrameGeneratorCapturer* Create(int width,
                                        int height,
                                        int target_fps,
                                        Clock* clock);

  static FrameGeneratorCapturer* CreateFromYuvFile(const std::string& file_name,
                                                   size_t width,
                                                   size_t height,
                                                   int target_fps,
                                                   Clock* clock);
  virtual ~FrameGeneratorCapturer();

  void Start() override;
  void Stop() override;
  void ChangeResolution(size_t width, size_t height);

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
  void InsertFrame();
  static bool Run(void* obj);

  Clock* const clock_;
  bool sending_;
  rtc::VideoSinkInterface<VideoFrame>* sink_ GUARDED_BY(&lock_);
  SinkWantsObserver* sink_wants_observer_ GUARDED_BY(&lock_);

  std::unique_ptr<EventTimerWrapper> tick_;
  rtc::CriticalSection lock_;
  rtc::PlatformThread thread_;
  std::unique_ptr<FrameGenerator> frame_generator_;

  int target_fps_;
  VideoRotation fake_rotation_ = kVideoRotation_0;

  int64_t first_frame_capture_time_;
};
}  // test
}  // webrtc

#endif  // WEBRTC_TEST_FRAME_GENERATOR_CAPTURER_H_
