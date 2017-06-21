/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/include/send_side_congestion_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/base/socket.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/acknowledge_bitrate_estimator.h"
#include "webrtc/modules/congestion_controller/probe_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"

namespace webrtc {
namespace {

static const int64_t kRetransmitWindowSizeMs = 500;

// Makes sure that the bitrate and the min, max values are in valid range.
static void ClampBitrates(int* bitrate_bps,
                          int* min_bitrate_bps,
                          int* max_bitrate_bps) {
  // TODO(holmer): We should make sure the default bitrates are set to 10 kbps,
  // and that we don't try to set the min bitrate to 0 from any applications.
  // The congestion controller should allow a min bitrate of 0.
  if (*min_bitrate_bps < congestion_controller::GetMinBitrateBps())
    *min_bitrate_bps = congestion_controller::GetMinBitrateBps();
  if (*max_bitrate_bps > 0)
    *max_bitrate_bps = std::max(*min_bitrate_bps, *max_bitrate_bps);
  if (*bitrate_bps > 0)
    *bitrate_bps = std::max(*min_bitrate_bps, *bitrate_bps);
}

std::vector<webrtc::PacketFeedback> ReceivedPacketFeedbackVector(
    const std::vector<webrtc::PacketFeedback>& input) {
  std::vector<PacketFeedback> received_packet_feedback_vector;
  auto is_received = [](const webrtc::PacketFeedback& packet_feedback) {
    return packet_feedback.arrival_time_ms !=
           webrtc::PacketFeedback::kNotReceived;
  };
  std::copy_if(input.begin(), input.end(),
               std::back_inserter(received_packet_feedback_vector),
               is_received);
  return received_packet_feedback_vector;
}

void SortPacketFeedbackVector(
    std::vector<webrtc::PacketFeedback>* const input) {
  RTC_DCHECK(input);
  std::sort(input->begin(), input->end(), PacketFeedbackComparator());
}

}  // namespace

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    Observer* observer,
    RtcEventLog* event_log,
    PacketRouter* packet_router)
    : SendSideCongestionController(
          clock,
          observer,
          event_log,
          std::unique_ptr<PacedSender>(
              new PacedSender(clock, packet_router, event_log))) {}

SendSideCongestionController::SendSideCongestionController(
    const Clock* clock,
    Observer* observer,
    RtcEventLog* event_log,
    std::unique_ptr<PacedSender> pacer)
    : clock_(clock),
      observer_(observer),
      event_log_(event_log),
      pacer_(std::move(pacer)),
      bitrate_controller_(
          BitrateController::CreateBitrateController(clock_, event_log)),
      acknowledged_bitrate_estimator_(
          rtc::MakeUnique<AcknowledgedBitrateEstimator>()),
      probe_controller_(new ProbeController(pacer_.get(), clock_)),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kRetransmitWindowSizeMs)),
      transport_feedback_adapter_(clock_),
      last_reported_bitrate_bps_(0),
      last_reported_fraction_loss_(0),
      last_reported_rtt_(0),
      network_state_(kNetworkUp),
      min_bitrate_bps_(congestion_controller::GetMinBitrateBps()),
      delay_based_bwe_(new DelayBasedBwe(event_log_, clock_)) {
  delay_based_bwe_->SetMinBitrate(min_bitrate_bps_);
}

SendSideCongestionController::~SendSideCongestionController() {}

void SendSideCongestionController::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void SendSideCongestionController::RegisterNetworkObserver(Observer* observer) {
  rtc::CritScope cs(&observer_lock_);
  RTC_DCHECK(observer_ == nullptr);
  observer_ = observer;
}

