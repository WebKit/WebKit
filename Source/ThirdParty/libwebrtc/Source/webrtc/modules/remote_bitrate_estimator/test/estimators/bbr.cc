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

#include "modules/remote_bitrate_estimator/test/estimators/bbr.h"

#include <stdlib.h>
#include <algorithm>

#include "modules/remote_bitrate_estimator/test/estimators/congestion_window.h"
#include "modules/remote_bitrate_estimator/test/estimators/max_bandwidth_filter.h"
#include "modules/remote_bitrate_estimator/test/estimators/min_rtt_filter.h"

namespace webrtc {
namespace testing {
namespace bwe {
namespace {
const int kFeedbackIntervalsMs = 5;
// BBR uses this value to double sending rate each round trip. Design document
// suggests using this value.
const float kHighGain = 2.885f;
// BBR uses this value to drain queues created during STARTUP in one round trip
// time.
const float kDrainGain = 1 / kHighGain;
// kStartupGrowthTarget and kMaxRoundsWithoutGrowth are chosen from
// experiments, according to the design document.
const float kStartupGrowthTarget = 1.25f;
const int kMaxRoundsWithoutGrowth = 3;
// Pacing gain values for Probe Bandwidth mode.
const float kPacingGain[] = {1.1f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
const size_t kGainCycleLength = sizeof(kPacingGain) / sizeof(kPacingGain[0]);
// Least number of rounds PROBE_RTT should last.
const int kProbeRttDurationRounds = 1;
// The least amount of milliseconds PROBE_RTT mode should last.
const int kProbeRttDurationMs = 200;
// Gain value for congestion window for assuming that network has no queues.
const float kTargetCongestionWindowGain = 1;
// Gain value for congestion window in PROBE_BW mode. In theory it should be
// equal to 1, but in practice because of delayed acks and the way networks
// work, it is nice to have some extra room in congestion window for full link
// utilization. Value chosen by observations on different tests.
const float kCruisingCongestionWindowGain = 2;
// Pacing gain specific for Recovery mode. Chosen by experiments in simulation
// tool.
const float kRecoveryPacingGain = 0.5f;
// Congestion window gain specific for Recovery mode. Chosen by experiments in
// simulation tool.
const float kRecoveryCongestionWindowGain = 1.5f;
// Number of rounds over which average RTT is stored for Recovery mode.
const size_t kPastRttsFilterSize = 1;
// Threshold to assume average RTT has increased for a round. Chosen by
// experiments in simulation tool.
const float kRttIncreaseThreshold = 3;
// Threshold to assume average RTT has decreased for a round. Chosen by
// experiments in simulation tool.
const float kRttDecreaseThreshold = 1.5f;
// If |kCongestionWindowThreshold| of the congestion window is filled up, tell
// encoder to stop, to avoid building sender side queues.
const float kCongestionWindowThreshold = 0.69f;
// Duration we send at |kDefaultRatebps| in order to ensure BBR has data to work
// with.
const int64_t kDefaultDurationMs = 200;
const int64_t kDefaultRatebps = 300000;
// Congestion window gain for PROBE_RTT mode.
const float kProbeRttCongestionWindowGain = 0.65f;
// We need to be sure that data inflight has increased by at least
// |kTargetCongestionWindowGainForHighGain| compared to the congestion window in
// PROBE_BW's high gain phase, to make ramp-up quicker. As high gain value has
// been decreased from 1.25 to 1.1 we need to make
// |kTargetCongestionWindowGainForHighGain| slightly higher than the actual high
// gain value.
const float kTargetCongestionWindowGainForHighGain = 1.15f;
// Encoder rate gain value for PROBE_RTT mode.
const float kEncoderRateGainForProbeRtt = 0.1f;
}  // namespace

BbrBweSender::BbrBweSender(BitrateObserver* observer, Clock* clock)
    : BweSender(0),
      observer_(observer),
      clock_(clock),
      mode_(STARTUP),
      max_bandwidth_filter_(new MaxBandwidthFilter()),
      min_rtt_filter_(new MinRttFilter()),
      congestion_window_(new CongestionWindow()),
      rand_(new Random(time(NULL))),
      round_count_(0),
      round_trip_end_(0),
      full_bandwidth_reached_(false),
      cycle_start_time_ms_(0),
      cycle_index_(0),
      bytes_acked_(0),
      probe_rtt_start_time_ms_(0),
      minimum_congestion_window_start_time_ms_(0),
      minimum_congestion_window_start_round_(0),
      bytes_sent_(0),
      last_packet_sent_sequence_number_(0),
      last_packet_acked_sequence_number_(0),
      last_packet_ack_time_(0),
      last_packet_send_time_(0),
      pacing_rate_bps_(0),
      last_packet_send_time_during_high_gain_ms_(-1),
      data_sent_before_high_gain_started_bytes_(-1),
      data_sent_before_high_gain_ended_bytes_(-1),
      first_packet_ack_time_during_high_gain_ms_(-1),
      last_packet_ack_time_during_high_gain_ms_(-1),
      data_acked_before_high_gain_started_bytes_(-1),
      data_acked_before_high_gain_ended_bytes_(-1),
      first_packet_seq_num_during_high_gain_(0),
      last_packet_seq_num_during_high_gain_(0),
      high_gain_over_(false),
      packet_stats_(),
      past_rtts_() {
  // Initially enter Startup mode.
  EnterStartup();
}

BbrBweSender::~BbrBweSender() {}

BbrBweSender::PacketStats::PacketStats() = default;

BbrBweSender::PacketStats::PacketStats(
    uint16_t sequence_number_,
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

BbrBweSender::PacketStats::PacketStats(const PacketStats&) = default;

int BbrBweSender::GetFeedbackIntervalMs() const {
  return kFeedbackIntervalsMs;
}

void BbrBweSender::CalculatePacingRate() {
  pacing_rate_bps_ =
      max_bandwidth_filter_->max_bandwidth_estimate_bps() * pacing_gain_;
}

// Declare lost packets as acked.
void BbrBweSender::HandleLoss(uint64_t last_acked_packet,
                              uint64_t recently_acked_packet) {
  for (uint16_t i = last_acked_packet + 1;
       AheadOrAt<uint16_t>(recently_acked_packet - 1, i); i++) {
    congestion_window_->AckReceived(packet_stats_[i].payload_size_bytes);
    observer_->OnBytesAcked(packet_stats_[i].payload_size_bytes);
  }
}

void BbrBweSender::AddToPastRtts(int64_t rtt_sample_ms) {
  uint64_t last_round = 0;
  if (!past_rtts_.empty())
    last_round = past_rtts_.back().round;

  // Try to add the sample to the last round.
  if (last_round == round_count_ && !past_rtts_.empty()) {
    past_rtts_.back().sum_of_rtts_ms += rtt_sample_ms;
    past_rtts_.back().num_samples++;
  } else {
    // If the sample belongs to a new round, keep number of rounds in the window
    // equal to |kPastRttsFilterSize|.
    if (past_rtts_.size() == kPastRttsFilterSize)
      past_rtts_.pop_front();
    past_rtts_.push_back(
        BbrBweSender::AverageRtt(rtt_sample_ms, 1, round_count_));
  }
}

void BbrBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  last_packet_ack_time_ = now_ms;
  const BbrBweFeedback& fb = static_cast<const BbrBweFeedback&>(feedback);
  // feedback_vector holds values of acknowledged packets' sequence numbers.
  const std::vector<uint16_t>& feedback_vector = fb.packet_feedback_vector();
  // Go through all the packets acked, update variables/containers accordingly.
  for (uint16_t sequence_number : feedback_vector) {
    // Completing packet information with a recently received ack.
    PacketStats* packet = &packet_stats_[sequence_number];
    bytes_acked_ += packet->payload_size_bytes;
    packet->data_sent_bytes = bytes_sent_;
    packet->last_sent_packet_send_time_ms = last_packet_send_time_;
    packet->data_acked_bytes = bytes_acked_;
    packet->ack_time_ms = now_ms;
    // Logic specific to applying "bucket" to high gain, in order to have
    // quicker ramp-up. We check if we started receiving acks for the packets
    // sent during high gain phase.
    if (packet->sequence_number == first_packet_seq_num_during_high_gain_) {
      first_packet_ack_time_during_high_gain_ms_ = now_ms;
      // Substracting half of the packet's size to avoid overestimation.
      data_acked_before_high_gain_started_bytes_ =
          bytes_acked_ - packet->payload_size_bytes / 2;
    }
    // If the last packet of high gain phase has been acked, high gain phase is
    // over.
    if (packet->sequence_number == last_packet_seq_num_during_high_gain_) {
      last_packet_ack_time_during_high_gain_ms_ = now_ms;
      data_acked_before_high_gain_ended_bytes_ =
          bytes_acked_ - packet->payload_size_bytes / 2;
      high_gain_over_ = true;
    }
    observer_->OnBytesAcked(packet->payload_size_bytes);
    congestion_window_->AckReceived(packet->payload_size_bytes);
    HandleLoss(last_packet_acked_sequence_number_, packet->sequence_number);
    last_packet_acked_sequence_number_ = packet->sequence_number;
    // Logic for wrapping sequence numbers. If round started with packet number
    // x, it can never end on y, if x > y. That could happen when sequence
    // numbers are wrapped after some point.
    if (packet->sequence_number == 0)
      round_trip_end_ = 0;
  }
  // Check if new round started for the connection.
  bool new_round_started = false;
  if (!feedback_vector.empty()) {
    if (last_packet_acked_sequence_number_ > round_trip_end_) {
      new_round_started = true;
      round_count_++;
      round_trip_end_ = last_packet_sent_sequence_number_;
    }
  }
  TryEnteringProbeRtt(now_ms);
  UpdateBandwidthAndMinRtt(now_ms, feedback_vector, bytes_acked_);
  if (new_round_started && !full_bandwidth_reached_) {
    full_bandwidth_reached_ = max_bandwidth_filter_->FullBandwidthReached(
        kStartupGrowthTarget, kMaxRoundsWithoutGrowth);
  }
  switch (mode_) {
    break;
    case STARTUP:
      TryExitingStartup();
      break;
    case DRAIN:
      TryExitingDrain(now_ms);
      break;
    case PROBE_BW:
      TryUpdatingCyclePhase(now_ms);
      break;
    case PROBE_RTT:
      TryExitingProbeRtt(now_ms, round_count_);
      break;
    case RECOVERY:
      TryExitingRecovery(new_round_started);
      break;
  }
  TryEnteringRecovery(new_round_started);  // Comment this line to disable
                                           // entering Recovery mode.
  for (uint16_t f : feedback_vector)
    AddToPastRtts(packet_stats_[f].ack_time_ms - packet_stats_[f].send_time_ms);
  CalculatePacingRate();
  size_t cwnd = congestion_window_->GetCongestionWindow(
      mode_, max_bandwidth_filter_->max_bandwidth_estimate_bps(),
      min_rtt_filter_->min_rtt_ms(), congestion_window_gain_);
  // Make sure we don't get stuck when pacing_rate is 0, because of simulation
  // tool specifics.
  if (!pacing_rate_bps_)
    pacing_rate_bps_ = 100;
  BWE_TEST_LOGGING_PLOT(1, "SendRate", now_ms, pacing_rate_bps_ / 1000);
  int64_t rate_for_pacer_bps = pacing_rate_bps_;
  int64_t rate_for_encoder_bps = pacing_rate_bps_;
  if (congestion_window_->data_inflight() >= cwnd * kCongestionWindowThreshold)
    rate_for_encoder_bps = 0;
  // We dont completely stop sending during PROBE_RTT, so we need encoder to
  // produce something, another way of doing this would be telling encoder to
  // stop and send padding instead of actual data.
  if (mode_ == PROBE_RTT)
    rate_for_encoder_bps = rate_for_pacer_bps * kEncoderRateGainForProbeRtt;
  // Send for 300 kbps for first 200 ms, so that BBR has data to work with.
  if (now_ms <= kDefaultDurationMs)
    observer_->OnNetworkChanged(
        kDefaultRatebps, kDefaultRatebps, false,
        clock_->TimeInMicroseconds() + kFeedbackIntervalsMs * 1000, cwnd);
  else
    observer_->OnNetworkChanged(
        rate_for_encoder_bps, rate_for_pacer_bps, mode_ == PROBE_RTT,
        clock_->TimeInMicroseconds() + kFeedbackIntervalsMs * 1000, cwnd);
}

size_t BbrBweSender::TargetCongestionWindow(float gain) {
  size_t target_congestion_window =
      congestion_window_->GetTargetCongestionWindow(
          max_bandwidth_filter_->max_bandwidth_estimate_bps(),
          min_rtt_filter_->min_rtt_ms(), gain);
  return target_congestion_window;
}

absl::optional<int64_t> BbrBweSender::CalculateBandwidthSample(
    size_t data_sent_bytes,
    int64_t send_time_delta_ms,
    size_t data_acked_bytes,
    int64_t ack_time_delta_ms) {
  absl::optional<int64_t> bandwidth_sample;
  if (send_time_delta_ms > 0)
    bandwidth_sample.emplace(data_sent_bytes * 8000 / send_time_delta_ms);
  absl::optional<int64_t> ack_rate;
  if (ack_time_delta_ms > 0)
    ack_rate.emplace(data_acked_bytes * 8000 / ack_time_delta_ms);
  // If send rate couldn't be calculated automaticaly set |bandwidth_sample| to
  // ack_rate.
  if (!bandwidth_sample)
    bandwidth_sample = ack_rate;
  if (bandwidth_sample && ack_rate)
    bandwidth_sample.emplace(std::min(*bandwidth_sample, *ack_rate));
  return bandwidth_sample;
}

void BbrBweSender::AddSampleForHighGain() {
  if (!high_gain_over_)
    return;
  high_gain_over_ = false;
  // Calculate data sent/acked and time elapsed only for packets sent during
  // high gain phase.
  size_t data_sent_bytes = data_sent_before_high_gain_ended_bytes_ -
                           data_sent_before_high_gain_started_bytes_;
  int64_t send_time_delta_ms = last_packet_send_time_during_high_gain_ms_ -
                               *first_packet_send_time_during_high_gain_ms_;
  size_t data_acked_bytes = data_acked_before_high_gain_ended_bytes_ -
                            data_acked_before_high_gain_started_bytes_;
  int64_t ack_time_delta_ms = last_packet_ack_time_during_high_gain_ms_ -
                              first_packet_ack_time_during_high_gain_ms_;
  absl::optional<int64_t> bandwidth_sample = CalculateBandwidthSample(
      data_sent_bytes, send_time_delta_ms, data_acked_bytes, ack_time_delta_ms);
  if (bandwidth_sample)
    max_bandwidth_filter_->AddBandwidthSample(*bandwidth_sample, round_count_);
  first_packet_send_time_during_high_gain_ms_.reset();
}

void BbrBweSender::UpdateBandwidthAndMinRtt(
    int64_t now_ms,
    const std::vector<uint16_t>& feedback_vector,
    int64_t bytes_acked) {
  absl::optional<int64_t> min_rtt_sample_ms;
  for (uint16_t f : feedback_vector) {
    PacketStats packet = packet_stats_[f];
    size_t data_sent_bytes =
        packet.data_sent_bytes - packet.data_sent_before_last_sent_packet_bytes;
    int64_t send_time_delta_ms =
        packet.last_sent_packet_send_time_ms - packet.send_time_ms;
    size_t data_acked_bytes = packet.data_acked_bytes -
                              packet.data_acked_before_last_acked_packet_bytes;
    int64_t ack_time_delta_ms =
        packet.ack_time_ms - packet.last_acked_packet_ack_time_ms;
    absl::optional<int64_t> bandwidth_sample =
        CalculateBandwidthSample(data_sent_bytes, send_time_delta_ms,
                                 data_acked_bytes, ack_time_delta_ms);
    if (bandwidth_sample)
      max_bandwidth_filter_->AddBandwidthSample(*bandwidth_sample,
                                                round_count_);
    // AddSampleForHighGain();  // Comment to disable bucket for high gain.
    if (!min_rtt_sample_ms)
      min_rtt_sample_ms.emplace(packet.ack_time_ms - packet.send_time_ms);
    else
      *min_rtt_sample_ms = std::min(*min_rtt_sample_ms,
                                    packet.ack_time_ms - packet.send_time_ms);
    BWE_TEST_LOGGING_PLOT(1, "MinRtt", now_ms,
                          packet.ack_time_ms - packet.send_time_ms);
  }
  // We only feed RTT samples into the min_rtt filter which were not produced
  // during 1.1 gain phase, to ensure they contain no queueing delay. But if the
  // rtt sample from 1.1 gain phase improves the current estimate then we should
  // make it as a new best estimate.
  if (pacing_gain_ <= 1.0f || !min_rtt_filter_->min_rtt_ms() ||
      *min_rtt_filter_->min_rtt_ms() >= *min_rtt_sample_ms)
    min_rtt_filter_->AddRttSample(*min_rtt_sample_ms, now_ms);
}

void BbrBweSender::EnterStartup() {
  mode_ = STARTUP;
  pacing_gain_ = kHighGain;
  congestion_window_gain_ = kHighGain;
}

void BbrBweSender::TryExitingStartup() {
  if (full_bandwidth_reached_) {
    mode_ = DRAIN;
    pacing_gain_ = kDrainGain;
    congestion_window_gain_ = kHighGain;
  }
}

void BbrBweSender::TryExitingDrain(int64_t now_ms) {
  if (congestion_window_->data_inflight() <=
      TargetCongestionWindow(kTargetCongestionWindowGain))
    EnterProbeBw(now_ms);
}

// Start probing with a random gain value, which is different form 0.75,
// starting with 0.75 doesn't offer any benefits as there are no queues to be
// drained.
void BbrBweSender::EnterProbeBw(int64_t now_ms) {
  mode_ = PROBE_BW;
  congestion_window_gain_ = kCruisingCongestionWindowGain;
  int index = rand_->Rand(kGainCycleLength - 2);
  if (index == 1)
    index = kGainCycleLength - 1;
  pacing_gain_ = kPacingGain[index];
  cycle_start_time_ms_ = now_ms;
  cycle_index_ = index;
}

void BbrBweSender::TryUpdatingCyclePhase(int64_t now_ms) {
  // Each phase should last rougly min_rtt ms time.
  bool advance_cycle_phase = false;
  if (min_rtt_filter_->min_rtt_ms())
    advance_cycle_phase =
        now_ms - cycle_start_time_ms_ > *min_rtt_filter_->min_rtt_ms();
  // If BBR was probing and it couldn't increase data inflight sufficiently in
  // one min_rtt time, continue probing. BBR design doc isn't clear about this,
  // but condition helps in quicker ramp-up and performs better.
  if (pacing_gain_ > 1.0 &&
      congestion_window_->data_inflight() <
          TargetCongestionWindow(kTargetCongestionWindowGainForHighGain))
    advance_cycle_phase = false;
  // If BBR has already drained queues there is no point in continuing draining
  // phase.
  if (pacing_gain_ < 1.0 &&
      congestion_window_->data_inflight() <= TargetCongestionWindow(1))
    advance_cycle_phase = true;
  if (advance_cycle_phase) {
    cycle_index_++;
    cycle_index_ %= kGainCycleLength;
    pacing_gain_ = kPacingGain[cycle_index_];
    cycle_start_time_ms_ = now_ms;
  }
}

void BbrBweSender::TryEnteringProbeRtt(int64_t now_ms) {
  if (min_rtt_filter_->MinRttExpired(now_ms) && mode_ != PROBE_RTT) {
    mode_ = PROBE_RTT;
    pacing_gain_ = 1;
    congestion_window_gain_ = kProbeRttCongestionWindowGain;
    probe_rtt_start_time_ms_ = now_ms;
    minimum_congestion_window_start_time_ms_.reset();
  }
}

// |minimum_congestion_window_start_time_|'s value is set to the first moment
// when data inflight was less then
// |CongestionWindow::kMinimumCongestionWindowBytes|, we should make sure that
// BBR has been in PROBE_RTT mode for at least one round or 200ms.
void BbrBweSender::TryExitingProbeRtt(int64_t now_ms, int64_t round) {
  if (!minimum_congestion_window_start_time_ms_) {
    if (congestion_window_->data_inflight() <=
        TargetCongestionWindow(kProbeRttCongestionWindowGain)) {
      minimum_congestion_window_start_time_ms_.emplace(now_ms);
      minimum_congestion_window_start_round_ = round;
    }
  } else {
    if (now_ms - *minimum_congestion_window_start_time_ms_ >=
            kProbeRttDurationMs &&
        round - minimum_congestion_window_start_round_ >=
            kProbeRttDurationRounds) {
      EnterProbeBw(now_ms);
    }
  }
}

void BbrBweSender::TryEnteringRecovery(bool new_round_started) {
  if (mode_ == RECOVERY || !new_round_started || !full_bandwidth_reached_ ||
      !min_rtt_filter_->min_rtt_ms())
    return;
  uint64_t increased_rtt_round_counter = 0;
  // If average RTT for past |kPastRttsFilterSize| rounds has been more than
  // some multiplier of min_rtt_ms enter Recovery.
  for (BbrBweSender::AverageRtt i : past_rtts_) {
    if (i.sum_of_rtts_ms / (int64_t)i.num_samples >=
        *min_rtt_filter_->min_rtt_ms() * kRttIncreaseThreshold)
      increased_rtt_round_counter++;
  }
  if (increased_rtt_round_counter < kPastRttsFilterSize)
    return;
  mode_ = RECOVERY;
  pacing_gain_ = kRecoveryPacingGain;
  congestion_window_gain_ = kRecoveryCongestionWindowGain;
}

void BbrBweSender::TryExitingRecovery(bool new_round_started) {
  if (mode_ != RECOVERY || !new_round_started || !full_bandwidth_reached_)
    return;
  // If average RTT for the past round has decreased sufficiently exit Recovery.
  if (!past_rtts_.empty()) {
    BbrBweSender::AverageRtt last_round_sample = past_rtts_.back();
    if (last_round_sample.sum_of_rtts_ms / last_round_sample.num_samples <=
        *min_rtt_filter_->min_rtt_ms() * kRttDecreaseThreshold) {
      EnterProbeBw(clock_->TimeInMilliseconds());
    }
  }
}

int64_t BbrBweSender::TimeUntilNextProcess() {
  return 50;
}

void BbrBweSender::OnPacketsSent(const Packets& packets) {
  for (Packet* packet : packets) {
    if (packet->GetPacketType() == Packet::kMedia) {
      MediaPacket* media_packet = static_cast<MediaPacket*>(packet);
      bytes_sent_ += media_packet->payload_size();
      PacketStats packet_stats = PacketStats(
          media_packet->sequence_number(), 0,
          media_packet->sender_timestamp_ms(), 0, last_packet_ack_time_,
          media_packet->payload_size(), 0, bytes_sent_, 0, bytes_acked_);
      packet_stats_[media_packet->sequence_number()] = packet_stats;
      last_packet_send_time_ = media_packet->sender_timestamp_ms();
      last_packet_sent_sequence_number_ = media_packet->sequence_number();
      // If this is the first packet sent for high gain phase, save data for it.
      if (!first_packet_send_time_during_high_gain_ms_ && pacing_gain_ > 1) {
        first_packet_send_time_during_high_gain_ms_.emplace(
            last_packet_send_time_);
        data_sent_before_high_gain_started_bytes_ =
            bytes_sent_ - media_packet->payload_size() / 2;
        first_packet_seq_num_during_high_gain_ =
            media_packet->sequence_number();
      }
      // This condition ensures that |last_packet_seq_num_during_high_gain_|
      // will contain a sequence number of the last packet sent during high gain
      // phase.
      if (pacing_gain_ > 1) {
        last_packet_send_time_during_high_gain_ms_ = last_packet_send_time_;
        data_sent_before_high_gain_ended_bytes_ =
            bytes_sent_ - media_packet->payload_size() / 2;
        last_packet_seq_num_during_high_gain_ = media_packet->sequence_number();
      }
      congestion_window_->PacketSent(media_packet->payload_size());
    }
  }
}

void BbrBweSender::Process() {}

BbrBweReceiver::BbrBweReceiver(int flow_id)
    : BweReceiver(flow_id, kReceivingRateTimeWindowMs),
      clock_(0),
      packet_feedbacks_(),
      last_feedback_ms_(0) {}

BbrBweReceiver::~BbrBweReceiver() {}

void BbrBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                   const MediaPacket& media_packet) {
  packet_feedbacks_.push_back(media_packet.sequence_number());
  BweReceiver::ReceivePacket(arrival_time_ms, media_packet);
}

FeedbackPacket* BbrBweReceiver::GetFeedback(int64_t now_ms) {
  last_feedback_ms_ = now_ms;
  int64_t corrected_send_time_ms = 0L;
  if (!received_packets_.empty()) {
    PacketIdentifierNode& latest = *(received_packets_.begin());
    corrected_send_time_ms =
        latest.send_time_ms + now_ms - latest.arrival_time_ms;
  }
  FeedbackPacket* fb = new BbrBweFeedback(
      flow_id_, now_ms * 1000, corrected_send_time_ms, packet_feedbacks_);
  packet_feedbacks_.clear();
  return fb;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
