/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/task_queue/task_queue_base.h"
#include "api/test/simulated_network.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
enum : int {  // The first valid value is 1.
  kTransportSequenceNumberExtensionId = 1,
};
}  // namespace

class ProbingEndToEndTest : public test::CallTest {
 public:
  ProbingEndToEndTest() {
    RegisterRtpExtension(RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                                      kTransportSequenceNumberExtensionId));
  }
};

class ProbingTest : public test::EndToEndTest {
 public:
  explicit ProbingTest(int start_bitrate_bps)
      : clock_(Clock::GetRealTimeClock()),
        start_bitrate_bps_(start_bitrate_bps),
        state_(0),
        sender_call_(nullptr) {}

  void ModifySenderBitrateConfig(BitrateConstraints* bitrate_config) override {
    bitrate_config->start_bitrate_bps = start_bitrate_bps_;
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    sender_call_ = sender_call;
  }

 protected:
  Clock* const clock_;
  const int start_bitrate_bps_;
  int state_;
  Call* sender_call_;
};

// Flaky under MemorySanitizer: bugs.webrtc.org/7419
// Flaky on iOS bots: bugs.webrtc.org/7851
#if defined(MEMORY_SANITIZER)
TEST_F(ProbingEndToEndTest, DISABLED_InitialProbing) {
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
TEST_F(ProbingEndToEndTest, DISABLED_InitialProbing) {
#else
TEST_F(ProbingEndToEndTest, InitialProbing) {
#endif

  class InitialProbingTest : public ProbingTest {
   public:
    explicit InitialProbingTest(bool* success, TaskQueueBase* task_queue)
        : ProbingTest(300000), success_(success), task_queue_(task_queue) {
      *success_ = false;
    }

    void PerformTest() override {
      int64_t start_time_ms = clock_->TimeInMilliseconds();
      do {
        if (clock_->TimeInMilliseconds() - start_time_ms > kTimeoutMs)
          break;

        Call::Stats stats;
        SendTask(RTC_FROM_HERE, task_queue_,
                 [this, &stats]() { stats = sender_call_->GetStats(); });
        // Initial probing is done with a x3 and x6 multiplier of the start
        // bitrate, so a x4 multiplier is a high enough threshold.
        if (stats.send_bandwidth_bps > 4 * 300000) {
          *success_ = true;
          break;
        }
      } while (!observation_complete_.Wait(20));
    }

   private:
    const int kTimeoutMs = 1000;
    bool* const success_;
    TaskQueueBase* const task_queue_;
  };

  bool success = false;
  const int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    InitialProbingTest test(&success, task_queue());
    RunBaseTest(&test);
    if (success)
      return;
  }
  EXPECT_TRUE(success) << "Failed to perform mid initial probing ("
                       << kMaxAttempts << " attempts).";
}

// Fails on Linux MSan: bugs.webrtc.org/7428
#if defined(MEMORY_SANITIZER)
TEST_F(ProbingEndToEndTest, DISABLED_TriggerMidCallProbing) {
// Fails on iOS bots: bugs.webrtc.org/7851
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
TEST_F(ProbingEndToEndTest, DISABLED_TriggerMidCallProbing) {
#else
TEST_F(ProbingEndToEndTest, TriggerMidCallProbing) {
#endif

  class TriggerMidCallProbingTest : public ProbingTest {
   public:
    TriggerMidCallProbingTest(TaskQueueBase* task_queue, bool* success)
        : ProbingTest(300000), success_(success), task_queue_(task_queue) {}

    void PerformTest() override {
      *success_ = false;
      int64_t start_time_ms = clock_->TimeInMilliseconds();
      do {
        if (clock_->TimeInMilliseconds() - start_time_ms > kTimeoutMs)
          break;

        Call::Stats stats;
        SendTask(RTC_FROM_HERE, task_queue_,
                 [this, &stats]() { stats = sender_call_->GetStats(); });

        switch (state_) {
          case 0:
            if (stats.send_bandwidth_bps > 5 * 300000) {
              BitrateConstraints bitrate_config;
              bitrate_config.max_bitrate_bps = 100000;
              SendTask(RTC_FROM_HERE, task_queue_, [this, &bitrate_config]() {
                sender_call_->GetTransportControllerSend()
                    ->SetSdpBitrateParameters(bitrate_config);
              });
              ++state_;
            }
            break;
          case 1:
            if (stats.send_bandwidth_bps < 110000) {
              BitrateConstraints bitrate_config;
              bitrate_config.max_bitrate_bps = 2500000;
              SendTask(RTC_FROM_HERE, task_queue_, [this, &bitrate_config]() {
                sender_call_->GetTransportControllerSend()
                    ->SetSdpBitrateParameters(bitrate_config);
              });
              ++state_;
            }
            break;
          case 2:
            // During high cpu load the pacer will not be able to pace packets
            // at the correct speed, but if we go from 110 to 1250 kbps
            // in 5 seconds then it is due to probing.
            if (stats.send_bandwidth_bps > 1250000) {
              *success_ = true;
              observation_complete_.Set();
            }
            break;
        }
      } while (!observation_complete_.Wait(20));
    }

   private:
    const int kTimeoutMs = 5000;
    bool* const success_;
    TaskQueueBase* const task_queue_;
  };

  bool success = false;
  const int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    TriggerMidCallProbingTest test(task_queue(), &success);
    RunBaseTest(&test);
    if (success)
      return;
  }
  EXPECT_TRUE(success) << "Failed to perform mid call probing (" << kMaxAttempts
                       << " attempts).";
}

#if defined(MEMORY_SANITIZER)
TEST_F(ProbingEndToEndTest, DISABLED_ProbeOnVideoEncoderReconfiguration) {
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
TEST_F(ProbingEndToEndTest, DISABLED_ProbeOnVideoEncoderReconfiguration) {
#else
TEST_F(ProbingEndToEndTest, ProbeOnVideoEncoderReconfiguration) {
#endif

  class ReconfigureTest : public ProbingTest {
   public:
    ReconfigureTest(TaskQueueBase* task_queue, bool* success)
        : ProbingTest(50000), task_queue_(task_queue), success_(success) {}

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config_ = encoder_config;
    }

    void OnVideoStreamsCreated(
        VideoSendStream* send_stream,
        const std::vector<VideoReceiveStream*>& receive_streams) override {
      send_stream_ = send_stream;
    }

    std::unique_ptr<test::PacketTransport> CreateSendTransport(
        TaskQueueBase* task_queue,
        Call* sender_call) override {
      auto network =
          std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig());
      send_simulated_network_ = network.get();
      return std::make_unique<test::PacketTransport>(
          task_queue, sender_call, this, test::PacketTransport::kSender,
          CallTest::payload_type_map_,
          std::make_unique<FakeNetworkPipe>(Clock::GetRealTimeClock(),
                                            std::move(network)));
    }

    void PerformTest() override {
      *success_ = false;
      int64_t start_time_ms = clock_->TimeInMilliseconds();
      int64_t max_allocation_change_time_ms = -1;
      do {
        if (clock_->TimeInMilliseconds() - start_time_ms > kTimeoutMs)
          break;

        Call::Stats stats;
        SendTask(RTC_FROM_HERE, task_queue_,
                 [this, &stats]() { stats = sender_call_->GetStats(); });

        switch (state_) {
          case 0:
            // Wait until initial probing has been completed (6 times start
            // bitrate).
            if (stats.send_bandwidth_bps >= 250000 &&
                stats.send_bandwidth_bps <= 350000) {
              BuiltInNetworkBehaviorConfig config;
              config.link_capacity_kbps = 200;
              send_simulated_network_->SetConfig(config);

              // In order to speed up the test we can interrupt exponential
              // probing by toggling the network availability. The alternative
              // is to wait for it to time out (1000 ms).
              sender_call_->GetTransportControllerSend()->OnNetworkAvailability(
                  false);
              sender_call_->GetTransportControllerSend()->OnNetworkAvailability(
                  true);

              ++state_;
            }
            break;
          case 1:
            if (stats.send_bandwidth_bps <= 200000) {
              // Initial probing finished. Increase link capacity and wait
              // until BWE ramped up enough to be in ALR. This takes a few
              // seconds.
              BuiltInNetworkBehaviorConfig config;
              config.link_capacity_kbps = 5000;
              send_simulated_network_->SetConfig(config);
              ++state_;
            }
            break;
          case 2:
            if (stats.send_bandwidth_bps > 240000) {
              // BWE ramped up enough to be in ALR. Setting higher max_bitrate
              // should trigger an allocation probe and fast ramp-up.
              encoder_config_->max_bitrate_bps = 2000000;
              encoder_config_->simulcast_layers[0].max_bitrate_bps = 1200000;
              SendTask(RTC_FROM_HERE, task_queue_, [this]() {
                send_stream_->ReconfigureVideoEncoder(encoder_config_->Copy());
              });
              max_allocation_change_time_ms = clock_->TimeInMilliseconds();
              ++state_;
            }
            break;
          case 3:
            if (stats.send_bandwidth_bps >= 1000000) {
              EXPECT_LT(
                  clock_->TimeInMilliseconds() - max_allocation_change_time_ms,
                  kRampUpMaxDurationMs);
              *success_ = true;
              observation_complete_.Set();
            }
            break;
        }
      } while (!observation_complete_.Wait(20));
    }

   private:
    const int kTimeoutMs = 10000;
    const int kRampUpMaxDurationMs = 500;

    TaskQueueBase* const task_queue_;
    bool* const success_;
    SimulatedNetwork* send_simulated_network_;
    VideoSendStream* send_stream_;
    VideoEncoderConfig* encoder_config_;
  };

  bool success = false;
  const int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    ReconfigureTest test(task_queue(), &success);
    RunBaseTest(&test);
    if (success) {
      return;
    }
  }
  EXPECT_TRUE(success) << "Failed to perform mid call probing (" << kMaxAttempts
                       << " attempts).";
}

}  // namespace webrtc
