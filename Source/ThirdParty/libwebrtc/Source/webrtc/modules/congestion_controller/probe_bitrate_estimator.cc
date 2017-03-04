/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace {
// The minumum number of probes we need for a valid cluster.
constexpr int kMinNumProbesValidCluster = 4;

// The maximum (receive rate)/(send rate) ratio for a valid estimate.
constexpr float kValidRatio = 2.0f;

// The maximum time period over which the cluster history is retained.
// This is also the maximum time period beyond which a probing burst is not
// expected to last.
constexpr int kMaxClusterHistoryMs = 1000;

// The maximum time interval between first and the last probe on a cluster
// on the sender side as well as the receive side.
constexpr int kMaxProbeIntervalMs = 1000;
}  // namespace

namespace webrtc {

ProbeBitrateEstimator::ProbeBitrateEstimator() {}

int ProbeBitrateEstimator::HandleProbeAndEstimateBitrate(
    const PacketInfo& packet_info) {
  int cluster_id = packet_info.pacing_info.probe_cluster_id;
  RTC_DCHECK_NE(cluster_id, PacedPacketInfo::kNotAProbe);

  EraseOldClusters(packet_info.arrival_time_ms - kMaxClusterHistoryMs);

  int payload_size_bits = packet_info.payload_size * 8;
  AggregatedCluster* cluster = &clusters_[cluster_id];

  if (packet_info.send_time_ms < cluster->first_send_ms) {
    cluster->first_send_ms = packet_info.send_time_ms;
  }
  if (packet_info.send_time_ms > cluster->last_send_ms) {
    cluster->last_send_ms = packet_info.send_time_ms;
    cluster->size_last_send = payload_size_bits;
  }
  if (packet_info.arrival_time_ms < cluster->first_receive_ms) {
    cluster->first_receive_ms = packet_info.arrival_time_ms;
    cluster->size_first_receive = payload_size_bits;
  }
  if (packet_info.arrival_time_ms > cluster->last_receive_ms) {
    cluster->last_receive_ms = packet_info.arrival_time_ms;
  }
  cluster->size_total += payload_size_bits;
  cluster->num_probes += 1;

  if (cluster->num_probes < kMinNumProbesValidCluster)
    return -1;

  float send_interval_ms = cluster->last_send_ms - cluster->first_send_ms;
  float receive_interval_ms =
      cluster->last_receive_ms - cluster->first_receive_ms;

  if (send_interval_ms <= 0 || send_interval_ms > kMaxProbeIntervalMs ||
      receive_interval_ms <= 0 || receive_interval_ms > kMaxProbeIntervalMs) {
    LOG(LS_INFO) << "Probing unsuccessful, invalid send/receive interval"
                 << " [cluster id: " << cluster_id
                 << "] [send interval: " << send_interval_ms << " ms]"
                 << " [receive interval: " << receive_interval_ms << " ms]";
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
  if (ratio > kValidRatio) {
    LOG(LS_INFO) << "Probing unsuccessful, receive/send ratio too high"
                 << " [cluster id: " << cluster_id << "] [send: " << send_size
                 << " bytes / " << send_interval_ms
                 << " ms = " << send_bps / 1000 << " kb/s]"
                 << " [receive: " << receive_size << " bytes / "
                 << receive_interval_ms << " ms = " << receive_bps / 1000
                 << " kb/s]"
                 << " [ratio: " << receive_bps / 1000 << " / "
                 << send_bps / 1000 << " = " << ratio << " > kValidRatio ("
                 << kValidRatio << ")]";
    return -1;
  }
  LOG(LS_INFO) << "Probing successful"
               << " [cluster id: " << cluster_id << "] [send: " << send_size
               << " bytes / " << send_interval_ms << " ms = " << send_bps / 1000
               << " kb/s]"
               << " [receive: " << receive_size << " bytes / "
               << receive_interval_ms << " ms = " << receive_bps / 1000
               << " kb/s]";
  return std::min(send_bps, receive_bps);
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
