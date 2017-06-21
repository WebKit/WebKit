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

void CongestionController::OnReceivedPacket(int64_t arrival_time_ms,
                                            size_t payload_size,
                                            const RTPHeader& header) {
  receive_side_cc_.OnReceivedPacket(arrival_time_ms, payload_size, header);
}

void CongestionController::SetBweBitrates(int min_bitrate_bps,
                                          int start_bitrate_bps,
                                          int max_bitrate_bps) {
  send_side_cc_.SetBweBitrates(min_bitrate_bps, start_bitrate_bps,
                               max_bitrate_bps);
}

// TODO(holmer): Split this up and use SetBweBitrates in combination with
// OnNetworkRouteChanged.
void CongestionController::OnNetworkRouteChanged(
    const rtc::NetworkRoute& network_route,
    int bitrate_bps,
    int min_bitrate_bps,
    int max_bitrate_bps) {
  send_side_cc_.OnNetworkRouteChanged(network_route, bitrate_bps,
                                      min_bitrate_bps, max_bitrate_bps);
}

BitrateController* CongestionController::GetBitrateController() const {
  return send_side_cc_.GetBitrateController();
}

RemoteBitrateEstimator* CongestionController::GetRemoteBitrateEstimator(
    bool send_side_bwe) {
  return receive_side_cc_.GetRemoteBitrateEstimator(send_side_bwe);
}

RateLimiter* CongestionController::GetRetransmissionRateLimiter() {
  return send_side_cc_.GetRetransmissionRateLimiter();
}

void CongestionController::EnablePeriodicAlrProbing(bool enable) {
  send_side_cc_.EnablePeriodicAlrProbing(enable);
}

void CongestionController::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps) {
  send_side_cc_.SetAllocatedSendBitrateLimits(min_send_bitrate_bps,
                                              max_padding_bitrate_bps);
}

int64_t CongestionController::GetPacerQueuingDelayMs() const {
  return send_side_cc_.GetPacerQueuingDelayMs();
}

void CongestionController::SignalNetworkState(NetworkState state) {
  send_side_cc_.SignalNetworkState(state);
}

void CongestionController::SetTransportOverhead(
    size_t transport_overhead_bytes_per_packet) {
  send_side_cc_.SetTransportOverhead(transport_overhead_bytes_per_packet);
}

void CongestionController::OnSentPacket(const rtc::SentPacket& sent_packet) {
  send_side_cc_.OnSentPacket(sent_packet);
}

void CongestionController::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  receive_side_cc_.OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  send_side_cc_.OnRttUpdate(avg_rtt_ms, max_rtt_ms);
}

int64_t CongestionController::TimeUntilNextProcess() {
  return std::min(send_side_cc_.TimeUntilNextProcess(),
                  receive_side_cc_.TimeUntilNextProcess());
}

void CongestionController::Process() {
  send_side_cc_.Process();
  receive_side_cc_.Process();
}

void CongestionController::AddPacket(uint32_t ssrc,
                                     uint16_t sequence_number,
                                     size_t length,
                                     const PacedPacketInfo& pacing_info) {
  send_side_cc_.AddPacket(ssrc, sequence_number, length, pacing_info);
}

void CongestionController::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  send_side_cc_.OnTransportFeedback(feedback);
}

std::vector<PacketFeedback> CongestionController::GetTransportFeedbackVector()
    const {
  return send_side_cc_.GetTransportFeedbackVector();
}

}  // namespace webrtc
