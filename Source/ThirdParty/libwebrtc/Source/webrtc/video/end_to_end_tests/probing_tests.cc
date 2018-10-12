/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/simulated_network.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"
namespace webrtc {

class ProbingEndToEndTest : public test::CallTest,
                            public testing::WithParamInterface<std::string> {
 public:
  ProbingEndToEndTest() : field_trial_(GetParam()) {}

  virtual ~ProbingEndToEndTest() {
  }

 private:
  test::ScopedFieldTrials field_trial_;
};
INSTANTIATE_TEST_CASE_P(
    FieldTrials,
    ProbingEndToEndTest,
    ::testing::Values("WebRTC-TaskQueueCongestionControl/Enabled/",
                      "WebRTC-TaskQueueCongestionControl/Disabled/"));

class ProbingTest : public test::EndToEndTest {
 public:
  explicit ProbingTest(int start_bitrate_bps)
      : clock_(Clock::GetRealTimeClock()),
        start_bitrate_bps_(start_bitrate_bps),
        state_(0),
        sender_call_(nullptr) {}

  void ModifySenderCallConfig(Call::Config* config) override {
    config->bitrate_config.start_bitrate_bps = start_bitrate_bps_;
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
TEST_P(ProbingEndToEndTest, DISABLED_InitialProbing) {
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
TEST_P(ProbingEndToEndTest, DISABLED_InitialProbing) {
#else
TEST_P(ProbingEndToEndTest, InitialProbing) {
#endif
  class InitialProbingTest : public ProbingTest {
   public:
    explicit InitialProbingTest(bool* success)
        : ProbingTest(300000), success_(success) {
      *success_ = false;
    }

    void PerformTest() override {
      int64_t start_time_ms = clock_->TimeInMilliseconds();
      do {
        if (clock_->TimeInMilliseconds() - start_time_ms > kTimeoutMs)
          break;

        Call::Stats stats = sender_call_->GetStats();
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
  };

  bool success = false;
  const int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    InitialProbingTest test(&success);
    RunBaseTest(&test);
    if (success)
      return;
  }
  EXPECT_TRUE(success) << "Failed to perform mid initial probing ("
                       << kMaxAttempts << " attempts).";
}

// Fails on Linux MSan: bugs.webrtc.org/7428
#if defined(MEMORY_SANITIZER)
TEST_P(ProbingEndToEndTest, DISABLED_TriggerMidCallProbing) {
// Fails on iOS bots: bugs.webrtc.org/7851
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
TEST_P(ProbingEndToEndTest, DISABLED_TriggerMidCallProbing) {
#else
TEST_P(ProbingEndToEndTest, TriggerMidCallProbing) {
#endif

  class TriggerMidCallProbingTest : public ProbingTest {
   public:
    TriggerMidCallProbingTest(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        bool* success)
        : ProbingTest(300000), success_(success), task_queue_(task_queue) {}

    void PerformTest() override {
      *success_ = false;
      int64_t start_time_ms = clock_->TimeInMilliseconds();
      do {
        if (clock_->TimeInMilliseconds() - start_time_ms > kTimeoutMs)
          break;

        Call::Stats stats = sender_call_->GetStats();

        switch (state_) {
          case 0:
            if (stats.send_bandwidth_bps > 5 * 300000) {
              BitrateConstraints bitrate_config;
              bitrate_config.max_bitrate_bps = 100000;
              task_queue_->SendTask([this, &bitrate_config]() {
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
              task_queue_->SendTask([this, &bitrate_config]() {
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
    test::SingleThreadedTaskQueueForTesting* const task_queue_;
  };

  bool success = false;
  const int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    TriggerMidCallProbingTest test(&task_queue_, &success);
    RunBaseTest(&test);
    if (success)
      return;
  }
  EXPECT_TRUE(success) << "Failed to perform mid call probing (" << kMaxAttempts
                       << " attempts).";
}

#if defined(MEMORY_SANITIZER)
TEST_P(ProbingEndToEndTest, DISABLED_ProbeOnVideoEncoderReconfiguration) {
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
TEST_P(ProbingEndToEndTest, DISABLED_ProbeOnVideoEncoderReconfiguration) {
#else
TEST_P(ProbingEndToEndTest, ProbeOnVideoEncoderReconfiguration) {
#endif

  class ReconfigureTest : public ProbingTest {
   public:
    ReconfigureTest(test::SingleThreadedTaskQueueForTesting* task_queue,
                    bool* success)
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

    void OnRtpTransportControllerSendCreated(
        RtpTransportControllerSend* transport_controller) override {
      transport_controller_ = transport_controller;
    }

    test::PacketTransport* CreateSendTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call) override {
      auto network =
          absl::make_unique<SimulatedNetwork>(DefaultNetworkSimulationConfig());
      send_simulated_network_ = network.get();
      return new test::PacketTransport(
          task_queue, sender_call, this, test::PacketTransport::kSender,
          CallTest::payload_type_map_,
          absl::make_unique<FakeNetworkPipe>(Clock::GetRealTimeClock(),
                                             std::move(network)));
    }

    void PerformTest() override {
      *success_ = false;
      int64_t start_time_ms = clock_->TimeInMilliseconds();
      do {
        if (clock_->TimeInMilliseconds() - start_time_ms > kTimeoutMs)
          break;

        Call::Stats stats = sender_call_->GetStats();

        switch (state_) {
          case 0:
            // Wait until initial probing has been completed (6 times start
            // bitrate).
            if (stats.send_bandwidth_bps >= 250000 &&
                stats.send_bandwidth_bps <= 350000) {
              DefaultNetworkSimulationConfig config;
              config.link_capacity_kbps = 200;
              send_simulated_network_->SetConfig(config);

              // In order to speed up the test we can interrupt exponential
              // probing by toggling the network availability. The alternative
              // is to wait for it to time out (1000 ms).
              transport_controller_->OnNetworkAvailability(false);
              transport_controller_->OnNetworkAvailability(true);

              ++state_;
            }
            break;
          case 1:
            if (stats.send_bandwidth_bps <= 210000) {
              DefaultNetworkSimulationConfig config;
              config.link_capacity_kbps = 5000;
              send_simulated_network_->SetConfig(config);

              encoder_config_->max_bitrate_bps = 2000000;
              encoder_config_->simulcast_layers[0].max_bitrate_bps = 1200000;
              task_queue_->SendTask([this]() {
                send_stream_->ReconfigureVideoEncoder(encoder_config_->Copy());
              });

              ++state_;
            }
            break;
          case 2:
            if (stats.send_bandwidth_bps >= 1000000) {
              *success_ = true;
              observation_complete_.Set();
            }
            break;
        }
      } while (!observation_complete_.Wait(20));
    }

   private:
    const int kTimeoutMs = 3000;
    test::SingleThreadedTaskQueueForTesting* const task_queue_;
    bool* const success_;
    SimulatedNetwork* send_simulated_network_;
    VideoSendStream* send_stream_;
    VideoEncoderConfig* encoder_config_;
    RtpTransportControllerSend* transport_controller_;
  };

  bool success = false;
  const int kMaxAttempts = 3;
  for (int i = 0; i < kMaxAttempts; ++i) {
    ReconfigureTest test(&task_queue_, &success);
    RunBaseTest(&test);
    if (success) {
      return;
    }
  }
  EXPECT_TRUE(success) << "Failed to perform mid call probing (" << kMaxAttempts
                       << " attempts).";
}

}  // namespace webrtc
