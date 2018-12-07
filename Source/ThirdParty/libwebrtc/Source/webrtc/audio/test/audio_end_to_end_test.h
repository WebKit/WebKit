/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef AUDIO_TEST_AUDIO_END_TO_END_TEST_H_
#define AUDIO_TEST_AUDIO_END_TO_END_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "api/test/simulated_network.h"
#include "test/call_test.h"

namespace webrtc {
namespace test {

class AudioEndToEndTest : public test::EndToEndTest {
 public:
  AudioEndToEndTest();

 protected:
  TestAudioDeviceModule* send_audio_device() { return send_audio_device_; }
  const AudioSendStream* send_stream() const { return send_stream_; }
  const AudioReceiveStream* receive_stream() const { return receive_stream_; }

  virtual BuiltInNetworkBehaviorConfig GetNetworkPipeConfig() const;

  size_t GetNumVideoStreams() const override;
  size_t GetNumAudioStreams() const override;
  size_t GetNumFlexfecStreams() const override;

  std::unique_ptr<TestAudioDeviceModule::Capturer> CreateCapturer() override;
  std::unique_ptr<TestAudioDeviceModule::Renderer> CreateRenderer() override;

  void OnFakeAudioDevicesCreated(
      TestAudioDeviceModule* send_audio_device,
      TestAudioDeviceModule* recv_audio_device) override;

  test::PacketTransport* CreateSendTransport(
      SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override;
  test::PacketTransport* CreateReceiveTransport(
      SingleThreadedTaskQueueForTesting* task_queue) override;

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override;
  void OnAudioStreamsCreated(
      AudioSendStream* send_stream,
      const std::vector<AudioReceiveStream*>& receive_streams) override;

  void PerformTest() override;

 private:
  TestAudioDeviceModule* send_audio_device_ = nullptr;
  AudioSendStream* send_stream_ = nullptr;
  AudioReceiveStream* receive_stream_ = nullptr;
};

}  // namespace test
}  // namespace webrtc

#endif  // AUDIO_TEST_AUDIO_END_TO_END_TEST_H_
