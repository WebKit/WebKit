/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// BBR (Bottleneck Bandwidth and RTT) congestion control algorithm.
// Based on the Quic BBR implementation in Chromium.

#ifndef MODULES_CONGESTION_CONTROLLER_BBR_BBR_NETWORK_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_BBR_NETWORK_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "modules/congestion_controller/bbr/bandwidth_sampler.h"
#include "modules/congestion_controller/bbr/loss_rate_filter.h"
#include "modules/congestion_controller/bbr/rtt_stats.h"
#include "modules/congestion_controller/bbr/windowed_filter.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace bbr {

typedef int64_t BbrRoundTripCount;

// BbrSender implements BBR congestion control algorithm.  BBR aims to estimate
// the current available Bottleneck Bandwidth and RTT (hence the name), and
// regulates the pacing rate and the size of the congestion window based on
// those signals.
//
// BBR relies on pacing in order to function properly.  Do not use BBR when
// pacing is disabled.
class BbrNetworkController : public NetworkControllerInterface {
 public:
  enum Mode {
    // Startup phase of the connection.
    STARTUP,
    // After achieving the highest possible bandwidth during the startup, lower
    // the pacing rate in order to drain the queue.
    DRAIN,
    // Cruising mode.
    PROBE_BW,
    // Temporarily slow down sending in order to empty the buffer and measure
    // the real minimum RTT.
    PROBE_RTT,
  };

  // Indicates how the congestion control limits the amount of bytes in flight.
  enum RecoveryState {
    // Do not limit.
    NOT_IN_RECOVERY = 0,
    // Allow an extra outstanding byte for each byte acknowledged.
    CONSERVATION = 1,
    // Allow 1.5 extra outstanding bytes for each byte acknowledged.
    MEDIUM_GROWTH = 2,
    // Allow two extra outstanding bytes for each byte acknowledged (slow
    // start).
    GROWTH = 3
  };
  struct BbrControllerConfig {
    FieldTrialParameter<double> probe_bw_pacing_gain_offset;
    FieldTrialParameter<double> encoder_rate_gain;
    FieldTrialParameter<double> encoder_rate_gain_in_probe_rtt;
    // RTT delta to determine if startup should be exited due to increased RTT.
    FieldTrialParameter<TimeDelta> exit_startup_rtt_threshold;

    FieldTrialParameter<DataSize> initial_congestion_window;
    FieldTrialParameter<DataSize> min_congestion_window;
    FieldTrialParameter<DataSize> max_congestion_window;

    FieldTrialParameter<double> probe_rtt_congestion_window_gain;
    FieldTrialParameter<bool> pacing_rate_as_target;

    // Configurable in QUIC BBR:
    FieldTrialParameter<bool> exit_startup_on_loss;
    // The number of RTTs to stay in STARTUP mode.  Defaults to 3.
    FieldTrialParameter<int> num_startup_rtts;
    // When true, recovery is rate based rather than congestion window based.
    FieldTrialParameter<bool> rate_based_recovery;
    FieldTrialParameter<double> max_aggregation_bytes_multiplier;
    // When true, pace at 1.5x and disable packet conservation in STARTUP.
    FieldTrialParameter<bool> slower_startup;
    // When true, disables packet conservation in STARTUP.
    FieldTrialParameter<bool> rate_based_startup;
    // Used as the initial packet conservation mode when first entering
    // recovery.
    FieldTrialEnum<RecoveryState> initial_conservation_in_startup;
    // If true, will not exit low gain mode until bytes_in_flight drops below
    // BDP or it's time for high gain mode.
    FieldTrialParameter<bool> fully_drain_queue;

    FieldTrialParameter<double> max_ack_height_window_multiplier;
    // If true, use a CWND of 0.75*BDP during probe_rtt instead of 4 packets.
    FieldTrialParameter<bool> probe_rtt_based_on_bdp;
    // If true, skip probe_rtt and update the timestamp of the existing min_rtt
    // to now if min_rtt over the last cycle is within 12.5% of the current
    // min_rtt. Even if the min_rtt is 12.5% too low, the 25% gain cycling and
    // 2x CWND gain should overcome an overly small min_rtt.
    FieldTrialParameter<bool> probe_rtt_skipped_if_similar_rtt;
    // If true, disable PROBE_RTT entirely as long as the connection was
    // recently app limited.
    FieldTrialParameter<bool> probe_rtt_disabled_if_app_limited;

    explicit BbrControllerConfig(std::string field_trial);
    ~BbrControllerConfig();
    BbrControllerConfig(const BbrControllerConfig&);
    static BbrControllerConfig FromTrial();
  };

  // Debug state can be exported in order to troubleshoot potential congestion
  // control issues.
  struct DebugState {
    explicit DebugState(const BbrNetworkController& sender);
    DebugState(const DebugState& state);

