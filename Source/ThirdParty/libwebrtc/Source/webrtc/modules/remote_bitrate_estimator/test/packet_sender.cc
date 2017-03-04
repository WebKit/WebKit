/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"

#include <algorithm>
#include <list>
#include <sstream>

#include "webrtc/base/checks.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/metric_recorder.h"

namespace webrtc {
namespace testing {
namespace bwe {

void PacketSender::Pause() {
  running_ = false;
  if (metric_recorder_ != nullptr) {
    metric_recorder_->PauseFlow();
  }
}

void PacketSender::Resume(int64_t paused_time_ms) {
  running_ = true;
  if (metric_recorder_ != nullptr) {
    metric_recorder_->ResumeFlow(paused_time_ms);
  }
}

void PacketSender::set_metric_recorder(MetricRecorder* metric_recorder) {
  metric_recorder_ = metric_recorder;
}

void PacketSender::RecordBitrate() {
  if (metric_recorder_ != nullptr) {
    BWE_TEST_LOGGING_CONTEXT("Sender");
    BWE_TEST_LOGGING_CONTEXT(*flow_ids().begin());
    metric_recorder_->UpdateTimeMs(clock_.TimeInMilliseconds());
    metric_recorder_->UpdateSendingEstimateKbps(TargetBitrateKbps());
  }
}

std::list<FeedbackPacket*> GetFeedbackPackets(Packets* in_out,
                                              int64_t end_time_ms,
                                              int flow_id) {
  std::list<FeedbackPacket*> fb_packets;
  for (auto it = in_out->begin(); it != in_out->end();) {
    if ((*it)->send_time_us() > 1000 * end_time_ms)
      break;
    if ((*it)->GetPacketType() == Packet::kFeedback &&
        flow_id == (*it)->flow_id()) {
      fb_packets.push_back(static_cast<FeedbackPacket*>(*it));
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  return fb_packets;
}

VideoSender::VideoSender(PacketProcessorListener* listener,
                         VideoSource* source,
                         BandwidthEstimatorType estimator_type)
    : PacketSender(listener, source->flow_id()),
      source_(source),
      bwe_(CreateBweSender(estimator_type,
                           source_->bits_per_second() / 1000,
                           this,
                           &clock_)),
      previous_sending_bitrate_(0) {
  modules_.push_back(bwe_.get());
}

VideoSender::~VideoSender() {
}

void VideoSender::Pause() {
  previous_sending_bitrate_ = TargetBitrateKbps();
  PacketSender::Pause();
}

void VideoSender::Resume(int64_t paused_time_ms) {
  source_->SetBitrateBps(previous_sending_bitrate_);
  PacketSender::Resume(paused_time_ms);
}

void VideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  std::list<FeedbackPacket*> feedbacks = GetFeedbackPackets(
      in_out, clock_.TimeInMilliseconds() + time_ms, source_->flow_id());
  ProcessFeedbackAndGeneratePackets(time_ms, &feedbacks, in_out);
}

void VideoSender::ProcessFeedbackAndGeneratePackets(
    int64_t time_ms,
    std::list<FeedbackPacket*>* feedbacks,
    Packets* packets) {
  do {
    // Make sure to at least run Process() below every 100 ms.
    int64_t time_to_run_ms = std::min<int64_t>(time_ms, 100);
    if (!feedbacks->empty()) {
      int64_t time_until_feedback_ms =
          feedbacks->front()->send_time_ms() - clock_.TimeInMilliseconds();
      time_to_run_ms =
          std::max<int64_t>(std::min(time_ms, time_until_feedback_ms), 0);
    }

    if (!running_) {
      source_->SetBitrateBps(0);
    }

    Packets generated;
    source_->RunFor(time_to_run_ms, &generated);
    bwe_->OnPacketsSent(generated);
    packets->merge(generated, DereferencingComparator<Packet>);

    clock_.AdvanceTimeMilliseconds(time_to_run_ms);

    if (!feedbacks->empty()) {
      bwe_->GiveFeedback(*feedbacks->front());
      delete feedbacks->front();
      feedbacks->pop_front();
    }

    bwe_->Process();

    time_ms -= time_to_run_ms;
  } while (time_ms > 0);
  assert(feedbacks->empty());
}

int VideoSender::GetFeedbackIntervalMs() const {
  return bwe_->GetFeedbackIntervalMs();
}

void VideoSender::OnNetworkChanged(uint32_t target_bitrate_bps,
                                   uint8_t fraction_lost,
                                   int64_t rtt) {
  source_->SetBitrateBps(target_bitrate_bps);
  RecordBitrate();
}

uint32_t VideoSender::TargetBitrateKbps() {
  return (source_->bits_per_second() + 500) / 1000;
}

PacedVideoSender::PacedVideoSender(PacketProcessorListener* listener,
                                   VideoSource* source,
                                   BandwidthEstimatorType estimator)
    : VideoSender(listener, source, estimator),
      pacer_(&clock_, this) {
  modules_.push_back(&pacer_);
  pacer_.SetEstimatedBitrate(source->bits_per_second());
}

PacedVideoSender::~PacedVideoSender() {
  for (Packet* packet : pacer_queue_)
    delete packet;
  for (Packet* packet : queue_)
    delete packet;
}

void PacedVideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  int64_t end_time_ms = clock_.TimeInMilliseconds() + time_ms;
  // Run process periodically to allow the packets to be paced out.
  std::list<FeedbackPacket*> feedbacks =
      GetFeedbackPackets(in_out, end_time_ms, source_->flow_id());
  int64_t last_run_time_ms = -1;
  BWE_TEST_LOGGING_CONTEXT("Sender");
  BWE_TEST_LOGGING_CONTEXT(source_->flow_id());
  do {
    int64_t time_until_process_ms = TimeUntilNextProcess(modules_);
    int64_t time_until_feedback_ms = time_ms;
    if (!feedbacks.empty())
      time_until_feedback_ms = std::max<int64_t>(
          feedbacks.front()->send_time_ms() - clock_.TimeInMilliseconds(), 0);

    int64_t time_until_next_event_ms =
        std::min(time_until_feedback_ms, time_until_process_ms);

    time_until_next_event_ms =
        std::min(source_->GetTimeUntilNextFrameMs(), time_until_next_event_ms);

    // Never run for longer than we have been asked for.
    if (clock_.TimeInMilliseconds() + time_until_next_event_ms > end_time_ms)
      time_until_next_event_ms = end_time_ms - clock_.TimeInMilliseconds();

    // Make sure we don't get stuck if an event doesn't trigger. This typically
    // happens if the prober wants to probe, but there's no packet to send.
    if (time_until_next_event_ms == 0 && last_run_time_ms == 0)
      time_until_next_event_ms = 1;
    last_run_time_ms = time_until_next_event_ms;

    Packets generated_packets;
    source_->RunFor(time_until_next_event_ms, &generated_packets);
    if (!generated_packets.empty()) {
      for (Packet* packet : generated_packets) {
        MediaPacket* media_packet = static_cast<MediaPacket*>(packet);
        pacer_.InsertPacket(
            PacedSender::kNormalPriority, media_packet->header().ssrc,
            media_packet->header().sequenceNumber, media_packet->send_time_ms(),
            media_packet->payload_size(), false);
        pacer_queue_.push_back(packet);
        assert(pacer_queue_.size() < 10000);
      }
    }

    clock_.AdvanceTimeMilliseconds(time_until_next_event_ms);

    if (time_until_next_event_ms == time_until_feedback_ms) {
      if (!feedbacks.empty()) {
        bwe_->GiveFeedback(*feedbacks.front());
        delete feedbacks.front();
        feedbacks.pop_front();
      }
      bwe_->Process();
    }

    if (time_until_next_event_ms == time_until_process_ms) {
      CallProcess(modules_);
    }
  } while (clock_.TimeInMilliseconds() < end_time_ms);
  QueuePackets(in_out, end_time_ms * 1000);
}

int64_t PacedVideoSender::TimeUntilNextProcess(
    const std::list<Module*>& modules) {
  int64_t time_until_next_process_ms = 10;
  for (Module* module : modules) {
    int64_t next_process_ms = module->TimeUntilNextProcess();
    if (next_process_ms < time_until_next_process_ms)
      time_until_next_process_ms = next_process_ms;
  }
  if (time_until_next_process_ms < 0)
    time_until_next_process_ms = 0;
  return time_until_next_process_ms;
}

void PacedVideoSender::CallProcess(const std::list<Module*>& modules) {
  for (Module* module : modules) {
    if (module->TimeUntilNextProcess() <= 0) {
      module->Process();
    }
  }
}

void PacedVideoSender::QueuePackets(Packets* batch,
                                    int64_t end_of_batch_time_us) {
  queue_.merge(*batch, DereferencingComparator<Packet>);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if ((*it)->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  bwe_->OnPacketsSent(to_transfer);
  batch->merge(to_transfer, DereferencingComparator<Packet>);
}

bool PacedVideoSender::TimeToSendPacket(uint32_t ssrc,
                                        uint16_t sequence_number,
                                        int64_t capture_time_ms,
                                        bool retransmission,
                                        const PacedPacketInfo& pacing_info) {
  for (Packets::iterator it = pacer_queue_.begin(); it != pacer_queue_.end();
       ++it) {
    MediaPacket* media_packet = static_cast<MediaPacket*>(*it);
    if (media_packet->header().sequenceNumber == sequence_number) {
      int64_t pace_out_time_ms = clock_.TimeInMilliseconds();

      // Make sure a packet is never paced out earlier than when it was put into
      // the pacer.
      assert(pace_out_time_ms >= media_packet->send_time_ms());

      media_packet->SetAbsSendTimeMs(pace_out_time_ms);
      media_packet->set_send_time_us(1000 * pace_out_time_ms);
      media_packet->set_sender_timestamp_us(1000 * pace_out_time_ms);
      queue_.push_back(media_packet);
      pacer_queue_.erase(it);
      return true;
    }
  }
  return false;
}

size_t PacedVideoSender::TimeToSendPadding(size_t bytes,
                                           const PacedPacketInfo& pacing_info) {
  return 0;
}

void PacedVideoSender::OnNetworkChanged(uint32_t target_bitrate_bps,
                                        uint8_t fraction_lost,
                                        int64_t rtt) {
  VideoSender::OnNetworkChanged(target_bitrate_bps, fraction_lost, rtt);
  pacer_.SetEstimatedBitrate(target_bitrate_bps);
}

const int kNoLimit = std::numeric_limits<int>::max();
const int kPacketSizeBytes = 1200;

TcpSender::TcpSender(PacketProcessorListener* listener,
                     int flow_id,
                     int64_t offset_ms)
    : TcpSender(listener, flow_id, offset_ms, kNoLimit) {
}

TcpSender::TcpSender(PacketProcessorListener* listener,
                     int flow_id,
                     int64_t offset_ms,
                     int send_limit_bytes)
    : PacketSender(listener, flow_id),
      cwnd_(10),
      ssthresh_(kNoLimit),
      ack_received_(false),
      last_acked_seq_num_(0),
      next_sequence_number_(0),
      offset_ms_(offset_ms),
      last_reduction_time_ms_(-1),
      last_rtt_ms_(0),
      total_sent_bytes_(0),
      send_limit_bytes_(send_limit_bytes),
      last_generated_packets_ms_(0),
      num_recent_sent_packets_(0),
      bitrate_kbps_(0) {
}

void TcpSender::RunFor(int64_t time_ms, Packets* in_out) {
  if (clock_.TimeInMilliseconds() + time_ms < offset_ms_) {
    clock_.AdvanceTimeMilliseconds(time_ms);
    if (running_) {
      Pause();
    }
    return;
  }

  if (!running_ && total_sent_bytes_ == 0) {
    Resume(offset_ms_);
  }

  int64_t start_time_ms = clock_.TimeInMilliseconds();

  std::list<FeedbackPacket*> feedbacks = GetFeedbackPackets(
      in_out, clock_.TimeInMilliseconds() + time_ms, *flow_ids().begin());
  // The number of packets which are sent in during time_ms depends on the
  // number of packets in_flight_ and the max number of packets in flight
  // (cwnd_). Therefore SendPackets() isn't directly dependent on time_ms.
  for (FeedbackPacket* fb : feedbacks) {
    clock_.AdvanceTimeMilliseconds(fb->send_time_ms() -
                                   clock_.TimeInMilliseconds());
    last_rtt_ms_ = fb->send_time_ms() - fb->latest_send_time_ms();
    UpdateCongestionControl(fb);
    SendPackets(in_out);
  }

  for (auto it = in_flight_.begin(); it != in_flight_.end();) {
    if (it->time_ms < clock_.TimeInMilliseconds() - 1000)
      in_flight_.erase(it++);
    else
      ++it;
  }

  clock_.AdvanceTimeMilliseconds(time_ms -
                                 (clock_.TimeInMilliseconds() - start_time_ms));
  SendPackets(in_out);
}

void TcpSender::SendPackets(Packets* in_out) {
  int cwnd = ceil(cwnd_);
  int packets_to_send = std::max(cwnd - static_cast<int>(in_flight_.size()), 0);
  int timed_out = TriggerTimeouts();
  if (timed_out > 0) {
    HandleLoss();
  }
  if (packets_to_send > 0) {
    Packets generated = GeneratePackets(packets_to_send);
    for (Packet* packet : generated)
      in_flight_.insert(InFlight(*static_cast<MediaPacket*>(packet)));

    in_out->merge(generated, DereferencingComparator<Packet>);
  }
}

void TcpSender::UpdateCongestionControl(const FeedbackPacket* fb) {
  const TcpFeedback* tcp_fb = static_cast<const TcpFeedback*>(fb);
  RTC_DCHECK(!tcp_fb->acked_packets().empty());
  ack_received_ = true;

  uint16_t expected = tcp_fb->acked_packets().back() - last_acked_seq_num_;
  uint16_t missing =
      expected - static_cast<uint16_t>(tcp_fb->acked_packets().size());

  for (uint16_t ack_seq_num : tcp_fb->acked_packets())
    in_flight_.erase(InFlight(ack_seq_num, clock_.TimeInMilliseconds()));

  if (missing > 0) {
    HandleLoss();
  } else if (cwnd_ <= ssthresh_) {
    cwnd_ += tcp_fb->acked_packets().size();
  } else {
    cwnd_ += 1.0f / cwnd_;
  }

  last_acked_seq_num_ =
      LatestSequenceNumber(tcp_fb->acked_packets().back(), last_acked_seq_num_);
}

int TcpSender::TriggerTimeouts() {
  int timed_out = 0;
  for (auto it = in_flight_.begin(); it != in_flight_.end();) {
    if (it->time_ms < clock_.TimeInMilliseconds() - 1000) {
      in_flight_.erase(it++);
      ++timed_out;
    } else {
      ++it;
    }
  }
  return timed_out;
}

void TcpSender::HandleLoss() {
  if (clock_.TimeInMilliseconds() - last_reduction_time_ms_ < last_rtt_ms_)
    return;
  last_reduction_time_ms_ = clock_.TimeInMilliseconds();
  ssthresh_ = std::max(static_cast<int>(in_flight_.size() / 2), 2);
  cwnd_ = ssthresh_;
}

Packets TcpSender::GeneratePackets(size_t num_packets) {
  Packets generated;

  UpdateSendBitrateEstimate(num_packets);

  for (size_t i = 0; i < num_packets; ++i) {
    if ((total_sent_bytes_ + kPacketSizeBytes) > send_limit_bytes_) {
      if (running_) {
        Pause();
      }
      break;
    }
    generated.push_back(
        new MediaPacket(*flow_ids().begin(), 1000 * clock_.TimeInMilliseconds(),
                        kPacketSizeBytes, next_sequence_number_++));
    generated.back()->set_sender_timestamp_us(
        1000 * clock_.TimeInMilliseconds());

    total_sent_bytes_ += kPacketSizeBytes;
  }

  return generated;
}

void TcpSender::UpdateSendBitrateEstimate(size_t num_packets) {
  const int kTimeWindowMs = 500;
  num_recent_sent_packets_ += num_packets;

  int64_t delta_ms = clock_.TimeInMilliseconds() - last_generated_packets_ms_;
  if (delta_ms >= kTimeWindowMs) {
    bitrate_kbps_ =
        static_cast<uint32_t>(8 * num_recent_sent_packets_ * kPacketSizeBytes) /
        delta_ms;
    last_generated_packets_ms_ = clock_.TimeInMilliseconds();
    num_recent_sent_packets_ = 0;
  }

  RecordBitrate();
}

uint32_t TcpSender::TargetBitrateKbps() {
  return bitrate_kbps_;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
