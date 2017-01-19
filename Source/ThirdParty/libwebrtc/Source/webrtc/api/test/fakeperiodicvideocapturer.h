/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// FakePeriodicVideoCapturer implements a fake cricket::VideoCapturer that
// creates video frames periodically after it has been started.

#ifndef WEBRTC_API_TEST_FAKEPERIODICVIDEOCAPTURER_H_
#define WEBRTC_API_TEST_FAKEPERIODICVIDEOCAPTURER_H_

#include <vector>

#include "webrtc/base/thread.h"
#include "webrtc/media/base/fakevideocapturer.h"

namespace webrtc {

class FakePeriodicVideoCapturer : public cricket::FakeVideoCapturer,
                                  public rtc::MessageHandler {
 public:
  FakePeriodicVideoCapturer() {
    std::vector<cricket::VideoFormat> formats;
    formats.push_back(cricket::VideoFormat(1280, 720,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 360,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(320, 240,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(160, 120,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    ResetSupportedFormats(formats);
  }

  virtual cricket::CaptureState Start(const cricket::VideoFormat& format) {
    cricket::CaptureState state = FakeVideoCapturer::Start(format);
    if (state != cricket::CS_FAILED) {
      rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_CREATEFRAME);
    }
    return state;
  }
  virtual void Stop() {
    rtc::Thread::Current()->Clear(this);
  }
  // Inherited from MesageHandler.
  virtual void OnMessage(rtc::Message* msg) {
    if (msg->message_id == MSG_CREATEFRAME) {
      if (IsRunning()) {
        CaptureFrame();
        rtc::Thread::Current()->PostDelayed(
            RTC_FROM_HERE, static_cast<int>(GetCaptureFormat()->interval /
                                            rtc::kNumNanosecsPerMillisec),
            this, MSG_CREATEFRAME);
        }
    }
  }

 private:
  enum {
    // Offset  0xFF to make sure this don't collide with base class messages.
    MSG_CREATEFRAME = 0xFF
  };
};

}  // namespace webrtc

#endif  //  WEBRTC_API_TEST_FAKEPERIODICVIDEOCAPTURER_H_