    Mode mode;
    DataRate max_bandwidth;
    BbrRoundTripCount round_trip_count;
    int gain_cycle_index;
    DataSize congestion_window;

    bool is_at_full_bandwidth;
    DataRate bandwidth_at_last_round;
    BbrRoundTripCount rounds_without_bandwidth_gain;

    TimeDelta min_rtt;
    Timestamp min_rtt_timestamp;

    RecoveryState recovery_state;
    DataSize recovery_window;

    bool last_sample_is_app_limited;
    int64_t end_of_app_limited_phase;
  };

  explicit BbrNetworkController(NetworkControllerConfig config);
  ~BbrNetworkController() override;

  // NetworkControllerInterface
  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override;
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override;
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override;
  NetworkControlUpdate OnSentPacket(SentPacket msg) override;
  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override;
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override;
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override;

  // Part of remote bitrate estimation api, not implemented for BBR
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override;
  NetworkControlUpdate OnReceivedPacket(ReceivedPacket msg) override;
  NetworkControlUpdate OnNetworkStateEstimate(
      NetworkStateEstimate msg) override;

  NetworkControlUpdate CreateRateUpdate(Timestamp at_time) const;

 private:
  void Reset();
  bool InSlowStart() const;
  bool InRecovery() const;
  bool IsProbingForMoreBandwidth() const;

  bool CanSend(DataSize bytes_in_flight);
  DataRate PacingRate() const;
  DataRate BandwidthEstimate() const;
  DataSize GetCongestionWindow() const;

  double GetPacingGain(int round_offset) const;

  void OnApplicationLimited(DataSize bytes_in_flight);
  // End implementation of SendAlgorithmInterface.

  typedef WindowedFilter<DataRate,
                         MaxFilter<DataRate>,
                         BbrRoundTripCount,
                         BbrRoundTripCount>
      MaxBandwidthFilter;

  typedef WindowedFilter<TimeDelta,
                         MaxFilter<TimeDelta>,
                         BbrRoundTripCount,
                         BbrRoundTripCount>
      MaxAckDelayFilter;

  typedef WindowedFilter<DataSize,
                         MaxFilter<DataSize>,
                         BbrRoundTripCount,
                         BbrRoundTripCount>
      MaxAckHeightFilter;

  // Returns the current estimate of the RTT of the connection.  Outside of the
  // edge cases, this is minimum RTT.
  TimeDelta GetMinRtt() const;
  // Returns whether the connection has achieved full bandwidth required to exit
  // the slow start.
  bool IsAtFullBandwidth() const;
  // Computes the target congestion window using the specified gain.
  DataSize GetTargetCongestionWindow(double gain) const;
  // The target congestion window during PROBE_RTT.
  DataSize ProbeRttCongestionWindow() const;
  // Returns true if the current min_rtt should be kept and we should not enter
  // PROBE_RTT immediately.
  bool ShouldExtendMinRttExpiry() const;

  // Enters the STARTUP mode.
  void EnterStartupMode();
  // Enters the PROBE_BW mode.
  void EnterProbeBandwidthMode(Timestamp now);

  // Discards the lost packets from BandwidthSampler state.
  void DiscardLostPackets(const std::vector<PacketResult>& lost_packets);
  // Updates the round-trip counter if a round-trip has passed.  Returns true if
  // the counter has been advanced.
  // |last_acked_packet| is the sequence number of the last acked packet.
  bool UpdateRoundTripCounter(int64_t last_acked_packet);
  // Updates the current bandwidth and min_rtt estimate based on the samples for
  // the received acknowledgements.  Returns true if min_rtt has expired.
  bool UpdateBandwidthAndMinRtt(Timestamp now,
                                const std::vector<PacketResult>& acked_packets);
  // Updates the current gain used in PROBE_BW mode.
  void UpdateGainCyclePhase(Timestamp now,
                            DataSize prior_in_flight,
                            bool has_losses);
  // Tracks for how many round-trips the bandwidth has not increased
  // significantly.
  void CheckIfFullBandwidthReached();
  // Transitions from STARTUP to DRAIN and from DRAIN to PROBE_BW if
  // appropriate.
  void MaybeExitStartupOrDrain(const TransportPacketsFeedback&);
  // Decides whether to enter or exit PROBE_RTT.
  void MaybeEnterOrExitProbeRtt(const TransportPacketsFeedback& msg,
                                bool is_round_start,
                                bool min_rtt_expired);
  // Determines whether BBR needs to enter, exit or advance state of the
  // recovery.
  void UpdateRecoveryState(int64_t last_acked_packet,
                           bool has_losses,
                           bool is_round_start);

  // Updates the ack aggregation max filter in bytes.
  void UpdateAckAggregationBytes(Timestamp ack_time,
                                 DataSize newly_acked_bytes);

