/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/simulated_time.h"

#include <algorithm>

#include "rtc_base/format_macros.h"

namespace webrtc {
namespace test {
namespace {
struct RawFeedbackReportPacket {
  static constexpr int MAX_FEEDBACKS = 10;
  struct Feedback {
    int16_t seq_offset;
    int32_t recv_offset_ms;
  };
  uint8_t count;
  int64_t first_seq_num;
  int64_t first_recv_time_ms;
  Feedback feedbacks[MAX_FEEDBACKS - 1];
};
}  // namespace

PacketStream::PacketStream(PacketStreamConfig config) : config_(config) {}

std::vector<int64_t> PacketStream::PullPackets(Timestamp at_time) {
  if (next_frame_time_.IsInfinite())
    next_frame_time_ = at_time;

  TimeDelta frame_interval = TimeDelta::seconds(1) / config_.frame_rate;
  int64_t frame_allowance = (frame_interval * target_rate_).bytes();

  if (next_frame_is_keyframe_) {
    frame_allowance *= config_.keyframe_multiplier;
    next_frame_is_keyframe_ = false;
  }

  std::vector<int64_t> packets;
  while (at_time >= next_frame_time_) {
    next_frame_time_ += frame_interval;

    int64_t frame_size = budget_ + frame_allowance;
    frame_size = std::max(frame_size, config_.min_frame_size.bytes());
    budget_ += frame_allowance - frame_size;

    int64_t packet_budget = frame_size;
    int64_t max_packet_size = config_.max_packet_size.bytes();
    while (packet_budget > max_packet_size) {
      packets.push_back(max_packet_size);
      packet_budget -= max_packet_size;
    }
    packets.push_back(packet_budget);
  }
  for (int64_t& packet : packets)
    packet += config_.packet_overhead.bytes();
  return packets;
}

void PacketStream::OnTargetRateUpdate(DataRate target_rate) {
  target_rate_ = std::min(target_rate, config_.max_data_rate);
}

SimpleFeedbackReportPacket FeedbackFromBuffer(
    rtc::CopyOnWriteBuffer raw_buffer) {
  RTC_CHECK_LE(sizeof(RawFeedbackReportPacket), raw_buffer.size());
  const RawFeedbackReportPacket& raw_packet =
      *reinterpret_cast<const RawFeedbackReportPacket*>(raw_buffer.cdata());
  RTC_CHECK_GE(raw_packet.count, 1);
  SimpleFeedbackReportPacket packet;
  packet.receive_times.emplace_back(SimpleFeedbackReportPacket::ReceiveInfo{
      raw_packet.first_seq_num, Timestamp::ms(raw_packet.first_recv_time_ms)});
  for (int i = 1; i < raw_packet.count; ++i)
    packet.receive_times.emplace_back(SimpleFeedbackReportPacket::ReceiveInfo{
        raw_packet.first_seq_num + raw_packet.feedbacks[i - 1].seq_offset,
        Timestamp::ms(raw_packet.first_recv_time_ms +
                      raw_packet.feedbacks[i - 1].recv_offset_ms)});
  return packet;
}

rtc::CopyOnWriteBuffer FeedbackToBuffer(
    const SimpleFeedbackReportPacket packet) {
  RTC_CHECK_LE(packet.receive_times.size(),
               RawFeedbackReportPacket::MAX_FEEDBACKS);
  RawFeedbackReportPacket report;
  report.count = packet.receive_times.size();
  RTC_CHECK(!packet.receive_times.empty());
  report.first_seq_num = packet.receive_times.front().sequence_number;
  report.first_recv_time_ms = packet.receive_times.front().receive_time.ms();

  for (int i = 1; i < report.count; ++i) {
    report.feedbacks[i - 1].seq_offset = static_cast<int16_t>(
        packet.receive_times[i].sequence_number - report.first_seq_num);
    report.feedbacks[i - 1].recv_offset_ms = static_cast<int32_t>(
        packet.receive_times[i].receive_time.ms() - report.first_recv_time_ms);
  }
  return rtc::CopyOnWriteBuffer(reinterpret_cast<uint8_t*>(&report),
                                sizeof(RawFeedbackReportPacket));
}

SimulatedSender::SimulatedSender(NetworkNode* send_node,
                                 uint64_t send_receiver_id)
    : send_node_(send_node), send_receiver_id_(send_receiver_id) {}

SimulatedSender::~SimulatedSender() {}

TransportPacketsFeedback SimulatedSender::PullFeedbackReport(
    SimpleFeedbackReportPacket packet,
    Timestamp at_time) {
  TransportPacketsFeedback report;
  report.prior_in_flight = data_in_flight_;
  report.feedback_time = at_time;

  for (auto& receive_info : packet.receive_times) {
    // Look up sender side information for all packets up to and including each
    // packet with feedback in the report.
    for (; next_feedback_seq_num_ <= receive_info.sequence_number;
         ++next_feedback_seq_num_) {
      PacketResult feedback;
      if (next_feedback_seq_num_ == receive_info.sequence_number) {
        feedback.receive_time = receive_info.receive_time;
      } else {
        // If we did not get any feedback for this packet, mark it as lost by
        // setting receive time to infinity. Note that this can also happen due
        // to reordering, we will newer send feedback out of order. In this case
        // the packet was not really lost, but we don't have that information.
        feedback.receive_time = Timestamp::PlusInfinity();
      }

      // Looking up send side information.
      for (auto it = sent_packets_.begin(); it != sent_packets_.end(); ++it) {
        if (it->sequence_number == next_feedback_seq_num_) {
          feedback.sent_packet = *it;
          if (feedback.receive_time.IsFinite())
            sent_packets_.erase(it);
          break;
        }
      }

      data_in_flight_ -= feedback.sent_packet->size;
      report.packet_feedbacks.push_back(feedback);
    }
  }
  report.data_in_flight = data_in_flight_;
  return report;
}

// Applies pacing and congetsion window based on the configuration from the
// congestion controller. This is not a complete implementation of the real
// pacer but useful for unit tests since it isn't limited to real time.
std::vector<SimulatedSender::PacketReadyToSend>
SimulatedSender::PaceAndPullSendPackets(Timestamp at_time) {
  // TODO(srte): Extract the behavior of PacedSender to a threading and time
  // independent component and use that here to allow a truthful simulation.
  if (last_update_.IsInfinite()) {
    pacing_budget_ = 0;
  } else {
    TimeDelta delta = at_time - last_update_;
    pacing_budget_ += (delta * pacer_config_.data_rate()).bytes();
  }
  std::vector<PacketReadyToSend> to_send;
  while (data_in_flight_ <= max_in_flight_ && pacing_budget_ >= 0 &&
         !packet_queue_.empty()) {
    PendingPacket pending = packet_queue_.front();
    pacing_budget_ -= pending.size;
    packet_queue_.pop_front();
    SentPacket sent;
    sent.sequence_number = next_sequence_number_++;
    sent.size = DataSize::bytes(pending.size);
    data_in_flight_ += sent.size;
    sent.data_in_flight = data_in_flight_;
    sent.pacing_info = PacedPacketInfo();
    sent.send_time = at_time;
    sent_packets_.push_back(sent);
    rtc::CopyOnWriteBuffer packet(
        std::max<size_t>(pending.size, sizeof(sent.sequence_number)));
    memcpy(packet.data(), &sent.sequence_number, sizeof(sent.sequence_number));
    to_send.emplace_back(PacketReadyToSend{sent, packet});
  }
  pacing_budget_ = std::min<int64_t>(pacing_budget_, 0);
  last_update_ = at_time;
  return to_send;
}

void SimulatedSender::Update(NetworkControlUpdate update) {
  if (update.pacer_config)
    pacer_config_ = *update.pacer_config;
  if (update.congestion_window)
    max_in_flight_ = *update.congestion_window;
}

SimulatedFeedback::SimulatedFeedback(SimulatedTimeClientConfig config,
                                     uint64_t return_receiver_id,
                                     NetworkNode* return_node)
    : config_(config),
      return_receiver_id_(return_receiver_id),
      return_node_(return_node) {}

// Polls receiver side for a feedback report and sends it to the stream sender
// via return_node_,
bool SimulatedFeedback::TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                                         uint64_t receiver,
                                         Timestamp at_time) {
  int64_t sequence_number;
  memcpy(&sequence_number, packet.cdata(), sizeof(sequence_number));
  receive_times_.insert({sequence_number, at_time});
  if (last_feedback_time_.IsInfinite())
    last_feedback_time_ = at_time;
  if (at_time >= last_feedback_time_ + config_.feedback.interval) {
    SimpleFeedbackReportPacket report;
    for (; next_feedback_seq_num_ <= sequence_number;
         ++next_feedback_seq_num_) {
      auto it = receive_times_.find(next_feedback_seq_num_);
      if (it != receive_times_.end()) {
        report.receive_times.emplace_back(
            SimpleFeedbackReportPacket::ReceiveInfo{next_feedback_seq_num_,
                                                    it->second});
        receive_times_.erase(it);
      }
      if (receive_times_.size() >= RawFeedbackReportPacket::MAX_FEEDBACKS) {
        return_node_->TryDeliverPacket(FeedbackToBuffer(report),
                                       return_receiver_id_, at_time);
        report = SimpleFeedbackReportPacket();
      }
    }
    if (!report.receive_times.empty())
      return_node_->TryDeliverPacket(FeedbackToBuffer(report),
                                     return_receiver_id_, at_time);
    last_feedback_time_ = at_time;
  }
  return true;
}

SimulatedTimeClient::SimulatedTimeClient(
    std::string log_filename,
    SimulatedTimeClientConfig config,
    std::vector<PacketStreamConfig> stream_configs,
    std::vector<NetworkNode*> send_link,
    std::vector<NetworkNode*> return_link,
    uint64_t send_receiver_id,
    uint64_t return_receiver_id,
    Timestamp at_time)
    : network_controller_factory_(log_filename, config.transport),
      send_link_(send_link),
      return_link_(return_link),
      sender_(send_link.front(), send_receiver_id),
      feedback_(config, return_receiver_id, return_link.front()) {
  NetworkControllerConfig initial_config;
  initial_config.constraints.at_time = at_time;
  initial_config.constraints.starting_rate = config.transport.rates.start_rate;
  initial_config.constraints.min_data_rate = config.transport.rates.min_rate;
  initial_config.constraints.max_data_rate = config.transport.rates.max_rate;
  congestion_controller_ = network_controller_factory_.Create(initial_config);
  for (auto& stream_config : stream_configs)
    packet_streams_.emplace_back(new PacketStream(stream_config));
  NetworkNode::Route(send_receiver_id, send_link, &feedback_);
  NetworkNode::Route(return_receiver_id, return_link, this);

  CongestionProcess(at_time);
  network_controller_factory_.LogCongestionControllerStats(at_time);
  if (!log_filename.empty()) {
    std::string packet_log_name = log_filename + ".packets.txt";
    packet_log_ = fopen(packet_log_name.c_str(), "w");
    fprintf(packet_log_,
            "transport_seq packet_size send_time recv_time feed_time\n");
  }
}

// Pulls feedback reports from sender side based on the recieved feedback
// packet. Updates congestion controller with the resulting report.
bool SimulatedTimeClient::TryDeliverPacket(rtc::CopyOnWriteBuffer raw_buffer,
                                           uint64_t receiver,
                                           Timestamp at_time) {
  auto report =
      sender_.PullFeedbackReport(FeedbackFromBuffer(raw_buffer), at_time);
  for (PacketResult& feedback : report.packet_feedbacks) {
    if (packet_log_)
      fprintf(packet_log_, "%" PRId64 " %" PRId64 " %.3lf %.3lf %.3lf\n",
              feedback.sent_packet->sequence_number,
              feedback.sent_packet->size.bytes(),
              feedback.sent_packet->send_time.seconds<double>(),
              feedback.receive_time.seconds<double>(),
              at_time.seconds<double>());
  }
  Update(congestion_controller_->OnTransportPacketsFeedback(report));
  return true;
}
SimulatedTimeClient::~SimulatedTimeClient() {
  if (packet_log_)
    fclose(packet_log_);
}

void SimulatedTimeClient::Update(NetworkControlUpdate update) {
  sender_.Update(update);
  if (update.target_rate) {
    // TODO(srte): Implement more realistic distribution of bandwidths between
    // streams. Either using BitrateAllocationStrategy directly or using
    // BitrateAllocation.
    double ratio_per_stream = 1.0 / packet_streams_.size();
    DataRate rate_per_stream =
        update.target_rate->target_rate * ratio_per_stream;
    target_rate_ = update.target_rate->target_rate;
    for (auto& stream : packet_streams_)
      stream->OnTargetRateUpdate(rate_per_stream);
  }
}

void SimulatedTimeClient::CongestionProcess(Timestamp at_time) {
  ProcessInterval msg;
  msg.at_time = at_time;
  Update(congestion_controller_->OnProcessInterval(msg));
}

void SimulatedTimeClient::PacerProcess(Timestamp at_time) {
  ProcessFrames(at_time);
  for (auto to_send : sender_.PaceAndPullSendPackets(at_time)) {
    sender_.send_node_->TryDeliverPacket(to_send.data,
                                         sender_.send_receiver_id_, at_time);
    Update(congestion_controller_->OnSentPacket(to_send.send_info));
  }
}

void SimulatedTimeClient::ProcessFrames(Timestamp at_time) {
  for (auto& stream : packet_streams_) {
    for (int64_t packet_size : stream->PullPackets(at_time)) {
      sender_.packet_queue_.push_back(
          SimulatedSender::PendingPacket{packet_size});
    }
  }
}

TimeDelta SimulatedTimeClient::GetNetworkControllerProcessInterval() const {
  return network_controller_factory_.GetProcessInterval();
}

double SimulatedTimeClient::target_rate_kbps() const {
  return target_rate_.kbps<double>();
}

}  // namespace test
}  // namespace webrtc
