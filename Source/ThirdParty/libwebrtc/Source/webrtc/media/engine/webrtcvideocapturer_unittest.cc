/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef HAVE_WEBRTC_VIDEO

#include <stdio.h>

#include <memory>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/base/testutils.h"
#include "webrtc/media/base/videocommon.h"
#include "webrtc/media/engine/fakewebrtcvcmfactory.h"
#include "webrtc/media/engine/webrtcvideocapturer.h"

using cricket::VideoFormat;

static const std::string kTestDeviceName = "JuberTech FakeCam Q123";
static const std::string kTestDeviceId = "foo://bar/baz";
const VideoFormat kDefaultVideoFormat =
    VideoFormat(640, 400, VideoFormat::FpsToInterval(30), cricket::FOURCC_ANY);

class WebRtcVideoCapturerTest : public testing::Test {
 public:
  WebRtcVideoCapturerTest()
      : factory_(new FakeWebRtcVcmFactory),
        capturer_(new cricket::WebRtcVideoCapturer(factory_)) {
    factory_->device_info.AddDevice(kTestDeviceName, kTestDeviceId);
    // add a VGA/I420 capability
    webrtc::VideoCaptureCapability vga;
    vga.width = 640;
    vga.height = 480;
    vga.maxFPS = 30;
    vga.videoType = webrtc::VideoType::kI420;
    factory_->device_info.AddCapability(kTestDeviceId, vga);
  }

 protected:
  FakeWebRtcVcmFactory* factory_;  // owned by capturer_
  std::unique_ptr<cricket::WebRtcVideoCapturer> capturer_;
};

TEST_F(WebRtcVideoCapturerTest, TestNotOpened) {
  EXPECT_EQ("", capturer_->GetId());
  EXPECT_TRUE(capturer_->GetSupportedFormats()->empty());
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(WebRtcVideoCapturerTest, TestBadInit) {
  EXPECT_FALSE(capturer_->Init(cricket::Device("bad-name", "bad-id")));
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(WebRtcVideoCapturerTest, TestInit) {
  EXPECT_TRUE(capturer_->Init(cricket::Device(kTestDeviceName, kTestDeviceId)));
  EXPECT_EQ(kTestDeviceId, capturer_->GetId());
  EXPECT_TRUE(NULL != capturer_->GetSupportedFormats());
  ASSERT_EQ(1U, capturer_->GetSupportedFormats()->size());
  EXPECT_EQ(640, (*capturer_->GetSupportedFormats())[0].width);
  EXPECT_EQ(480, (*capturer_->GetSupportedFormats())[0].height);
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);  // not started yet
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(WebRtcVideoCapturerTest, TestInitVcm) {
  EXPECT_TRUE(capturer_->Init(factory_->Create(
      reinterpret_cast<const char*>(kTestDeviceId.c_str()))));
}

TEST_F(WebRtcVideoCapturerTest, TestCapture) {
  EXPECT_TRUE(capturer_->Init(cricket::Device(kTestDeviceName, kTestDeviceId)));
  cricket::VideoCapturerListener listener(capturer_.get());
  cricket::VideoFormat format(
      capturer_->GetSupportedFormats()->at(0));
  EXPECT_EQ(cricket::CS_STARTING, capturer_->Start(format));
  EXPECT_TRUE(capturer_->IsRunning());
  ASSERT_TRUE(capturer_->GetCaptureFormat() != NULL);
  EXPECT_EQ(format, *capturer_->GetCaptureFormat());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, listener.last_capture_state(), 1000);
  factory_->modules[0]->SendFrame(640, 480);
  EXPECT_TRUE_WAIT(listener.frame_count() > 0, 5000);
  EXPECT_EQ(640, listener.frame_width());
  EXPECT_EQ(480, listener.frame_height());
  EXPECT_EQ(cricket::CS_FAILED, capturer_->Start(format));
  capturer_->Stop();
  EXPECT_FALSE(capturer_->IsRunning());
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
  EXPECT_EQ_WAIT(cricket::CS_STOPPED, listener.last_capture_state(), 1000);
}

TEST_F(WebRtcVideoCapturerTest, TestCaptureVcm) {
  EXPECT_TRUE(capturer_->Init(factory_->Create(
      reinterpret_cast<const char*>(kTestDeviceId.c_str()))));
  cricket::VideoCapturerListener listener(capturer_.get());
  EXPECT_TRUE(capturer_->GetSupportedFormats()->empty());
  VideoFormat format;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(kDefaultVideoFormat, &format));
  EXPECT_EQ(kDefaultVideoFormat.width, format.width);
  EXPECT_EQ(kDefaultVideoFormat.height, format.height);
  EXPECT_EQ(kDefaultVideoFormat.interval, format.interval);
  EXPECT_EQ(cricket::FOURCC_I420, format.fourcc);
  EXPECT_EQ(cricket::CS_STARTING, capturer_->Start(format));
  EXPECT_TRUE(capturer_->IsRunning());
  ASSERT_TRUE(capturer_->GetCaptureFormat() != NULL);
  EXPECT_EQ(format, *capturer_->GetCaptureFormat());
  EXPECT_EQ_WAIT(cricket::CS_RUNNING, listener.last_capture_state(), 1000);
  factory_->modules[0]->SendFrame(640, 480);
  EXPECT_TRUE_WAIT(listener.frame_count() > 0, 5000);
  EXPECT_EQ(640, listener.frame_width());
  EXPECT_EQ(480, listener.frame_height());
  EXPECT_EQ(cricket::CS_FAILED, capturer_->Start(format));
  capturer_->Stop();
  EXPECT_FALSE(capturer_->IsRunning());
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
}

TEST_F(WebRtcVideoCapturerTest, TestCaptureWithoutInit) {
  cricket::VideoFormat format;
  EXPECT_EQ(cricket::CS_FAILED, capturer_->Start(format));
  EXPECT_TRUE(capturer_->GetCaptureFormat() == NULL);
  EXPECT_FALSE(capturer_->IsRunning());
}

#endif  // HAVE_WEBRTC_VIDEO
