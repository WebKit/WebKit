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

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(size_t width,
                                                       size_t height,
                                                       int target_fps,
                                                       Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock, FrameGenerator::CreateChromaGenerator(width, height), target_fps);
  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateFromYuvFile(
    const std::string& file_name,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock, FrameGenerator::CreateFromYuvFile(
                 std::vector<std::string>(1, file_name), width, height, 1),
      target_fps);
  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer::FrameGeneratorCapturer(Clock* clock,
                                               FrameGenerator* frame_generator,
                                               int target_fps)
    : clock_(clock),
      sending_(false),
      sink_(nullptr),
      sink_wants_observer_(nullptr),
      tick_(EventTimerWrapper::Create()),
      thread_(FrameGeneratorCapturer::Run, this, "FrameGeneratorCapturer"),
      frame_generator_(frame_generator),
      target_fps_(target_fps),
      first_frame_capture_time_(-1) {
  RTC_DCHECK(frame_generator);
  RTC_DCHECK_GT(target_fps, 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();

  thread_.Stop();
}

void FrameGeneratorCapturer::SetFakeRotation(VideoRotation rotation) {
  rtc::CritScope cs(&lock_);
  fake_rotation_ = rotation;
}

bool FrameGeneratorCapturer::Init() {
  // This check is added because frame_generator_ might be file based and should
  // not crash because a file moved.
  if (frame_generator_.get() == NULL)
    return false;

  if (!tick_->StartTimer(true, 1000 / target_fps_))
    return false;
  thread_.Start();
  thread_.SetPriority(rtc::kHighPriority);
  return true;
}

bool FrameGeneratorCapturer::Run(void* obj) {
  static_cast<FrameGeneratorCapturer*>(obj)->InsertFrame();
  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  {
    rtc::CritScope cs(&lock_);
    if (sending_) {
      VideoFrame* frame = frame_generator_->NextFrame();
      frame->set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
      frame->set_rotation(fake_rotation_);
      if (first_frame_capture_time_ == -1) {
        first_frame_capture_time_ = frame->ntp_time_ms();
      }
      if (sink_)
        sink_->OnFrame(*frame);
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
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
}

void FrameGeneratorCapturer::RemoveSink(
    rtc::VideoSinkInterface<VideoFrame>* sink) {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(sink_ == sink);
  sink_ = nullptr;
}

void FrameGeneratorCapturer::ForceFrame() {
  tick_->Set();
}
}  // test
}  // webrtc
