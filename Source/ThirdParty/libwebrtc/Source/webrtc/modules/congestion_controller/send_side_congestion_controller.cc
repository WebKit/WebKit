/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/send_side_congestion_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/congestion_controller/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/probe_controller.h"
#include "modules/pacing/alr_detector.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/socket.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

const char kCwndExperiment[] = "WebRTC-CwndExperiment";
const char kPacerPushbackExperiment[] = "WebRTC-PacerPushbackExperiment";
const int64_t kDefaultAcceptedQueueMs = 250;

bool CwndExperimentEnabled() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCwndExperiment);
  // The experiment is enabled iff the field trial string begins with "Enabled".
  return experiment_string.find("Enabled") == 0;
}

bool ReadCwndExperimentParameter(int64_t* accepted_queue_ms) {
  RTC_DCHECK(accepted_queue_ms);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCwndExperiment);
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%" PRId64, accepted_queue_ms);
  if (parsed_values == 1) {
    RTC_CHECK_GE(*accepted_queue_ms, 0)
        << "Accepted must be greater than or equal to 0.";
    return true;
  }
  return false;
}

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
    PacedSender* pacer)
    : clock_(clock),
      observer_(observer),
      event_log_(event_log),
      pacer_(pacer),
      bitrate_controller_(
          BitrateController::CreateBitrateController(clock_, event_log)),
      acknowledged_bitrate_estimator_(
          rtc::MakeUnique<AcknowledgedBitrateEstimator>()),
      probe_controller_(new ProbeController(pacer_, clock_)),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kRetransmitWindowSizeMs)),
      transport_feedback_adapter_(clock_),
      last_reported_bitrate_bps_(0),
      last_reported_fraction_loss_(0),
      last_reported_rtt_(0),
      network_state_(kNetworkUp),
      pause_pacer_(false),
      pacer_paused_(false),
      min_bitrate_bps_(congestion_controller::GetMinBitrateBps()),
      delay_based_bwe_(new DelayBasedBwe(event_log_, clock_)),
      in_cwnd_experiment_(CwndExperimentEnabled()),
      accepted_queue_ms_(kDefaultAcceptedQueueMs),
      was_in_alr_(false),
      pacer_pushback_experiment_(
          webrtc::field_trial::IsEnabled(kPacerPushbackExperiment)) {
  delay_based_bwe_->SetMinBitrate(min_bitrate_bps_);
  if (in_cwnd_experiment_ &&
      !ReadCwndExperimentParameter(&accepted_queue_ms_)) {
    RTC_LOG(LS_WARNING) << "Failed to parse parameters for CwndExperiment "
                           "from field trial string. Experiment disabled.";
    in_cwnd_experiment_ = false;
  }
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

bool SendSideCongestionController::AvailableBandwidth(
    uint32_t* bandwidth) const {
  return bitrate_controller_->AvailableBandwidth(bandwidth);
}

RtcpBandwidthObserver* SendSideCongestionController::GetBandwidthObserver()
    const {
  return bitrate_controller_.get();
}

RateLimiter* SendSideCongestionController::GetRetransmissionRateLimiter() {
  return retransmission_rate_limiter_.get();
}

void SendSideCongestionController::EnablePeriodicAlrProbing(bool enable) {
  probe_controller_->EnablePeriodicAlrProbing(enable);
}

int64_t SendSideCongestionController::GetPacerQueuingDelayMs() const {
  return IsNetworkDown() ? 0 : pacer_->QueueInMs();
}

int64_t SendSideCongestionController::GetFirstPacketTimeMs() const {
  return pacer_->FirstSentPacketTimeMs();
}

TransportFeedbackObserver*
SendSideCongestionController::GetTransportFeedbackObserver() {
  return this;
}

void SendSideCongestionController::SignalNetworkState(NetworkState state) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (state == kNetworkUp ? "Up" : "Down");
  {
    rtc::CritScope cs(&network_state_lock_);
    pause_pacer_ = state == kNetworkDown;
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
  if (in_cwnd_experiment_)
    LimitOutstandingBytes(transport_feedback_adapter_.GetOutstandingBytes());
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
  bool pause_pacer;
  // TODO(holmer): Once this class is running on a task queue we should
  // replace this with a task instead.
  {
    rtc::CritScope lock(&network_state_lock_);
    pause_pacer = pause_pacer_;
  }
  if (pause_pacer && !pacer_paused_) {
    pacer_->Pause();
    pacer_paused_ = true;
  } else if (!pause_pacer && pacer_paused_) {
    pacer_->Resume();
    pacer_paused_ = false;
  }
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

  bool currently_in_alr =
      pacer_->GetApplicationLimitedRegionStartTime().has_value();
  if (was_in_alr_ && !currently_in_alr) {
    int64_t now_ms = rtc::TimeMillis();
    acknowledged_bitrate_estimator_->SetAlrEndedTimeMs(now_ms);
    probe_controller_->SetAlrEndedTimeMs(now_ms);
  }
  was_in_alr_ = currently_in_alr;

  acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
      feedback_vector);
  DelayBasedBwe::Result result;
  {
    rtc::CritScope cs(&bwe_lock_);
    result = delay_based_bwe_->IncomingPacketFeedbackVector(
        feedback_vector, acknowledged_bitrate_estimator_->bitrate_bps());
  }
  if (result.updated) {
    bitrate_controller_->OnDelayBasedBweResult(result);
    // Update the estimate in the ProbeController, in case we want to probe.
    MaybeTriggerOnNetworkChanged();
  }
  if (result.recovered_from_overuse)
    probe_controller_->RequestProbe();
  if (in_cwnd_experiment_)
    LimitOutstandingBytes(transport_feedback_adapter_.GetOutstandingBytes());
}

