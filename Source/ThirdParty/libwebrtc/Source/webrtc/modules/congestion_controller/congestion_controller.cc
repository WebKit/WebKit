/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/include/congestion_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/base/socket.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/probe_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

namespace webrtc {
namespace {

static const uint32_t kTimeOffsetSwitchThreshold = 30;
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

}  // namespace

CongestionController::WrappingBitrateEstimator::WrappingBitrateEstimator(
    RemoteBitrateObserver* observer, Clock* clock)
    : observer_(observer),
      clock_(clock),
      rbe_(new RemoteBitrateEstimatorSingleStream(observer_, clock_)),
      using_absolute_send_time_(false),
      packets_since_absolute_send_time_(0),
      min_bitrate_bps_(congestion_controller::GetMinBitrateBps()) {}

void CongestionController::WrappingBitrateEstimator::IncomingPacket(
    int64_t arrival_time_ms,
    size_t payload_size,
    const RTPHeader& header) {
  rtc::CritScope cs(&crit_sect_);
  PickEstimatorFromHeader(header);
  rbe_->IncomingPacket(arrival_time_ms, payload_size, header);
}

void CongestionController::WrappingBitrateEstimator::Process() {
  rtc::CritScope cs(&crit_sect_);
  rbe_->Process();
}

int64_t CongestionController::WrappingBitrateEstimator::TimeUntilNextProcess() {
  rtc::CritScope cs(&crit_sect_);
  return rbe_->TimeUntilNextProcess();
}

void CongestionController::WrappingBitrateEstimator::OnRttUpdate(
    int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  rtc::CritScope cs(&crit_sect_);
  rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

void CongestionController::WrappingBitrateEstimator::RemoveStream(
    unsigned int ssrc) {
  rtc::CritScope cs(&crit_sect_);
  rbe_->RemoveStream(ssrc);
}

bool CongestionController::WrappingBitrateEstimator::LatestEstimate(
    std::vector<unsigned int>* ssrcs,
    unsigned int* bitrate_bps) const {
  rtc::CritScope cs(&crit_sect_);
  return rbe_->LatestEstimate(ssrcs, bitrate_bps);
}

void CongestionController::WrappingBitrateEstimator::SetMinBitrate(
    int min_bitrate_bps) {
  rtc::CritScope cs(&crit_sect_);
  rbe_->SetMinBitrate(min_bitrate_bps);
  min_bitrate_bps_ = min_bitrate_bps;
}

void CongestionController::WrappingBitrateEstimator::PickEstimatorFromHeader(
    const RTPHeader& header) {
  if (header.extension.hasAbsoluteSendTime) {
    // If we see AST in header, switch RBE strategy immediately.
    if (!using_absolute_send_time_) {
      LOG(LS_INFO) <<
          "WrappingBitrateEstimator: Switching to absolute send time RBE.";
      using_absolute_send_time_ = true;
      PickEstimator();
    }
    packets_since_absolute_send_time_ = 0;
  } else {
    // When we don't see AST, wait for a few packets before going back to TOF.
    if (using_absolute_send_time_) {
      ++packets_since_absolute_send_time_;
      if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
        LOG(LS_INFO) << "WrappingBitrateEstimator: Switching to transmission "
                     << "time offset RBE.";
        using_absolute_send_time_ = false;
        PickEstimator();
      }
    }
  }
}

// Instantiate RBE for Time Offset or Absolute Send Time extensions.
void CongestionController::WrappingBitrateEstimator::PickEstimator() {
  if (using_absolute_send_time_) {
    rbe_.reset(new RemoteBitrateEstimatorAbsSendTime(observer_, clock_));
  } else {
    rbe_.reset(new RemoteBitrateEstimatorSingleStream(observer_, clock_));
  }
  rbe_->SetMinBitrate(min_bitrate_bps_);
}

CongestionController::CongestionController(
    Clock* clock,
    Observer* observer,
    RemoteBitrateObserver* remote_bitrate_observer,
    RtcEventLog* event_log,
    PacketRouter* packet_router)
    : CongestionController(
          clock,
          observer,
          remote_bitrate_observer,
          event_log,
          packet_router,
          std::unique_ptr<PacedSender>(new PacedSender(clock, packet_router))) {
}

CongestionController::CongestionController(
    Clock* clock,
    Observer* observer,
    RemoteBitrateObserver* remote_bitrate_observer,
    RtcEventLog* event_log,
    PacketRouter* packet_router,
    std::unique_ptr<PacedSender> pacer)
    : clock_(clock),
      observer_(observer),
      packet_router_(packet_router),
      pacer_(std::move(pacer)),
      bitrate_controller_(
          BitrateController::CreateBitrateController(clock_, event_log)),
      probe_controller_(new ProbeController(pacer_.get(), clock_)),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kRetransmitWindowSizeMs)),
      remote_bitrate_estimator_(remote_bitrate_observer, clock_),
      remote_estimator_proxy_(clock_, packet_router_),
      transport_feedback_adapter_(event_log, clock_, bitrate_controller_.get()),
      min_bitrate_bps_(congestion_controller::GetMinBitrateBps()),
      max_bitrate_bps_(0),
      last_reported_bitrate_bps_(0),
      last_reported_fraction_loss_(0),
      last_reported_rtt_(0),
      network_state_(kNetworkUp) {
  transport_feedback_adapter_.InitBwe();
  transport_feedback_adapter_.SetMinBitrate(min_bitrate_bps_);
}

