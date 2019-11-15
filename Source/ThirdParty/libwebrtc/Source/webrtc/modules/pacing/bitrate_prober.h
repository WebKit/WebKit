/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_BITRATE_PROBER_H_
#define MODULES_PACING_BITRATE_PROBER_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {
class RtcEventLog;

struct BitrateProberConfig {
  explicit BitrateProberConfig(const WebRtcKeyValueConfig* key_value_config);
  BitrateProberConfig(const BitrateProberConfig&) = default;
  BitrateProberConfig& operator=(const BitrateProberConfig&) = default;
  ~BitrateProberConfig() = default;

  // The minimum number probing packets used.
  FieldTrialParameter<int> min_probe_packets_sent;
  // A minimum interval between probes to allow scheduling to be feasible.
  FieldTrialParameter<TimeDelta> min_probe_delta;
  // The minimum probing duration.
  FieldTrialParameter<TimeDelta> min_probe_duration;
  // Maximum amount of time each probe can be delayed. Probe cluster is reset
  // and retried from the start when this limit is reached.
  FieldTrialParameter<TimeDelta> max_probe_delay;
};

// Note that this class isn't thread-safe by itself and therefore relies
// on being protected by the caller.
class BitrateProber {
 public:
  explicit BitrateProber(const WebRtcKeyValueConfig& field_trials);
  ~BitrateProber();

  void SetEnabled(bool enable);

  // Returns true if the prober is in a probing session, i.e., it currently
  // wants packets to be sent out according to the time returned by
  // TimeUntilNextProbe().
  bool IsProbing() const;

  // Initializes a new probing session if the prober is allowed to probe. Does
  // not initialize the prober unless the packet size is large enough to probe
  // with.
  void OnIncomingPacket(size_t packet_size);

  // Create a cluster used to probe for |bitrate_bps| with |num_probes| number
  // of probes.
  void CreateProbeCluster(int bitrate_bps, int64_t now_ms, int cluster_id);

  // Returns the number of milliseconds until the next probe should be sent to
  // get accurate probing.
  int TimeUntilNextProbe(int64_t now_ms);

  // Information about the current probing cluster.
  PacedPacketInfo CurrentCluster() const;

  // Returns the minimum number of bytes that the prober recommends for
  // the next probe.
  size_t RecommendedMinProbeSize() const;

  // Called to report to the prober that a probe has been sent. In case of
  // multiple packets per probe, this call would be made at the end of sending
  // the last packet in probe. |probe_size| is the total size of all packets
  // in probe.
  void ProbeSent(int64_t now_ms, size_t probe_size);

 private:
  enum class ProbingState {
    // Probing will not be triggered in this state at all times.
    kDisabled,
    // Probing is enabled and ready to trigger on the first packet arrival.
    kInactive,
    // Probe cluster is filled with the set of data rates to be probed and
    // probes are being sent.
    kActive,
    // Probing is enabled, but currently suspended until an explicit trigger
    // to start probing again.
    kSuspended,
  };

  // A probe cluster consists of a set of probes. Each probe in turn can be
  // divided into a number of packets to accommodate the MTU on the network.
  struct ProbeCluster {
    PacedPacketInfo pace_info;

    int sent_probes = 0;
    int sent_bytes = 0;
    int64_t time_created_ms = -1;
    int64_t time_started_ms = -1;
    int retries = 0;
  };

  int64_t GetNextProbeTime(const ProbeCluster& cluster);

  ProbingState probing_state_;

  // Probe bitrate per packet. These are used to compute the delta relative to
  // the previous probe packet based on the size and time when that packet was
  // sent.
  std::queue<ProbeCluster> clusters_;

  // Time the next probe should be sent when in kActive state.
  int64_t next_probe_time_ms_;

  int total_probe_count_;
  int total_failed_probe_count_;

  BitrateProberConfig config_;
};

}  // namespace webrtc

#endif  // MODULES_PACING_BITRATE_PROBER_H_
