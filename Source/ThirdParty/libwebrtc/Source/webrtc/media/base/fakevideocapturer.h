/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_FAKEVIDEOCAPTURER_H_
#define MEDIA_BASE_FAKEVIDEOCAPTURER_H_

#include <string.h>

#include <memory>
#include <vector>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "media/base/fakeframesource.h"
#include "media/base/videocapturer.h"
#include "media/base/videocommon.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/timeutils.h"

namespace cricket {

// Fake video capturer that allows the test to manually pump in frames.
class FakeVideoCapturer : public cricket::VideoCapturer {
 public:
  explicit FakeVideoCapturer(bool is_screencast);
  FakeVideoCapturer();

  ~FakeVideoCapturer() override;

  void ResetSupportedFormats(const std::vector<cricket::VideoFormat>& formats);
  virtual bool CaptureFrame();
  virtual bool CaptureCustomFrame(int width, int height);

  sigslot::signal1<FakeVideoCapturer*> SignalDestroyed;

  cricket::CaptureState Start(const cricket::VideoFormat& format) override;
  void Stop() override;
  bool IsRunning() override;
  bool IsScreencast() const override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;

  void SetRotation(webrtc::VideoRotation rotation);

  webrtc::VideoRotation GetRotation();

 private:
  bool CaptureFrame(const webrtc::VideoFrame& frame);

  bool running_;
  const bool is_screencast_;
  // Duplicates FakeFrameSource::rotation_, but needed to support
  // SetRotation before Start.
  webrtc::VideoRotation rotation_;
  std::unique_ptr<FakeFrameSource> frame_source_;
};

// Inherits from FakeVideoCapturer but adds a TaskQueue so that frames can be
// delivered on a TaskQueue as expected by VideoSinkInterface implementations.
class FakeVideoCapturerWithTaskQueue : public FakeVideoCapturer {
 public:
  explicit FakeVideoCapturerWithTaskQueue(bool is_screencast);
  FakeVideoCapturerWithTaskQueue();

  bool CaptureFrame() override;
  bool CaptureCustomFrame(int width, int height) override;

 protected:
  rtc::test::TaskQueueForTest task_queue_{"FakeVideoCapturerWithTaskQueue"};
};

}  // namespace cricket

#endif  // MEDIA_BASE_FAKEVIDEOCAPTURER_H_