CongestionController::~CongestionController() {}

void CongestionController::OnReceivedPacket(int64_t arrival_time_ms,
                                            size_t payload_size,
                                            const RTPHeader& header) {
  // Send-side BWE.
  if (header.extension.hasTransportSequenceNumber) {
    remote_estimator_proxy_.IncomingPacket(arrival_time_ms, payload_size,
                                           header);
  } else {
    // Receive-side BWE.
    remote_bitrate_estimator_.IncomingPacket(arrival_time_ms, payload_size,
                                              header);
  }
}

void CongestionController::SetBweBitrates(int min_bitrate_bps,
                                          int start_bitrate_bps,
                                          int max_bitrate_bps) {
  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);
  bitrate_controller_->SetBitrates(start_bitrate_bps,
                                   min_bitrate_bps,
                                   max_bitrate_bps);

  probe_controller_->SetBitrates(min_bitrate_bps, start_bitrate_bps,
                                 max_bitrate_bps);
  max_bitrate_bps_ = max_bitrate_bps;

  remote_bitrate_estimator_.SetMinBitrate(min_bitrate_bps);
  min_bitrate_bps_ = min_bitrate_bps;
  if (start_bitrate_bps > 0)
    transport_feedback_adapter_.SetStartBitrate(start_bitrate_bps);
  transport_feedback_adapter_.SetMinBitrate(min_bitrate_bps_);
  MaybeTriggerOnNetworkChanged();
}

void CongestionController::ResetBweAndBitrates(int bitrate_bps,
                                               int min_bitrate_bps,
                                               int max_bitrate_bps) {
  ClampBitrates(&bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);
  // TODO(honghaiz): Recreate this object once the bitrate controller is
  // no longer exposed outside CongestionController.
  bitrate_controller_->ResetBitrates(bitrate_bps, min_bitrate_bps,
                                     max_bitrate_bps);
  min_bitrate_bps_ = min_bitrate_bps;
  max_bitrate_bps_ = max_bitrate_bps;
  // TODO(honghaiz): Recreate this object once the remote bitrate estimator is
  // no longer exposed outside CongestionController.
  remote_bitrate_estimator_.SetMinBitrate(min_bitrate_bps);

  transport_feedback_adapter_.InitBwe();
  transport_feedback_adapter_.SetStartBitrate(bitrate_bps);
  transport_feedback_adapter_.SetMinBitrate(min_bitrate_bps);
  // TODO(holmer): Trigger a new probe once mid-call probing is implemented.
  MaybeTriggerOnNetworkChanged();
}

