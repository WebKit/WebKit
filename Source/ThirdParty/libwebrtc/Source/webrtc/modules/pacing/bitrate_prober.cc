/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/bitrate_prober.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// A minimum interval between probes to allow scheduling to be feasible.
constexpr int kMinProbeDeltaMs = 1;

// The minimum number probing packets used.
constexpr int kMinProbePacketsSent = 5;

// The minimum probing duration in ms.
constexpr int kMinProbeDurationMs = 15;

// Maximum amount of time each probe can be delayed. Probe cluster is reset and
// retried from the start when this limit is reached.
constexpr int kMaxProbeDelayMs = 3;

// The min probe packet size is scaled with the bitrate we're probing at.
// This defines the max min probe packet size, meaning that on high bitrates
// we have a min probe packet size of 200 bytes.
constexpr size_t kMinProbePacketSize = 200;

constexpr int64_t kProbeClusterTimeoutMs = 5000;

}  // namespace

BitrateProber::BitrateProber() : BitrateProber(nullptr) {}

BitrateProber::~BitrateProber() = default;

BitrateProber::BitrateProber(RtcEventLog* event_log)
    : probing_state_(ProbingState::kDisabled),
      next_probe_time_ms_(-1),
      next_cluster_id_(0),
      event_log_(event_log) {
  SetEnabled(true);
}

void BitrateProber::SetEnabled(bool enable) {
  if (enable) {
    if (probing_state_ == ProbingState::kDisabled) {
      probing_state_ = ProbingState::kInactive;
      RTC_LOG(LS_INFO) << "Bandwidth probing enabled, set to inactive";
    }
  } else {
    probing_state_ = ProbingState::kDisabled;
    RTC_LOG(LS_INFO) << "Bandwidth probing disabled";
  }
}

bool BitrateProber::IsProbing() const {
  return probing_state_ == ProbingState::kActive;
}

void BitrateProber::OnIncomingPacket(size_t packet_size) {
  // Don't initialize probing unless we have something large enough to start
  // probing.
  if (probing_state_ == ProbingState::kInactive && !clusters_.empty() &&
      packet_size >=
          std::min<size_t>(RecommendedMinProbeSize(), kMinProbePacketSize)) {
    // Send next probe right away.
    next_probe_time_ms_ = -1;
    probing_state_ = ProbingState::kActive;
  }
}

void BitrateProber::CreateProbeCluster(int bitrate_bps, int64_t now_ms) {
  RTC_DCHECK(probing_state_ != ProbingState::kDisabled);
  RTC_DCHECK_GT(bitrate_bps, 0);
  while (!clusters_.empty() &&
         now_ms - clusters_.front().time_created_ms > kProbeClusterTimeoutMs) {
    clusters_.pop();
  }

  ProbeCluster cluster;
  cluster.time_created_ms = now_ms;
  cluster.pace_info.probe_cluster_min_probes = kMinProbePacketsSent;
  cluster.pace_info.probe_cluster_min_bytes = static_cast<int32_t>(
      static_cast<int64_t>(bitrate_bps) * kMinProbeDurationMs / 8000);
  RTC_DCHECK_GE(cluster.pace_info.probe_cluster_min_bytes, 0);
  cluster.pace_info.send_bitrate_bps = bitrate_bps;
  cluster.pace_info.probe_cluster_id = next_cluster_id_++;
  clusters_.push(cluster);
  if (event_log_)
    event_log_->Log(absl::make_unique<RtcEventProbeClusterCreated>(
        cluster.pace_info.probe_cluster_id, cluster.pace_info.send_bitrate_bps,
        cluster.pace_info.probe_cluster_min_probes,
        cluster.pace_info.probe_cluster_min_bytes));

  RTC_LOG(LS_INFO) << "Probe cluster (bitrate:min bytes:min packets): ("
                   << cluster.pace_info.send_bitrate_bps << ":"
                   << cluster.pace_info.probe_cluster_min_bytes << ":"
                   << cluster.pace_info.probe_cluster_min_probes << ")";
  // If we are already probing, continue to do so. Otherwise set it to
  // kInactive and wait for OnIncomingPacket to start the probing.
  if (probing_state_ != ProbingState::kActive)
    probing_state_ = ProbingState::kInactive;
}

int BitrateProber::TimeUntilNextProbe(int64_t now_ms) {
  // Probing is not active or probing is already complete.
  if (probing_state_ != ProbingState::kActive || clusters_.empty())
    return -1;

  int time_until_probe_ms = 0;
  if (next_probe_time_ms_ >= 0) {
    time_until_probe_ms = next_probe_time_ms_ - now_ms;
    if (time_until_probe_ms < -kMaxProbeDelayMs) {
      RTC_DLOG(LS_WARNING) << "Probe delay too high"
                           << " (next_ms:" << next_probe_time_ms_
                           << ", now_ms: " << now_ms << ")";
      return -1;
    }
  }

  return std::max(time_until_probe_ms, 0);
}

PacedPacketInfo BitrateProber::CurrentCluster() const {
  RTC_DCHECK(!clusters_.empty());
  RTC_DCHECK(probing_state_ == ProbingState::kActive);
  return clusters_.front().pace_info;
}

// Probe size is recommended based on the probe bitrate required. We choose
// a minimum of twice |kMinProbeDeltaMs| interval to allow scheduling to be
// feasible.
size_t BitrateProber::RecommendedMinProbeSize() const {
  RTC_DCHECK(!clusters_.empty());
  return clusters_.front().pace_info.send_bitrate_bps * 2 * kMinProbeDeltaMs /
         (8 * 1000);
}

void BitrateProber::ProbeSent(int64_t now_ms, size_t bytes) {
  RTC_DCHECK(probing_state_ == ProbingState::kActive);
  RTC_DCHECK_GT(bytes, 0);

  if (!clusters_.empty()) {
    ProbeCluster* cluster = &clusters_.front();
    if (cluster->sent_probes == 0) {
      RTC_DCHECK_EQ(cluster->time_started_ms, -1);
      cluster->time_started_ms = now_ms;
    }
    cluster->sent_bytes += static_cast<int>(bytes);
    cluster->sent_probes += 1;
    next_probe_time_ms_ = GetNextProbeTime(*cluster);
    if (cluster->sent_bytes >= cluster->pace_info.probe_cluster_min_bytes &&
        cluster->sent_probes >= cluster->pace_info.probe_cluster_min_probes) {
      clusters_.pop();
    }
    if (clusters_.empty())
      probing_state_ = ProbingState::kSuspended;
  }
}

int64_t BitrateProber::GetNextProbeTime(const ProbeCluster& cluster) {
  RTC_CHECK_GT(cluster.pace_info.send_bitrate_bps, 0);
  RTC_CHECK_GE(cluster.time_started_ms, 0);

  // Compute the time delta from the cluster start to ensure probe bitrate stays
  // close to the target bitrate. Result is in milliseconds.
  int64_t delta_ms =
      (8000ll * cluster.sent_bytes + cluster.pace_info.send_bitrate_bps / 2) /
      cluster.pace_info.send_bitrate_bps;
  return cluster.time_started_ms + delta_ms;
}

}  // namespace webrtc