void SendSideCongestionController::LimitOutstandingBytes(
    size_t num_outstanding_bytes) {
  RTC_DCHECK(in_cwnd_experiment_);
  rtc::CritScope lock(&network_state_lock_);
  rtc::Optional<int64_t> min_rtt_ms =
      transport_feedback_adapter_.GetMinFeedbackLoopRtt();
  // No valid RTT. Could be because send-side BWE isn't used, in which case
  // we don't try to limit the outstanding packets.
  if (!min_rtt_ms)
    return;
  const size_t kMinCwndBytes = 2 * 1500;
  size_t max_outstanding_bytes =
      std::max<size_t>((*min_rtt_ms + accepted_queue_ms_) *
                           last_reported_bitrate_bps_ / 1000 / 8,
                       kMinCwndBytes);
  RTC_LOG(LS_INFO) << clock_->TimeInMilliseconds()
                   << " Outstanding bytes: " << num_outstanding_bytes
                   << " pacer queue: " << pacer_->QueueInMs()
                   << " max outstanding: " << max_outstanding_bytes;
  RTC_LOG(LS_INFO) << "Feedback rtt: " << *min_rtt_ms
                   << " Bitrate: " << last_reported_bitrate_bps_;
  pause_pacer_ = num_outstanding_bytes > max_outstanding_bytes;
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

  if (!pacer_pushback_experiment_) {
    bitrate_bps = IsNetworkDown() || IsSendQueueFull() ? 0 : bitrate_bps;
  } else {
    if (IsNetworkDown()) {
      bitrate_bps = 0;
    } else {
      int64_t queue_length_ms = pacer_->ExpectedQueueTimeMs();

      if (queue_length_ms == 0) {
        encoding_rate_ = 1.0;
      } else if (queue_length_ms > 50) {
        float encoding_rate = 1.0 - queue_length_ms / 1000.0;
        encoding_rate_ = std::min(encoding_rate_, encoding_rate);
        encoding_rate_ = std::max(encoding_rate_, 0.0f);
      }

      bitrate_bps *= encoding_rate_;
      bitrate_bps = bitrate_bps < 50000 ? 0 : bitrate_bps;
    }
  }

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
    RTC_LOG(LS_INFO) << "Bitrate estimate state changed, BWE: " << bitrate_bps
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
