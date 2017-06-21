/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>
#include <memory>
#include <sstream>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/gtest.h"

using webrtc::SleepMs;
using webrtc::VideoCaptureCapability;
using webrtc::VideoCaptureFactory;
using webrtc::VideoCaptureModule;


#define WAIT_(ex, timeout, res) \
  do { \
    res = (ex); \
    int64_t start = rtc::TimeMillis(); \
    while (!res && rtc::TimeMillis() < start + timeout) { \
      SleepMs(5); \
      res = (ex); \
    } \
  } while (0)

#define EXPECT_TRUE_WAIT(ex, timeout) \
  do { \
    bool res; \
    WAIT_(ex, timeout, res); \
    if (!res) EXPECT_TRUE(ex); \
  } while (0)


static const int kTimeOut = 5000;
static const int kTestHeight = 288;
static const int kTestWidth = 352;
static const int kTestFramerate = 30;

class TestVideoCaptureCallback
    : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  TestVideoCaptureCallback() :
        last_render_time_ms_(0),
        incoming_frames_(0),
        timing_warnings_(0),
        rotate_frame_(webrtc::kVideoRotation_0) {}

  ~TestVideoCaptureCallback() {
    if (timing_warnings_ > 0)
      printf("No of timing warnings %d\n", timing_warnings_);
  }

  void OnFrame(const webrtc::VideoFrame& videoFrame) override {
    rtc::CritScope cs(&capture_cs_);
    int height = videoFrame.height();
    int width = videoFrame.width();
#if defined(ANDROID) && ANDROID
    // Android camera frames may be rotated depending on test device
    // orientation.
    EXPECT_TRUE(height == capability_.height || height == capability_.width);
    EXPECT_TRUE(width == capability_.width || width == capability_.height);
#else
    EXPECT_EQ(height, capability_.height);
    EXPECT_EQ(width, capability_.width);
    EXPECT_EQ(rotate_frame_, videoFrame.rotation());
#endif
    // RenderTimstamp should be the time now.
    EXPECT_TRUE(
        videoFrame.render_time_ms() >= rtc::TimeMillis()-30 &&
        videoFrame.render_time_ms() <= rtc::TimeMillis());

    if ((videoFrame.render_time_ms() >
            last_render_time_ms_ + (1000 * 1.1) / capability_.maxFPS &&
            last_render_time_ms_ > 0) ||
        (videoFrame.render_time_ms() <
            last_render_time_ms_ + (1000 * 0.9) / capability_.maxFPS &&
            last_render_time_ms_ > 0)) {
      timing_warnings_++;
    }

    incoming_frames_++;
    last_render_time_ms_ = videoFrame.render_time_ms();
    last_frame_ = videoFrame.video_frame_buffer();
  }

  void SetExpectedCapability(VideoCaptureCapability capability) {
    rtc::CritScope cs(&capture_cs_);
    capability_= capability;
    incoming_frames_ = 0;
    last_render_time_ms_ = 0;
  }
  int incoming_frames() {
    rtc::CritScope cs(&capture_cs_);
    return incoming_frames_;
  }

  int timing_warnings() {
    rtc::CritScope cs(&capture_cs_);
    return timing_warnings_;
  }
  VideoCaptureCapability capability() {
    rtc::CritScope cs(&capture_cs_);
    return capability_;
  }

  bool CompareLastFrame(const webrtc::VideoFrame& frame) {
    rtc::CritScope cs(&capture_cs_);
    return webrtc::test::FrameBufsEqual(last_frame_,
                                        frame.video_frame_buffer());
  }

  void SetExpectedCaptureRotation(webrtc::VideoRotation rotation) {
    rtc::CritScope cs(&capture_cs_);
    rotate_frame_ = rotation;
  }

 private:
  rtc::CriticalSection capture_cs_;
  VideoCaptureCapability capability_;
  int64_t last_render_time_ms_;
  int incoming_frames_;
  int timing_warnings_;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> last_frame_;
  webrtc::VideoRotation rotate_frame_;
};

class VideoCaptureTest : public testing::Test {
 public:
  VideoCaptureTest() : number_of_devices_(0) {}

