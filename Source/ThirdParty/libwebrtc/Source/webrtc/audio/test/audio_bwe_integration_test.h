/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef AUDIO_TEST_AUDIO_BWE_INTEGRATION_TEST_H_
#define AUDIO_TEST_AUDIO_BWE_INTEGRATION_TEST_H_

#include <memory>
#include <string>

#include "api/test/simulated_network.h"
#include "test/call_test.h"
#include "test/single_threaded_task_queue.h"

namespace webrtc {
namespace test {

class AudioBweTest : public test::EndToEndTest {
 public:
  AudioBweTest();

 protected:
  virtual std::string AudioInputFile() = 0;

  virtual DefaultNetworkSimulationConfig GetNetworkPipeConfig() = 0;

  size_t GetNumVideoStreams() const override;
  size_t GetNumAudioStreams() const override;
  size_t GetNumFlexfecStreams() const override;

  std::unique_ptr<TestAudioDeviceModule::Capturer> CreateCapturer() override;

  void OnFakeAudioDevicesCreated(
      TestAudioDeviceModule* send_audio_device,
      TestAudioDeviceModule* recv_audio_device) override;

  test::PacketTransport* CreateSendTransport(
      SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override;
  test::PacketTransport* CreateReceiveTransport(
      SingleThreadedTaskQueueForTesting* task_queue) override;

  void PerformTest() override;

 private:
  TestAudioDeviceModule* send_audio_device_;
};

}  // namespace test
}  // namespace webrtc

#endif  // AUDIO_TEST_AUDIO_BWE_INTEGRATION_TEST_H_
