/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/probe_bitrate_estimator.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace {
// The minumum number of probes we need to receive feedback about in percent
// in order to have a valid estimate.
constexpr int kMinReceivedProbesPercent = 80;

// The minumum number of bytes we need to receive feedback about in percent
// in order to have a valid estimate.
constexpr int kMinReceivedBytesPercent = 80;

// The maximum |receive rate| / |send rate| ratio for a valid estimate.
constexpr float kMaxValidRatio = 2.0f;

// The minimum |receive rate| / |send rate| ratio assuming that the link is
// not saturated, i.e. we assume that we will receive at least
// kMinRatioForUnsaturatedLink * |send rate| if |send rate| is less than the
// link capacity.
constexpr float kMinRatioForUnsaturatedLink = 0.9f;

// The target utilization of the link. If we know true link capacity
// we'd like to send at 95% of that rate.
constexpr float kTargetUtilizationFraction = 0.95f;

// The maximum time period over which the cluster history is retained.
// This is also the maximum time period beyond which a probing burst is not
// expected to last.
constexpr int kMaxClusterHistoryMs = 1000;

// The maximum time interval between first and the last probe on a cluster
// on the sender side as well as the receive side.
constexpr int kMaxProbeIntervalMs = 1000;
}  // namespace

namespace webrtc {

ProbeBitrateEstimator::ProbeBitrateEstimator(RtcEventLog* event_log)
    : event_log_(event_log) {}

ProbeBitrateEstimator::~ProbeBitrateEstimator() = default;

int ProbeBitrateEstimator::HandleProbeAndEstimateBitrate(
    const PacketFeedback& packet_feedback) {
  int cluster_id = packet_feedback.pacing_info.probe_cluster_id;
  RTC_DCHECK_NE(cluster_id, PacedPacketInfo::kNotAProbe);

  EraseOldClusters(packet_feedback.arrival_time_ms - kMaxClusterHistoryMs);

  int payload_size_bits =
      rtc::dchecked_cast<int>(packet_feedback.payload_size * 8);
  AggregatedCluster* cluster = &clusters_[cluster_id];

  if (packet_feedback.send_time_ms < cluster->first_send_ms) {
    cluster->first_send_ms = packet_feedback.send_time_ms;
  }
  if (packet_feedback.send_time_ms > cluster->last_send_ms) {
    cluster->last_send_ms = packet_feedback.send_time_ms;
    cluster->size_last_send = payload_size_bits;
  }
  if (packet_feedback.arrival_time_ms < cluster->first_receive_ms) {
    cluster->first_receive_ms = packet_feedback.arrival_time_ms;
    cluster->size_first_receive = payload_size_bits;
  }
  if (packet_feedback.arrival_time_ms > cluster->last_receive_ms) {
    cluster->last_receive_ms = packet_feedback.arrival_time_ms;
  }
  cluster->size_total += payload_size_bits;
  cluster->num_probes += 1;

  RTC_DCHECK_GT(packet_feedback.pacing_info.probe_cluster_min_probes, 0);
  RTC_DCHECK_GT(packet_feedback.pacing_info.probe_cluster_min_bytes, 0);

  int min_probes = packet_feedback.pacing_info.probe_cluster_min_probes *
                   kMinReceivedProbesPercent / 100;
  int min_bytes = packet_feedback.pacing_info.probe_cluster_min_bytes *
                  kMinReceivedBytesPercent / 100;
  if (cluster->num_probes < min_probes || cluster->size_total < min_bytes * 8)
    return -1;

  float send_interval_ms = cluster->last_send_ms - cluster->first_send_ms;
  float receive_interval_ms =
      cluster->last_receive_ms - cluster->first_receive_ms;

  if (send_interval_ms <= 0 || send_interval_ms > kMaxProbeIntervalMs ||
      receive_interval_ms <= 0 || receive_interval_ms > kMaxProbeIntervalMs) {
    RTC_LOG(LS_INFO) << "Probing unsuccessful, invalid send/receive interval"
                     << " [cluster id: " << cluster_id
                     << "] [send interval: " << send_interval_ms << " ms]"
                     << " [receive interval: " << receive_interval_ms << " ms]";
    if (event_log_) {
      event_log_->Log(absl::make_unique<RtcEventProbeResultFailure>(
          cluster_id, ProbeFailureReason::kInvalidSendReceiveInterval));
    }
    return -1;
  }
  // Since the |send_interval_ms| does not include the time it takes to actually
  // send the last packet the size of the last sent packet should not be
  // included when calculating the send bitrate.
  RTC_DCHECK_GT(cluster->size_total, cluster->size_last_send);
  float send_size = cluster->size_total - cluster->size_last_send;
  float send_bps = send_size / send_interval_ms * 1000;

  // Since the |receive_interval_ms| does not include the time it takes to
  // actually receive the first packet the size of the first received packet
  // should not be included when calculating the receive bitrate.
  RTC_DCHECK_GT(cluster->size_total, cluster->size_first_receive);
  float receive_size = cluster->size_total - cluster->size_first_receive;
  float receive_bps = receive_size / receive_interval_ms * 1000;

  float ratio = receive_bps / send_bps;
  if (ratio > kMaxValidRatio) {
    RTC_LOG(LS_INFO) << "Probing unsuccessful, receive/send ratio too high"
                     << " [cluster id: " << cluster_id
                     << "] [send: " << send_size << " bytes / "
                     << send_interval_ms << " ms = " << send_bps / 1000
                     << " kb/s]"
                     << " [receive: " << receive_size << " bytes / "
                     << receive_interval_ms << " ms = " << receive_bps / 1000
                     << " kb/s]"
                     << " [ratio: " << receive_bps / 1000 << " / "
                     << send_bps / 1000 << " = " << ratio
                     << " > kMaxValidRatio (" << kMaxValidRatio << ")]";
    if (event_log_) {
      event_log_->Log(absl::make_unique<RtcEventProbeResultFailure>(
          cluster_id, ProbeFailureReason::kInvalidSendReceiveRatio));
    }
    return -1;
  }
  RTC_LOG(LS_INFO) << "Probing successful"
                   << " [cluster id: " << cluster_id << "] [send: " << send_size
                   << " bytes / " << send_interval_ms
                   << " ms = " << send_bps / 1000 << " kb/s]"
                   << " [receive: " << receive_size << " bytes / "
                   << receive_interval_ms << " ms = " << receive_bps / 1000
                   << " kb/s]";

  float res = std::min(send_bps, receive_bps);
  // If we're receiving at significantly lower bitrate than we were sending at,
  // it suggests that we've found the true capacity of the link. In this case,
  // set the target bitrate slightly lower to not immediately overuse.
  if (receive_bps < kMinRatioForUnsaturatedLink * send_bps) {
    RTC_DCHECK_GT(send_bps, receive_bps);
    res = kTargetUtilizationFraction * receive_bps;
  }
  if (event_log_) {
    event_log_->Log(
        absl::make_unique<RtcEventProbeResultSuccess>(cluster_id, res));
  }
  estimated_bitrate_bps_ = res;
  return *estimated_bitrate_bps_;
}

absl::optional<DataRate>
ProbeBitrateEstimator::FetchAndResetLastEstimatedBitrate() {
  absl::optional<int> estimated_bitrate_bps = estimated_bitrate_bps_;
  estimated_bitrate_bps_.reset();
  if (estimated_bitrate_bps)
    return DataRate::bps(*estimated_bitrate_bps);
  return absl::nullopt;
}

void ProbeBitrateEstimator::EraseOldClusters(int64_t timestamp_ms) {
  for (auto it = clusters_.begin(); it != clusters_.end();) {
    if (it->second.last_receive_ms < timestamp_ms) {
      it = clusters_.erase(it);
    } else {
      ++it;
    }
  }
}
}  // namespace webrtc
