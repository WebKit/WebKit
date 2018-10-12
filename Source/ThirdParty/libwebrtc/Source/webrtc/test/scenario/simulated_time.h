/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SIMULATED_TIME_H_
#define TEST_SCENARIO_SIMULATED_TIME_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"

namespace webrtc {
namespace test {
class PacketStream {
 public:
  explicit PacketStream(PacketStreamConfig config);

 private:
  std::vector<int64_t> PullPackets(Timestamp at_time);
  void OnTargetRateUpdate(DataRate target_rate);

  friend class SimulatedTimeClient;
  PacketStreamConfig config_;
  bool next_frame_is_keyframe_ = true;
  Timestamp next_frame_time_ = Timestamp::MinusInfinity();
  DataRate target_rate_ = DataRate::Zero();
  int64_t budget_ = 0;
};

class SimulatedFeedback : NetworkReceiverInterface {
 public:
  SimulatedFeedback(SimulatedTimeClientConfig config,
                    uint64_t return_receiver_id,
                    NetworkNode* return_node);
  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver,
                        Timestamp at_time) override;

 private:
  friend class SimulatedTimeClient;
  const SimulatedTimeClientConfig config_;
  const uint64_t return_receiver_id_;
  NetworkNode* return_node_;
  Timestamp last_feedback_time_ = Timestamp::MinusInfinity();
  int32_t next_feedback_seq_num_ = 1;
  std::map<int64_t, Timestamp> receive_times_;
};

struct SimpleFeedbackReportPacket {
  struct ReceiveInfo {
    int64_t sequence_number;
    Timestamp receive_time;
  };
  std::vector<ReceiveInfo> receive_times;
};

SimpleFeedbackReportPacket FeedbackFromBuffer(
    rtc::CopyOnWriteBuffer raw_buffer);
rtc::CopyOnWriteBuffer FeedbackToBuffer(
    const SimpleFeedbackReportPacket packet);

class SimulatedSender {
 public:
  struct PacketReadyToSend {
    SentPacket send_info;
    rtc::CopyOnWriteBuffer data;
  };
  struct PendingPacket {
    int64_t size;
  };

  SimulatedSender(NetworkNode* send_node, uint64_t send_receiver_id);
  SimulatedSender(const SimulatedSender&) = delete;
  ~SimulatedSender();
  TransportPacketsFeedback PullFeedbackReport(SimpleFeedbackReportPacket report,
                                              Timestamp at_time);
  std::vector<PacketReadyToSend> PaceAndPullSendPackets(Timestamp at_time);
  void Update(NetworkControlUpdate update);

 private:
  friend class SimulatedTimeClient;
  NetworkNode* send_node_;
  uint64_t send_receiver_id_;
  PacerConfig pacer_config_;
  DataSize max_in_flight_ = DataSize::Infinity();

  std::deque<PendingPacket> packet_queue_;
  std::vector<SentPacket> sent_packets_;

  Timestamp last_update_ = Timestamp::MinusInfinity();
  int64_t pacing_budget_ = 0;
  int64_t next_sequence_number_ = 1;
  int64_t next_feedback_seq_num_ = 1;
  DataSize data_in_flight_ = DataSize::Zero();
};

// SimulatedTimeClient emulates core parts of the behavior of WebRTC from the
// perspective of congestion controllers. This is intended for use in functional
// unit tests to ensure that congestion controllers behave in a reasonable way.
// It does not, however, completely simulate the actual behavior of WebRTC. For
// a more accurate simulation, use the real time only CallClient.
class SimulatedTimeClient : NetworkReceiverInterface {
 public:
  SimulatedTimeClient(std::string log_filename,
                      SimulatedTimeClientConfig config,
                      std::vector<PacketStreamConfig> stream_configs,
                      std::vector<NetworkNode*> send_link,
                      std::vector<NetworkNode*> return_link,
                      uint64_t send_receiver_id,
                      uint64_t return_receiver_id,
                      Timestamp at_time);
  SimulatedTimeClient(const SimulatedTimeClient&) = delete;
  ~SimulatedTimeClient();
  void Update(NetworkControlUpdate update);
  void CongestionProcess(Timestamp at_time);
  void PacerProcess(Timestamp at_time);
  void ProcessFrames(Timestamp at_time);
  TimeDelta GetNetworkControllerProcessInterval() const;
  double target_rate_kbps() const;

  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver,
                        Timestamp at_time) override;

 private:
  friend class Scenario;
  LoggingNetworkControllerFactory network_controller_factory_;
  std::unique_ptr<NetworkControllerInterface> congestion_controller_;
  std::vector<NetworkNode*> send_link_;
  std::vector<NetworkNode*> return_link_;
  SimulatedSender sender_;
  SimulatedFeedback feedback_;
  DataRate target_rate_ = DataRate::Infinity();
  FILE* packet_log_ = nullptr;

  std::vector<std::unique_ptr<PacketStream>> packet_streams_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SIMULATED_TIME_H_