BitrateController* CongestionController::GetBitrateController() const {
  return bitrate_controller_.get();
}

RemoteBitrateEstimator* CongestionController::GetRemoteBitrateEstimator(
    bool send_side_bwe) {
  if (send_side_bwe) {
    return &remote_estimator_proxy_;
  } else {
    return &remote_bitrate_estimator_;
  }
}

TransportFeedbackObserver*
CongestionController::GetTransportFeedbackObserver() {
  return &transport_feedback_adapter_;
}

RateLimiter* CongestionController::GetRetransmissionRateLimiter() {
  return retransmission_rate_limiter_.get();
}

void CongestionController::EnablePeriodicAlrProbing(bool enable) {
  probe_controller_->EnablePeriodicAlrProbing(enable);
}

void CongestionController::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps) {
  pacer_->SetSendBitrateLimits(min_send_bitrate_bps, max_padding_bitrate_bps);
}

int64_t CongestionController::GetPacerQueuingDelayMs() const {
  return IsNetworkDown() ? 0 : pacer_->QueueInMs();
}

void CongestionController::SignalNetworkState(NetworkState state) {
  LOG(LS_INFO) << "SignalNetworkState "
               << (state == kNetworkUp ? "Up" : "Down");
  if (state == kNetworkUp) {
    pacer_->Resume();
  } else {
    pacer_->Pause();
  }
  {
    rtc::CritScope cs(&critsect_);
    network_state_ = state;
  }
  probe_controller_->OnNetworkStateChanged(state);
  MaybeTriggerOnNetworkChanged();
}

void CongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  transport_feedback_adapter_.SetTransportOverhead(
      transport_overhead_bytes_per_packet);
}

void CongestionController::OnSentPacket(const rtc::SentPacket& sent_packet) {
  // We're not interested in packets without an id, which may be stun packets,
  // etc, sent on the same transport.
  if (sent_packet.packet_id == -1)
    return;
  transport_feedback_adapter_.OnSentPacket(sent_packet.packet_id,
                                           sent_packet.send_time_ms);
}

void CongestionController::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  remote_bitrate_estimator_.OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  transport_feedback_adapter_.OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

int64_t CongestionController::TimeUntilNextProcess() {
  return std::min(bitrate_controller_->TimeUntilNextProcess(),
                  remote_bitrate_estimator_.TimeUntilNextProcess());
}

void CongestionController::Process() {
  bitrate_controller_->Process();
  remote_bitrate_estimator_.Process();
  probe_controller_->Process();
  MaybeTriggerOnNetworkChanged();
}

void CongestionController::MaybeTriggerOnNetworkChanged() {
  // TODO(perkj): |observer_| can be nullptr if the ctor that accepts a
  // BitrateObserver is used. Remove this check once the ctor is removed.
  if (!observer_)
    return;

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
    observer_->OnNetworkChanged(
        bitrate_bps, fraction_loss, rtt,
        transport_feedback_adapter_.GetProbingIntervalMs());
    remote_estimator_proxy_.OnBitrateChanged(bitrate_bps);
  }
}

bool CongestionController::HasNetworkParametersToReportChanged(
    uint32_t bitrate_bps,
    uint8_t fraction_loss,
    int64_t rtt) {
  rtc::CritScope cs(&critsect_);
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

bool CongestionController::IsSendQueueFull() const {
  return pacer_->ExpectedQueueTimeMs() > PacedSender::kMaxQueueLengthMs;
}

bool CongestionController::IsNetworkDown() const {
  rtc::CritScope cs(&critsect_);
  return network_state_ == kNetworkDown;
}

}  // namespace webrtc
