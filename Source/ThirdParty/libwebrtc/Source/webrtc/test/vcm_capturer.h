/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_VCM_CAPTURER_H_
#define WEBRTC_TEST_VCM_CAPTURER_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/test/video_capturer.h"

namespace webrtc {
namespace test {

class VcmCapturer
    : public VideoCapturer,
      public rtc::VideoSinkInterface<VideoFrame> {
 public:
  static VcmCapturer* Create(size_t width, size_t height, size_t target_fps);
  virtual ~VcmCapturer();

  void Start() override;
  void Stop() override;
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

  void OnFrame(const VideoFrame& frame) override;

 private:
  VcmCapturer();
  bool Init(size_t width, size_t height, size_t target_fps);
  void Destroy();

  rtc::CriticalSection crit_;
  bool started_ GUARDED_BY(crit_);
  rtc::VideoSinkInterface<VideoFrame>* sink_ GUARDED_BY(crit_);
  rtc::scoped_refptr<VideoCaptureModule> vcm_;
  VideoCaptureCapability capability_;
};

}  // test
}  // webrtc

#endif  // WEBRTC_TEST_VCM_CAPTURER_H_