void SendSideCongestionController::DeRegisterNetworkObserver(
    Observer* observer) {
  rtc::CritScope cs(&observer_lock_);
  RTC_DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

void SendSideCongestionController::SetBweBitrates(int min_bitrate_bps,
                                                  int start_bitrate_bps,
                                                  int max_bitrate_bps) {
  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);
  bitrate_controller_->SetBitrates(start_bitrate_bps, min_bitrate_bps,
                                   max_bitrate_bps);

  probe_controller_->SetBitrates(min_bitrate_bps, start_bitrate_bps,
                                 max_bitrate_bps);

  {
    rtc::CritScope cs(&bwe_lock_);
    if (start_bitrate_bps > 0)
      delay_based_bwe_->SetStartBitrate(start_bitrate_bps);
    min_bitrate_bps_ = min_bitrate_bps;
    delay_based_bwe_->SetMinBitrate(min_bitrate_bps_);
  }
  MaybeTriggerOnNetworkChanged();
}

// TODO(holmer): Split this up and use SetBweBitrates in combination with
// OnNetworkRouteChanged.
void SendSideCongestionController::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  ClampBitrates(&bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);
  // TODO(honghaiz): Recreate this object once the bitrate controller is
  // no longer exposed outside SendSideCongestionController.
  bitrate_controller_->ResetBitrates(bitrate_bps, min_bitrate_bps,
                                     max_bitrate_bps);

  transport_feedback_adapter_.SetNetworkIds(network_route.local_network_id,
                                            network_route.remote_network_id);
  {
    rtc::CritScope cs(&bwe_lock_);
    min_bitrate_bps_ = min_bitrate_bps;
    delay_based_bwe_.reset(new DelayBasedBwe(event_log_, clock_));
    acknowledged_bitrate_estimator_.reset(new AcknowledgedBitrateEstimator());
    delay_based_bwe_->SetStartBitrate(bitrate_bps);
    delay_based_bwe_->SetMinBitrate(min_bitrate_bps);
  }

  probe_controller_->Reset();
  probe_controller_->SetBitrates(min_bitrate_bps, bitrate_bps, max_bitrate_bps);

  MaybeTriggerOnNetworkChanged();
}

BitrateController* SendSideCongestionController::GetBitrateController() const {
  return bitrate_controller_.get();
}

RateLimiter* SendSideCongestionController::GetRetransmissionRateLimiter() {
  return retransmission_rate_limiter_.get();
}

void SendSideCongestionController::EnablePeriodicAlrProbing(bool enable) {
  probe_controller_->EnablePeriodicAlrProbing(enable);
}

void SendSideCongestionController::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps) {
  pacer_->SetSendBitrateLimits(min_send_bitrate_bps, max_padding_bitrate_bps);
}

int64_t SendSideCongestionController::GetPacerQueuingDelayMs() const {
  return IsNetworkDown() ? 0 : pacer_->QueueInMs();
}

int64_t SendSideCongestionController::GetFirstPacketTimeMs() const {
  return pacer_->FirstSentPacketTimeMs();
}

PacedSender* SendSideCongestionController::pacer() {
  return pacer_.get();
}

TransportFeedbackObserver*
SendSideCongestionController::GetTransportFeedbackObserver() {
  return this;
}

void SendSideCongestionController::SignalNetworkState(NetworkState state) {
  LOG(LS_INFO) << "SignalNetworkState "
               << (state == kNetworkUp ? "Up" : "Down");
  if (state == kNetworkUp) {
    pacer_->Resume();
  } else {
    pacer_->Pause();
  }
  {
    rtc::CritScope cs(&network_state_lock_);
    network_state_ = state;
  }
  probe_controller_->OnNetworkStateChanged(state);
  MaybeTriggerOnNetworkChanged();
}

void SendSideCongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  transport_feedback_adapter_.SetTransportOverhead(
      transport_overhead_bytes_per_packet);
}

void SendSideCongestionController::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  // We're not interested in packets without an id, which may be stun packets,
  // etc, sent on the same transport.
  if (sent_packet.packet_id == -1)
    return;
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                           sent_packet.send_time_ms);
}

