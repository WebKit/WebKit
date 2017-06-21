/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_H_
#define WEBRTC_AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/test/call_test.h"
#include "webrtc/test/fake_audio_device.h"

namespace webrtc {
namespace test {

class AudioQualityTest : public test::EndToEndTest {
 public:
  AudioQualityTest();

 protected:
  virtual std::string AudioInputFile();
  virtual std::string AudioOutputFile();

  virtual FakeNetworkPipe::Config GetNetworkPipeConfig();

  size_t GetNumVideoStreams() const override;
  size_t GetNumAudioStreams() const override;
  size_t GetNumFlexfecStreams() const override;

  std::unique_ptr<test::FakeAudioDevice::Capturer> CreateCapturer() override;
  std::unique_ptr<test::FakeAudioDevice::Renderer> CreateRenderer() override;

  void OnFakeAudioDevicesCreated(
      test::FakeAudioDevice* send_audio_device,
      test::FakeAudioDevice* recv_audio_device) override;

  test::PacketTransport* CreateSendTransport(Call* sender_call) override;
  test::PacketTransport* CreateReceiveTransport() override;

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override;

  void PerformTest() override;
  void OnTestFinished() override;

 private:
  test::FakeAudioDevice* send_audio_device_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_H_
