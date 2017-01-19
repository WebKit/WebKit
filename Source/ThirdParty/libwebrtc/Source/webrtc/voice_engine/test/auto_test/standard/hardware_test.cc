/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

class HardwareTest : public AfterStreamingFixture {
};

#if !defined(WEBRTC_IOS) && !defined(WEBRTC_ANDROID)
TEST_F(HardwareTest, AbleToQueryForDevices) {
  int num_recording_devices = 0;
  int num_playout_devices = 0;
  EXPECT_EQ(0, voe_hardware_->GetNumOfRecordingDevices(num_recording_devices));
  EXPECT_EQ(0, voe_hardware_->GetNumOfPlayoutDevices(num_playout_devices));

  ASSERT_GT(num_recording_devices, 0) <<
      "There seem to be no recording devices on your system, "
      "and this test really doesn't make sense then.";
  ASSERT_GT(num_playout_devices, 0) <<
      "There seem to be no playout devices on your system, "
      "and this test really doesn't make sense then.";

  // Recording devices are handled a bit differently on Windows - we can
  // just tell it to set the 'default' communication device there.
#ifdef _WIN32
  // Should also work while already recording.
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(
      webrtc::AudioDeviceModule::kDefaultCommunicationDevice));
  // Should also work while already playing.
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(
      webrtc::AudioDeviceModule::kDefaultCommunicationDevice));
#else
  // For other platforms, just use the first device encountered.
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(0));
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(0));
#endif

  // It's hard to know what names this will return (it's system-dependent),
  // so just check that it's possible to do it.
  char device_name[128] = {0};
  char guid_name[128] = {0};
  EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(
      0, device_name, guid_name));
  EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(
      0, device_name, guid_name));
}
#endif