  void SetUp() {
    device_info_.reset(VideoCaptureFactory::CreateDeviceInfo());
    assert(device_info_.get());
    number_of_devices_ = device_info_->NumberOfDevices();
    ASSERT_GT(number_of_devices_, 0u);
  }

  rtc::scoped_refptr<VideoCaptureModule> OpenVideoCaptureDevice(
      unsigned int device,
      rtc::VideoSinkInterface<webrtc::VideoFrame>* callback) {
    char device_name[256];
    char unique_name[256];

    EXPECT_EQ(0, device_info_->GetDeviceName(
        device, device_name, 256, unique_name, 256));

    rtc::scoped_refptr<VideoCaptureModule> module(
        VideoCaptureFactory::Create(unique_name));
    if (module.get() == NULL)
      return NULL;

    EXPECT_FALSE(module->CaptureStarted());

    module->RegisterCaptureDataCallback(callback);
    return module;
  }

  void StartCapture(VideoCaptureModule* capture_module,
                    VideoCaptureCapability capability) {
    ASSERT_EQ(0, capture_module->StartCapture(capability));
    EXPECT_TRUE(capture_module->CaptureStarted());

    VideoCaptureCapability resulting_capability;
    EXPECT_EQ(0, capture_module->CaptureSettings(resulting_capability));
    EXPECT_EQ(capability.width, resulting_capability.width);
    EXPECT_EQ(capability.height, resulting_capability.height);
  }

  std::unique_ptr<VideoCaptureModule::DeviceInfo> device_info_;
  unsigned int number_of_devices_;
};

#ifdef WEBRTC_MAC
// Currently fails on Mac 64-bit, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5406
#define MAYBE_CreateDelete DISABLED_CreateDelete
#else
#define MAYBE_CreateDelete CreateDelete
#endif
TEST_F(VideoCaptureTest, MAYBE_CreateDelete) {
  for (int i = 0; i < 5; ++i) {
    int64_t start_time = rtc::TimeMillis();
    TestVideoCaptureCallback capture_observer;
    rtc::scoped_refptr<VideoCaptureModule> module(
        OpenVideoCaptureDevice(0, &capture_observer));
    ASSERT_TRUE(module.get() != NULL);

    VideoCaptureCapability capability;
#ifndef WEBRTC_MAC
    device_info_->GetCapability(module->CurrentDeviceName(), 0, capability);
#else
    capability.width = kTestWidth;
    capability.height = kTestHeight;
    capability.maxFPS = kTestFramerate;
    capability.videoType = webrtc::VideoType::kUnknown;
#endif
    capture_observer.SetExpectedCapability(capability);
    ASSERT_NO_FATAL_FAILURE(StartCapture(module.get(), capability));

    // Less than 4s to start the camera.
    EXPECT_LE(rtc::TimeMillis() - start_time, 4000);

    // Make sure 5 frames are captured.
    EXPECT_TRUE_WAIT(capture_observer.incoming_frames() >= 5, kTimeOut);

    int64_t stop_time = rtc::TimeMillis();
    EXPECT_EQ(0, module->StopCapture());
    EXPECT_FALSE(module->CaptureStarted());

    // Less than 3s to stop the camera.
    EXPECT_LE(rtc::TimeMillis() - stop_time, 3000);
  }
}