void SendSideCongestionController::OnRttUpdate(int64_t avg_rtt_ms,
                                               int64_t max_rtt_ms) {
  rtc::CritScope cs(&bwe_lock_);
  delay_based_bwe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

int64_t SendSideCongestionController::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

void SendSideCongestionController::Process() {
  bitrate_controller_->Process();
  probe_controller_->Process();
  MaybeTriggerOnNetworkChanged();
}

void SendSideCongestionController::AddPacket(
    uint32_t ssrc,
    uint16_t sequence_number,
    size_t length,
    const PacedPacketInfo& pacing_info) {
  transport_feedback_adapter_.AddPacket(ssrc, sequence_number, length,
                                        pacing_info);
}

void SendSideCongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  transport_feedback_adapter_.OnTransportFeedback(feedback);
  std::vector<PacketFeedback> feedback_vector = ReceivedPacketFeedbackVector(
      transport_feedback_adapter_.GetTransportFeedbackVector());
  SortPacketFeedbackVector(&feedback_vector);
  acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
      feedback_vector);
  DelayBasedBwe::Result result;
  {
    rtc::CritScope cs(&bwe_lock_);
    result = delay_based_bwe_->IncomingPacketFeedbackVector(
        feedback_vector, acknowledged_bitrate_estimator_->bitrate_bps());
  }
  if (result.updated)
    bitrate_controller_->OnDelayBasedBweResult(result);
}

std::vector<PacketFeedback>
SendSideCongestionController::GetTransportFeedbackVector() const {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);
  return transport_feedback_adapter_.GetTransportFeedbackVector();
}

void SendSideCongestionController::MaybeTriggerOnNetworkChanged() {
  uint32_t bitrate_bps;
  uint8_t fraction_loss;
  int64_t rtt;
  bool estimate_changed = bitrate_controller_->GetNetworkParameters(
      &bitrate_bps, &fraction_loss, &rtt);
  if (estimate_changed) {
    pacer_->SetEstimatedBitrate(bitrate_bps);
    probe_controller_->SetEstimatedBitrate(bitrate_bps);
    retransmission_rate_limiter_->SetMaxRate(bitrate_bps);
  }

  bitrate_bps = IsNetworkDown() || IsSendQueueFull() ? 0 : bitrate_bps;

  if (HasNetworkParametersToReportChanged(bitrate_bps, fraction_loss, rtt)) {
    int64_t probing_interval_ms;
    {
      rtc::CritScope cs(&bwe_lock_);
      probing_interval_ms = delay_based_bwe_->GetExpectedBwePeriodMs();
    }
    {
      rtc::CritScope cs(&observer_lock_);
      if (observer_) {
        observer_->OnNetworkChanged(bitrate_bps, fraction_loss, rtt,
                                    probing_interval_ms);
      }
    }
  }
}

bool SendSideCongestionController::HasNetworkParametersToReportChanged(
    uint32_t bitrate_bps,
    uint8_t fraction_loss,
    int64_t rtt) {
  rtc::CritScope cs(&network_state_lock_);
  bool changed =
      last_reported_bitrate_bps_ != bitrate_bps ||
      (bitrate_bps > 0 && (last_reported_fraction_loss_ != fraction_loss ||
                           last_reported_rtt_ != rtt));
  if (changed && (last_reported_bitrate_bps_ == 0 || bitrate_bps == 0)) {
    LOG(LS_INFO) << "Bitrate estimate state changed, BWE: " << bitrate_bps
                 << " bps.";
  }
  last_reported_bitrate_bps_ = bitrate_bps;
  last_reported_fraction_loss_ = fraction_loss;
  last_reported_rtt_ = rtt;
  return changed;
}

bool SendSideCongestionController::IsSendQueueFull() const {
  return pacer_->ExpectedQueueTimeMs() > PacedSender::kMaxQueueLengthMs;
}

bool SendSideCongestionController::IsNetworkDown() const {
  rtc::CritScope cs(&network_state_lock_);
  return network_state_ == kNetworkDown;
}

}  // namespace webrtc