  // Determines the appropriate pacing rate for the connection.
  void CalculatePacingRate();
  // Determines the appropriate congestion window for the connection.
  void CalculateCongestionWindow(DataSize bytes_acked);
  // Determines the approriate window that constrains the
  // in-flight during recovery.
  void CalculateRecoveryWindow(DataSize bytes_acked,
                               DataSize bytes_lost,
                               DataSize bytes_in_flight);

  BbrControllerConfig config_;

  RttStats rtt_stats_;
  webrtc::Random random_;
  LossRateFilter loss_rate_;

  absl::optional<TargetRateConstraints> constraints_;

  Mode mode_;

  // Bandwidth sampler provides BBR with the bandwidth measurements at
  // individual points.
  std::unique_ptr<BandwidthSampler> sampler_;

  // The number of the round trips that have occurred during the connection.
  BbrRoundTripCount round_trip_count_ = 0;

  // The packet number of the most recently sent packet.
  int64_t last_sent_packet_;
  // Acknowledgement of any packet after |current_round_trip_end_| will cause
  // the round trip counter to advance.
  int64_t current_round_trip_end_;

  // The filter that tracks the maximum bandwidth over the multiple recent
  // round-trips.
  MaxBandwidthFilter max_bandwidth_;

  DataRate default_bandwidth_;

  // Tracks the maximum number of bytes acked faster than the sending rate.
  MaxAckHeightFilter max_ack_height_;

  // The time this aggregation started and the number of bytes acked during it.
  absl::optional<Timestamp> aggregation_epoch_start_time_;
  DataSize aggregation_epoch_bytes_;

  // The number of bytes acknowledged since the last time bytes in flight
  // dropped below the target window.
  DataSize bytes_acked_since_queue_drained_;

  // The muliplier for calculating the max amount of extra CWND to add to
  // compensate for ack aggregation.
  double max_aggregation_bytes_multiplier_;

  // Minimum RTT estimate.  Automatically expires within 10 seconds (and
  // triggers PROBE_RTT mode) if no new value is sampled during that period.
  TimeDelta min_rtt_;
  TimeDelta last_rtt_;
  // The time at which the current value of |min_rtt_| was assigned.
  Timestamp min_rtt_timestamp_;

  // The maximum allowed number of bytes in flight.
  DataSize congestion_window_;

  // The initial value of the |congestion_window_|.
  DataSize initial_congestion_window_;

  // The smallest value the |congestion_window_| can achieve.
  DataSize min_congestion_window_;

  // The largest value the |congestion_window_| can achieve.
  DataSize max_congestion_window_;

  // The current pacing rate of the connection.
  DataRate pacing_rate_;

  // The gain currently applied to the pacing rate.
  double pacing_gain_;
  // The gain currently applied to the congestion window.
  double congestion_window_gain_;

  // The gain used for the congestion window during PROBE_BW.  Latched from
  // quic_bbr_cwnd_gain flag.
  const double congestion_window_gain_constant_;
  // The coefficient by which mean RTT variance is added to the congestion
  // window.  Latched from quic_bbr_rtt_variation_weight flag.
  const double rtt_variance_weight_;

  // Number of round-trips in PROBE_BW mode, used for determining the current
  // pacing gain cycle.
  int cycle_current_offset_;
  // The time at which the last pacing gain cycle was started.
  Timestamp last_cycle_start_;

  // Indicates whether the connection has reached the full bandwidth mode.
  bool is_at_full_bandwidth_;
  // Number of rounds during which there was no significant bandwidth increase.
  BbrRoundTripCount rounds_without_bandwidth_gain_;
  // The bandwidth compared to which the increase is measured.
  DataRate bandwidth_at_last_round_;

  // Set to true upon exiting quiescence.
  bool exiting_quiescence_;

  // Time at which PROBE_RTT has to be exited.  Setting it to zero indicates
  // that the time is yet unknown as the number of packets in flight has not
  // reached the required value.
  absl::optional<Timestamp> exit_probe_rtt_at_;
  // Indicates whether a round-trip has passed since PROBE_RTT became active.
  bool probe_rtt_round_passed_;

  // Indicates whether the most recent bandwidth sample was marked as
  // app-limited.
  bool last_sample_is_app_limited_;

  // Current state of recovery.
  RecoveryState recovery_state_;
  // Receiving acknowledgement of a packet after |end_recovery_at_| will cause
  // BBR to exit the recovery mode.  A set value indicates at least one
  // loss has been detected, so it must not be reset.
  absl::optional<int64_t> end_recovery_at_;
  // A window used to limit the number of bytes in flight during loss recovery.
  DataSize recovery_window_;

  bool app_limited_since_last_probe_rtt_;
  TimeDelta min_rtt_since_last_probe_rtt_;

  RTC_DISALLOW_COPY_AND_ASSIGN(BbrNetworkController);
};

// Used in log output
std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const BbrNetworkController::Mode& mode);

}  // namespace bbr
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_BBR_NETWORK_CONTROLLER_H_
