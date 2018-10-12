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
#include "rtc_base/criticalsection.h"
#include "rtc_base/timeutils.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {

// This tester simulates a series of clock reset events where different offsets
// are added to the receive time. It detects jumps in the resulting reported
// receive times of more than 200 ms.
class ReportedReceiveTimeTester : public test::EndToEndTest {
 public:
  struct TimeJump {
    int64_t at_send_time_ms;
    int64_t add_offset_ms;
    static constexpr int64_t kStop = 0;
  };

  ReportedReceiveTimeTester()
      : EndToEndTest(test::CallTest::kDefaultTimeoutMs) {
    // These should be let trough without correction and filtered if correction
    // is enabled.
    jumps_.push({500, 2000});
    jumps_.push({1000, -400});
    jumps_.push({1500, 2000000});
    jumps_.push({1700, TimeJump::kStop});
  }
  bool JumpInReportedTimes() { return jump_in_reported_times_; }

 protected:
  Action OnReceiveRtcp(const uint8_t* data, size_t length) override {
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(data, length));
    const auto& fb = parser.transport_feedback();
    if (fb->num_packets() > 0) {
      int64_t arrival_time_us = fb->GetBaseTimeUs();
      for (const auto& pkt : fb->GetReceivedPackets()) {
        arrival_time_us += pkt.delta_us();
        if (last_arrival_time_us_ != 0) {
          int64_t delta_us = arrival_time_us - last_arrival_time_us_;
          rtc::CritScope crit(&send_times_crit_);
          if (send_times_us_.size() >= 2) {
            int64_t ground_truth_delta_us =
                send_times_us_[1] - send_times_us_[0];
            send_times_us_.pop_front();
            int64_t delta_diff_ms = (delta_us - ground_truth_delta_us) / 1000;
            if (std::abs(delta_diff_ms) > 200) {
              jump_in_reported_times_ = true;
              observation_complete_.Set();
            }
          }
        }
        last_arrival_time_us_ = arrival_time_us;
      }
    }
    return SEND_PACKET;
  }
  Action OnSendRtp(const uint8_t* data, size_t length) override {
    {
      rtc::CritScope crit(&send_times_crit_);
      send_times_us_.push_back(rtc::TimeMicros());
    }
    int64_t now_ms = rtc::TimeMillis();
    if (!first_send_time_ms_)
      first_send_time_ms_ = now_ms;
    int64_t send_time_ms = now_ms - first_send_time_ms_;
    if (send_time_ms >= jumps_.front().at_send_time_ms) {
      if (jumps_.front().add_offset_ms == TimeJump::kStop) {
        observation_complete_.Set();
        jumps_.pop();
        return SEND_PACKET;
      }
      clock_offset_ms_ += jumps_.front().add_offset_ms;
      send_pipe_->SetClockOffset(clock_offset_ms_);
      jumps_.pop();
    }
    return SEND_PACKET;
  }
  test::PacketTransport* CreateSendTransport(
      test::SingleThreadedTaskQueueForTesting* task_queue,
      Call* sender_call) override {
    auto pipe = absl::make_unique<FakeNetworkPipe>(
        Clock::GetRealTimeClock(),
        absl::make_unique<SimulatedNetwork>(DefaultNetworkSimulationConfig()));
    send_pipe_ = pipe.get();
    return send_transport_ = new test::PacketTransport(
               task_queue, sender_call, this, test::PacketTransport::kSender,
               test::CallTest::payload_type_map_, std::move(pipe));
  }
  void PerformTest() override {
    observation_complete_.Wait(test::CallTest::kDefaultTimeoutMs);
  }
  size_t GetNumVideoStreams() const override { return 1; }
  size_t GetNumAudioStreams() const override { return 0; }

 private:
  int64_t last_arrival_time_us_ = 0;
  int64_t first_send_time_ms_ = 0;
  rtc::CriticalSection send_times_crit_;
  std::deque<int64_t> send_times_us_ RTC_GUARDED_BY(send_times_crit_);
  bool jump_in_reported_times_ = false;
  FakeNetworkPipe* send_pipe_;
  test::PacketTransport* send_transport_;
  int64_t clock_offset_ms_ = 0;
  std::queue<TimeJump> jumps_;
};
}  // namespace

class ReceiveTimeEndToEndTest : public test::CallTest {
 public:
  ReceiveTimeEndToEndTest() {}

  virtual ~ReceiveTimeEndToEndTest() {}
};

TEST_F(ReceiveTimeEndToEndTest, ReceiveTimeJumpsWithoutFieldTrial) {
  // Without the field trial, the jumps in clock offset should be let trough and
  // be detected.
  ReportedReceiveTimeTester test;
  RunBaseTest(&test);
  EXPECT_TRUE(test.JumpInReportedTimes());
}

TEST_F(ReceiveTimeEndToEndTest, ReceiveTimeSteadyWithFieldTrial) {
  // Since all the added jumps by the tester are outside the interval of -100 ms
  // to 1000 ms, they should all be filtered by the field trial below, and no
  // jumps should be detected.
  test::ScopedFieldTrials field_trial(
      "WebRTC-BweReceiveTimeCorrection/Enabled,-100,1000/");
  ReportedReceiveTimeTester test;
  RunBaseTest(&test);
  EXPECT_FALSE(test.JumpInReportedTimes());
}
}  // namespace webrtc