#ifdef WEBRTC_MAC
// Currently fails on Mac 64-bit, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5406
#define MAYBE_Capabilities DISABLED_Capabilities
#else
#define MAYBE_Capabilities Capabilities
#endif
TEST_F(VideoCaptureTest, MAYBE_Capabilities) {
#ifdef WEBRTC_MAC
  printf("Video capture capabilities are not supported on Mac.\n");
  return;
#endif

  TestVideoCaptureCallback capture_observer;

  rtc::scoped_refptr<VideoCaptureModule> module(
      OpenVideoCaptureDevice(0, &capture_observer));
  ASSERT_TRUE(module.get() != NULL);

  int number_of_capabilities = device_info_->NumberOfCapabilities(
      module->CurrentDeviceName());
  EXPECT_GT(number_of_capabilities, 0);
  // Key is <width>x<height>, value is vector of maxFPS values at that
  // resolution.
  typedef std::map<std::string, std::vector<int> > FrameRatesByResolution;
  FrameRatesByResolution frame_rates_by_resolution;
  for (int i = 0; i < number_of_capabilities; ++i) {
    VideoCaptureCapability capability;
    EXPECT_EQ(0, device_info_->GetCapability(module->CurrentDeviceName(), i,
                                             capability));
    std::ostringstream resolutionStream;
    resolutionStream << capability.width << "x" << capability.height;
    resolutionStream.flush();
    std::string resolution = resolutionStream.str();
    frame_rates_by_resolution[resolution].push_back(capability.maxFPS);

    // Since Android presents so many resolution/FPS combinations and the test
    // runner imposes a timeout, we only actually start the capture and test
    // that a frame was captured for 2 frame-rates at each resolution.
    if (frame_rates_by_resolution[resolution].size() > 2)
      continue;

    capture_observer.SetExpectedCapability(capability);
    ASSERT_NO_FATAL_FAILURE(StartCapture(module.get(), capability));
    // Make sure at least one frame is captured.
    EXPECT_TRUE_WAIT(capture_observer.incoming_frames() >= 1, kTimeOut);

    EXPECT_EQ(0, module->StopCapture());
  }

#if defined(ANDROID) && ANDROID
  // There's no reason for this to _necessarily_ be true, but in practice all
  // Android devices this test runs on in fact do support multiple capture
  // resolutions and multiple frame-rates per captured resolution, so we assert
  // this fact here as a regression-test against the time that we only noticed a
  // single frame-rate per resolution (bug 2974).  If this test starts being run
  // on devices for which this is untrue (e.g. Nexus4) then the following should
  // probably be wrapped in a base::android::BuildInfo::model()/device() check.
  EXPECT_GT(frame_rates_by_resolution.size(), 1U);
  for (FrameRatesByResolution::const_iterator it =
           frame_rates_by_resolution.begin();
       it != frame_rates_by_resolution.end();
       ++it) {
    EXPECT_GT(it->second.size(), 1U) << it->first;
  }
#endif  // ANDROID
}

// NOTE: flaky, crashes sometimes.
// http://code.google.com/p/webrtc/issues/detail?id=777
TEST_F(VideoCaptureTest, DISABLED_TestTwoCameras) {
  if (number_of_devices_ < 2) {
    printf("There are not two cameras available. Aborting test. \n");
    return;
  }

  TestVideoCaptureCallback capture_observer1;
  rtc::scoped_refptr<VideoCaptureModule> module1(
      OpenVideoCaptureDevice(0, &capture_observer1));
  ASSERT_TRUE(module1.get() != NULL);
  VideoCaptureCapability capability1;
#ifndef WEBRTC_MAC
  device_info_->GetCapability(module1->CurrentDeviceName(), 0, capability1);
#else
  capability1.width = kTestWidth;
  capability1.height = kTestHeight;
  capability1.maxFPS = kTestFramerate;
  capability1.videoType = webrtc::VideoType::kUnknown;
#endif
  capture_observer1.SetExpectedCapability(capability1);

  TestVideoCaptureCallback capture_observer2;
  rtc::scoped_refptr<VideoCaptureModule> module2(
      OpenVideoCaptureDevice(1, &capture_observer2));
  ASSERT_TRUE(module1.get() != NULL);


  VideoCaptureCapability capability2;
#ifndef WEBRTC_MAC
  device_info_->GetCapability(module2->CurrentDeviceName(), 0, capability2);
#else
  capability2.width = kTestWidth;
  capability2.height = kTestHeight;
  capability2.maxFPS = kTestFramerate;
  capability2.videoType = webrtc::VideoType::kUnknown;
#endif
  capture_observer2.SetExpectedCapability(capability2);

  ASSERT_NO_FATAL_FAILURE(StartCapture(module1.get(), capability1));
  ASSERT_NO_FATAL_FAILURE(StartCapture(module2.get(), capability2));
  EXPECT_TRUE_WAIT(capture_observer1.incoming_frames() >= 5, kTimeOut);
  EXPECT_TRUE_WAIT(capture_observer2.incoming_frames() >= 5, kTimeOut);
  EXPECT_EQ(0, module2->StopCapture());
  EXPECT_EQ(0, module1->StopCapture());
}

