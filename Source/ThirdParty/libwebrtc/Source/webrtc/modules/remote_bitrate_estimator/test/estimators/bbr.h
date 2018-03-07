/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_BBR_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_BBR_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "api/optional.h"
#include "modules/remote_bitrate_estimator/test/bwe.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace testing {
namespace bwe {
class MaxBandwidthFilter;
class MinRttFilter;
class CongestionWindow;
class BbrBweSender : public BweSender {
 public:
  explicit BbrBweSender(BitrateObserver* observer, Clock* clock);
  virtual ~BbrBweSender();
  enum Mode {
    // Startup phase.
    STARTUP,
    // Queue draining phase, which where created during startup.
    DRAIN,
    // Cruising, probing new bandwidth.
    PROBE_BW,
    // Temporarily limiting congestion window size in order to measure
    // minimum RTT.
    PROBE_RTT,
    // Temporarily reducing pacing rate and congestion window, in order to
    // ensure no queues are built.
    RECOVERY
  };

  struct PacketStats {
    PacketStats() {}
    PacketStats(uint16_t sequence_number_,
                int64_t last_sent_packet_send_time_ms_,
                int64_t send_time_ms_,
                int64_t ack_time_ms_,
                int64_t last_acked_packet_ack_time_ms_,
                size_t payload_size_bytes_,
                size_t data_sent_bytes_,
                size_t data_sent_before_last_sent_packet_bytes_,
                size_t data_acked_bytes_,
                size_t data_acked_before_last_acked_packet_bytes_)
        : sequence_number(sequence_number_),
          last_sent_packet_send_time_ms(last_sent_packet_send_time_ms_),
          send_time_ms(send_time_ms_),
          ack_time_ms(ack_time_ms_),
          last_acked_packet_ack_time_ms(last_acked_packet_ack_time_ms_),
          payload_size_bytes(payload_size_bytes_),
          data_sent_bytes(data_sent_bytes_),
          data_sent_before_last_sent_packet_bytes(
              data_sent_before_last_sent_packet_bytes_),
          data_acked_bytes(data_acked_bytes_),
          data_acked_before_last_acked_packet_bytes(
              data_acked_before_last_acked_packet_bytes_) {}
    // Sequence number of this packet.
    uint16_t sequence_number;
    // Send time of the last sent packet at ack time of this packet.
    int64_t last_sent_packet_send_time_ms;
    // Send time of this packet.
    int64_t send_time_ms;
    // Ack time of this packet.
    int64_t ack_time_ms;
    // Ack time of the last acked packet at send time of this packet.
    int64_t last_acked_packet_ack_time_ms;
    // Payload size of this packet.
    size_t payload_size_bytes;
    // Amount of data sent before this packet was sent.
    size_t data_sent_bytes;
    // Amount of data sent, before last sent packet.
    size_t data_sent_before_last_sent_packet_bytes;
    // Amount of data acked, before this packet was acked.
    size_t data_acked_bytes;
    // Amount of data acked, before last acked packet.
    size_t data_acked_before_last_acked_packet_bytes;
  };
  struct AverageRtt {
    AverageRtt() {}
    AverageRtt(int64_t sum_of_rtts_ms_, int64_t num_samples_, uint64_t round_)
        : sum_of_rtts_ms(sum_of_rtts_ms_),
          num_samples(num_samples_),
          round(round_) {}
    // Sum of RTTs over the round.
    int64_t sum_of_rtts_ms;
    // Number of RTT samples over the round.
    int64_t num_samples;
    // The number of the round average RTT is recorded for.
    uint64_t round;
  };
  void OnPacketsSent(const Packets& packets) override;
  int GetFeedbackIntervalMs() const override;
  void GiveFeedback(const FeedbackPacket& feedback) override;
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 private:
  void EnterStartup();
  void UpdateBandwidthAndMinRtt(int64_t now_ms,
                                const std::vector<uint16_t>& feedback_vector,
                                int64_t bytes_acked);
  void TryExitingStartup();
  void TryExitingDrain(int64_t now_ms);
  void EnterProbeBw(int64_t now_ms);
  void TryUpdatingCyclePhase(int64_t now_ms);
  void TryEnteringProbeRtt(int64_t now_ms);
  void TryExitingProbeRtt(int64_t now_ms, int64_t round);
  void TryEnteringRecovery(bool new_round_started);
  void TryExitingRecovery(bool new_round_started);
  size_t TargetCongestionWindow(float gain);
  void CalculatePacingRate();

