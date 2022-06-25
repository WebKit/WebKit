/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/test/audio_bwe_integration_test.h"

#include <memory>

#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "common_audio/wav_file.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

namespace {
enum : int {  // The first valid value is 1.
  kTransportSequenceNumberExtensionId = 1,
};

// Wait a second between stopping sending and stopping receiving audio.
constexpr int kExtraProcessTimeMs = 1000;
}  // namespace

AudioBweTest::AudioBweTest() : EndToEndTest(CallTest::kDefaultTimeoutMs) {}

size_t AudioBweTest::GetNumVideoStreams() const {
  return 0;
}
size_t AudioBweTest::GetNumAudioStreams() const {
  return 1;
}
size_t AudioBweTest::GetNumFlexfecStreams() const {
  return 0;
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
AudioBweTest::CreateCapturer() {
  return TestAudioDeviceModule::CreateWavFileReader(AudioInputFile());
}

void AudioBweTest::OnFakeAudioDevicesCreated(
    TestAudioDeviceModule* send_audio_device,
    TestAudioDeviceModule* recv_audio_device) {
  send_audio_device_ = send_audio_device;
}

std::unique_ptr<test::PacketTransport> AudioBweTest::CreateSendTransport(
    TaskQueueBase* task_queue,
    Call* sender_call) {
  return std::make_unique<test::PacketTransport>(
      task_queue, sender_call, this, test::PacketTransport::kSender,
      test::CallTest::payload_type_map_,
      std::make_unique<FakeNetworkPipe>(
          Clock::GetRealTimeClock(),
          std::make_unique<SimulatedNetwork>(GetNetworkPipeConfig())));
}

std::unique_ptr<test::PacketTransport> AudioBweTest::CreateReceiveTransport(
    TaskQueueBase* task_queue) {
  return std::make_unique<test::PacketTransport>(
      task_queue, nullptr, this, test::PacketTransport::kReceiver,
      test::CallTest::payload_type_map_,
      std::make_unique<FakeNetworkPipe>(
          Clock::GetRealTimeClock(),
          std::make_unique<SimulatedNetwork>(GetNetworkPipeConfig())));
}

void AudioBweTest::PerformTest() {
  send_audio_device_->WaitForRecordingEnd();
  SleepMs(GetNetworkPipeConfig().queue_delay_ms + kExtraProcessTimeMs);
}

class StatsPollTask : public QueuedTask {
 public:
  explicit StatsPollTask(Call* sender_call) : sender_call_(sender_call) {}

 private:
  bool Run() override {
    RTC_CHECK(sender_call_);
    Call::Stats call_stats = sender_call_->GetStats();
    EXPECT_GT(call_stats.send_bandwidth_bps, 25000);
    TaskQueueBase::Current()->PostDelayedTask(std::unique_ptr<QueuedTask>(this),
                                              100);
    return false;
  }
  Call* sender_call_;
};

class NoBandwidthDropAfterDtx : public AudioBweTest {
 public:
  NoBandwidthDropAfterDtx()
      : sender_call_(nullptr), stats_poller_("stats poller task queue") {}

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    send_config->send_codec_spec = AudioSendStream::Config::SendCodecSpec(
        test::CallTest::kAudioSendPayloadType,
        {"OPUS",
         48000,
         2,
         {{"ptime", "60"}, {"usedtx", "1"}, {"stereo", "1"}}});

    send_config->min_bitrate_bps = 6000;
    send_config->max_bitrate_bps = 100000;
    send_config->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                     kTransportSequenceNumberExtensionId));
    for (AudioReceiveStream::Config& recv_config : *receive_configs) {
      recv_config.rtp.transport_cc = true;
      recv_config.rtp.extensions = send_config->rtp.extensions;
      recv_config.rtp.remote_ssrc = send_config->rtp.ssrc;
    }
  }

  std::string AudioInputFile() override {
    return test::ResourcePath("voice_engine/audio_dtx16", "wav");
  }

  BuiltInNetworkBehaviorConfig GetNetworkPipeConfig() override {
    BuiltInNetworkBehaviorConfig pipe_config;
    pipe_config.link_capacity_kbps = 50;
    pipe_config.queue_length_packets = 1500;
    pipe_config.queue_delay_ms = 300;
    return pipe_config;
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    sender_call_ = sender_call;
  }

  void PerformTest() override {
    stats_poller_.PostDelayedTask(std::make_unique<StatsPollTask>(sender_call_),
                                  100);
    sender_call_->OnAudioTransportOverheadChanged(0);
    AudioBweTest::PerformTest();
  }

 private:
  Call* sender_call_;
  TaskQueueForTest stats_poller_;
};

using AudioBweIntegrationTest = CallTest;

// TODO(tschumim): This test is flaky when run on android and mac. Re-enable the
// test for when the issue is fixed.
TEST_F(AudioBweIntegrationTest, DISABLED_NoBandwidthDropAfterDtx) {
  NoBandwidthDropAfterDtx test;
  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