// Test class for testing external capture and capture feedback information
// such as frame rate and picture alarm.
class VideoCaptureExternalTest : public testing::Test {
 public:
  void SetUp() {
    capture_module_ = VideoCaptureFactory::Create(capture_input_interface_);

    VideoCaptureCapability capability;
    capability.width = kTestWidth;
    capability.height = kTestHeight;
    capability.videoType = webrtc::VideoType::kYV12;
    capability.maxFPS = kTestFramerate;
    capture_callback_.SetExpectedCapability(capability);

    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kTestWidth, kTestHeight);

    memset(buffer->MutableDataY(), 127, buffer->height() * buffer->StrideY());
    memset(buffer->MutableDataU(), 127,
           buffer->ChromaHeight() * buffer->StrideU());
    memset(buffer->MutableDataV(), 127,
           buffer->ChromaHeight() * buffer->StrideV());
    test_frame_.reset(
        new webrtc::VideoFrame(buffer, 0, 0, webrtc::kVideoRotation_0));

    SleepMs(1);  // Wait 1ms so that two tests can't have the same timestamp.

    capture_module_->RegisterCaptureDataCallback(&capture_callback_);
  }

  void TearDown() {
  }

  webrtc::VideoCaptureExternal* capture_input_interface_;
  rtc::scoped_refptr<VideoCaptureModule> capture_module_;
  std::unique_ptr<webrtc::VideoFrame> test_frame_;
  TestVideoCaptureCallback capture_callback_;
};

// Test input of external video frames.
TEST_F(VideoCaptureExternalTest, TestExternalCapture) {
  size_t length = webrtc::CalcBufferSize(
      webrtc::VideoType::kI420, test_frame_->width(), test_frame_->height());
  std::unique_ptr<uint8_t[]> test_buffer(new uint8_t[length]);
  webrtc::ExtractBuffer(*test_frame_, length, test_buffer.get());
  EXPECT_EQ(0, capture_input_interface_->IncomingFrame(test_buffer.get(),
      length, capture_callback_.capability(), 0));
  EXPECT_TRUE(capture_callback_.CompareLastFrame(*test_frame_));
}

TEST_F(VideoCaptureExternalTest, Rotation) {
  EXPECT_EQ(0, capture_module_->SetCaptureRotation(webrtc::kVideoRotation_0));
  size_t length = webrtc::CalcBufferSize(
      webrtc::VideoType::kI420, test_frame_->width(), test_frame_->height());
  std::unique_ptr<uint8_t[]> test_buffer(new uint8_t[length]);
  webrtc::ExtractBuffer(*test_frame_, length, test_buffer.get());
  EXPECT_EQ(0, capture_input_interface_->IncomingFrame(test_buffer.get(),
    length, capture_callback_.capability(), 0));
  EXPECT_EQ(0, capture_module_->SetCaptureRotation(webrtc::kVideoRotation_90));
  capture_callback_.SetExpectedCaptureRotation(webrtc::kVideoRotation_90);
  EXPECT_EQ(0, capture_input_interface_->IncomingFrame(test_buffer.get(),
    length, capture_callback_.capability(), 0));
  EXPECT_EQ(0, capture_module_->SetCaptureRotation(webrtc::kVideoRotation_180));
  capture_callback_.SetExpectedCaptureRotation(webrtc::kVideoRotation_180);
  EXPECT_EQ(0, capture_input_interface_->IncomingFrame(test_buffer.get(),
    length, capture_callback_.capability(), 0));
  EXPECT_EQ(0, capture_module_->SetCaptureRotation(webrtc::kVideoRotation_270));
  capture_callback_.SetExpectedCaptureRotation(webrtc::kVideoRotation_270);
  EXPECT_EQ(0, capture_input_interface_->IncomingFrame(test_buffer.get(),
    length, capture_callback_.capability(), 0));
}