  // Calculates and returns bandwidth sample as minimum between send rate and
  // ack rate, returns nothing if sample cannot be calculated.
  rtc::Optional<int64_t> CalculateBandwidthSample(size_t data_sent,
                                                  int64_t send_time_delta_ms,
                                                  size_t data_acked,
                                                  int64_t ack_time_delta_ms);

  // Calculate and add bandwidth sample only for packets' sent during high gain
  // phase. Motivation of having a seperate bucket for high gain phase is to
  // achieve quicker ramp up. Slight overestimations may happen due to window
  // not being as large as usual.
  void AddSampleForHighGain();

  // Declares lost packets as acked. Implements simple logic by looking at the
  // gap between sequence numbers. If there is a gap between sequence numbers we
  // declare those packets as lost immediately.
  void HandleLoss(uint64_t last_acked_packet, uint64_t recently_acked_packet);
  void AddToPastRtts(int64_t rtt_sample_ms);
  BitrateObserver* observer_;
  Clock* const clock_;
  Mode mode_;
  std::unique_ptr<MaxBandwidthFilter> max_bandwidth_filter_;
  std::unique_ptr<MinRttFilter> min_rtt_filter_;
  std::unique_ptr<CongestionWindow> congestion_window_;
  std::unique_ptr<Random> rand_;
  uint64_t round_count_;
  uint64_t round_trip_end_;
  float pacing_gain_;
  float congestion_window_gain_;

  // If optimal bandwidth has been discovered and reached, (for example after
  // Startup mode) set this variable to true.
  bool full_bandwidth_reached_;

  // Entering time for PROBE_BW mode's cycle phase.
  int64_t cycle_start_time_ms_;

  // Index number of the currently used gain value in PROBE_BW mode, from 0 to
  // kGainCycleLength - 1.
  int64_t cycle_index_;
  size_t bytes_acked_;

  // Time we entered PROBE_RTT mode.
  int64_t probe_rtt_start_time_ms_;

  // First moment of time when data inflight decreased below
  // kMinimumCongestionWindow in PROBE_RTT mode.
  rtc::Optional<int64_t> minimum_congestion_window_start_time_ms_;

  // First round when data inflight decreased below kMinimumCongestionWindow in
  // PROBE_RTT mode.
  int64_t minimum_congestion_window_start_round_;
  size_t bytes_sent_;
  uint16_t last_packet_sent_sequence_number_;
  uint16_t last_packet_acked_sequence_number_;
  int64_t last_packet_ack_time_;
  int64_t last_packet_send_time_;
  int64_t pacing_rate_bps_;

  // Send time of a packet sent first during high gain phase. Also serves as a
  // flag, holding value means that we are already in high gain.
  rtc::Optional<int64_t> first_packet_send_time_during_high_gain_ms_;

  // Send time of a packet sent last during high gain phase.
  int64_t last_packet_send_time_during_high_gain_ms_;

  // Amount of data sent, before first packet was sent during high gain phase.
  int64_t data_sent_before_high_gain_started_bytes_;

  // Amount of data sent, before last packet was sent during high gain phase.
  int64_t data_sent_before_high_gain_ended_bytes_;

  // Ack time of a packet acked first during high gain phase.
  int64_t first_packet_ack_time_during_high_gain_ms_;

  // Ack time of a packet acked last during high gain phase.
  int64_t last_packet_ack_time_during_high_gain_ms_;

  // Amount of data acked, before the first packet was acked during high gain
  // phase.
  int64_t data_acked_before_high_gain_started_bytes_;

  // Amount of data acked, before the last packet was acked during high gain
  // phase.
  int64_t data_acked_before_high_gain_ended_bytes_;

  // Sequence number of the first packet sent during high gain phase.
  uint16_t first_packet_seq_num_during_high_gain_;

  // Sequence number of the last packet sent during high gain phase.
  uint16_t last_packet_seq_num_during_high_gain_;
  bool high_gain_over_;
  std::map<int64_t, PacketStats> packet_stats_;
  std::list<AverageRtt> past_rtts_;
};

class BbrBweReceiver : public BweReceiver {
 public:
  explicit BbrBweReceiver(int flow_id);
  virtual ~BbrBweReceiver();
  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;
 private:
  SimulatedClock clock_;
  std::vector<uint16_t> packet_feedbacks_;
  int64_t last_feedback_ms_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_BBR_H_
