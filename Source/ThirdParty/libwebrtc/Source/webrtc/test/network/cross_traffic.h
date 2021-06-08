/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_CROSS_TRAFFIC_H_
#define TEST_NETWORK_CROSS_TRAFFIC_H_

#include <algorithm>
#include <map>
#include <memory>

#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/random.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "test/network/traffic_route.h"
#include "test/scenario/column_printer.h"

namespace webrtc {
namespace test {

struct RandomWalkConfig {
  int random_seed = 1;
  DataRate peak_rate = DataRate::KilobitsPerSec(100);
  DataSize min_packet_size = DataSize::Bytes(200);
  TimeDelta min_packet_interval = TimeDelta::Millis(1);
  TimeDelta update_interval = TimeDelta::Millis(200);
  double variance = 0.6;
  double bias = -0.1;
};

class RandomWalkCrossTraffic {
 public:
  RandomWalkCrossTraffic(RandomWalkConfig config, TrafficRoute* traffic_route);
  ~RandomWalkCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  SequenceChecker sequence_checker_;
  const RandomWalkConfig config_;
  TrafficRoute* const traffic_route_ RTC_PT_GUARDED_BY(sequence_checker_);
  webrtc::Random random_ RTC_GUARDED_BY(sequence_checker_);

  Timestamp last_process_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
  Timestamp last_update_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
  Timestamp last_send_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
  double intensity_ RTC_GUARDED_BY(sequence_checker_) = 0;
  DataSize pending_size_ RTC_GUARDED_BY(sequence_checker_) = DataSize::Zero();
};

struct PulsedPeaksConfig {
  DataRate peak_rate = DataRate::KilobitsPerSec(100);
  DataSize min_packet_size = DataSize::Bytes(200);
  TimeDelta min_packet_interval = TimeDelta::Millis(1);
  TimeDelta send_duration = TimeDelta::Millis(100);
  TimeDelta hold_duration = TimeDelta::Millis(2000);
};

class PulsedPeaksCrossTraffic {
 public:
  PulsedPeaksCrossTraffic(PulsedPeaksConfig config,
                          TrafficRoute* traffic_route);
  ~PulsedPeaksCrossTraffic();

  void Process(Timestamp at_time);
  DataRate TrafficRate() const;
  ColumnPrinter StatsPrinter();

 private:
  SequenceChecker sequence_checker_;
  const PulsedPeaksConfig config_;
  TrafficRoute* const traffic_route_ RTC_PT_GUARDED_BY(sequence_checker_);

  Timestamp last_update_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
  Timestamp last_send_time_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
  bool sending_ RTC_GUARDED_BY(sequence_checker_) = false;
};

class TcpMessageRouteImpl final : public TcpMessageRoute {
 public:
  TcpMessageRouteImpl(Clock* clock,
                      TaskQueueBase* task_queue,
                      EmulatedRoute* send_route,
                      EmulatedRoute* ret_route);

  // Sends a TCP message of the given |size| over the route, |on_received| is
  // called when the message has been delivered. Note that the connection
  // parameters are reset iff there's no currently pending message on the route.
  void SendMessage(size_t size, std::function<void()> on_received) override;

 private:
  // Represents a message sent over the route. When all fragments has been
  // delivered, the message is considered delivered and the handler is
  // triggered. This only happen once.
  struct Message {
    std::function<void()> handler;
    std::set<int> pending_fragment_ids;
  };
  // Represents a piece of a message that fit into a TCP packet.
  struct MessageFragment {
    int fragment_id;
    size_t size;
  };
  // Represents a packet sent on the wire.
  struct TcpPacket {
    int sequence_number;
    Timestamp send_time = Timestamp::MinusInfinity();
    MessageFragment fragment;
  };

  void OnRequest(TcpPacket packet_info);
  void OnResponse(TcpPacket packet_info, Timestamp at_time);
  void HandleLoss(Timestamp at_time);
  void SendPackets(Timestamp at_time);
  void HandlePacketTimeout(int seq_num, Timestamp at_time);

  Clock* const clock_;
  TaskQueueBase* const task_queue_;
  FakePacketRoute<TcpPacket> request_route_;
  FakePacketRoute<TcpPacket> response_route_;

  std::deque<MessageFragment> pending_;
  std::map<int, TcpPacket> in_flight_;
  std::list<Message> messages_;

  double cwnd_;
  double ssthresh_;

  int last_acked_seq_num_ = 0;
  int next_sequence_number_ = 0;
  int next_fragment_id_ = 0;
  Timestamp last_reduction_time_ = Timestamp::MinusInfinity();
  TimeDelta last_rtt_ = TimeDelta::Zero();
};

struct FakeTcpConfig {
  DataSize packet_size = DataSize::Bytes(1200);
  DataSize send_limit = DataSize::PlusInfinity();
  TimeDelta process_interval = TimeDelta::Millis(200);
  TimeDelta packet_timeout = TimeDelta::Seconds(1);
};

class FakeTcpCrossTraffic
    : public TwoWayFakeTrafficRoute<int, int>::TrafficHandlerInterface {
 public:
  FakeTcpCrossTraffic(Clock* clock,
                      FakeTcpConfig config,
                      EmulatedRoute* send_route,
                      EmulatedRoute* ret_route);
  void Start(TaskQueueBase* task_queue);
  void Stop();
  void Process(Timestamp at_time);
  void OnRequest(int sequence_number, Timestamp at_time) override;
  void OnResponse(int sequence_number, Timestamp at_time) override;

  void HandleLoss(Timestamp at_time);

  void SendPackets(Timestamp at_time);

 private:
  Clock* const clock_;
  const FakeTcpConfig conf_;
  TwoWayFakeTrafficRoute<int, int> route_;

  std::map<int, Timestamp> in_flight_;
  double cwnd_ = 10;
  double ssthresh_ = INFINITY;
  bool ack_received_ = false;
  int last_acked_seq_num_ = 0;
  int next_sequence_number_ = 0;
  Timestamp last_reduction_time_ = Timestamp::MinusInfinity();
  TimeDelta last_rtt_ = TimeDelta::Zero();
  DataSize total_sent_ = DataSize::Zero();
  RepeatingTaskHandle repeating_task_handle_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_CROSS_TRAFFIC_H_
