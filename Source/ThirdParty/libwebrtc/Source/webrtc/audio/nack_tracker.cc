/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/nack_tracker.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr Frequency kDefaultSampleRate = Frequency::Hertz(48000);
constexpr TimeDelta kMaxPacketDuration = TimeDelta::Millis(120);
constexpr TimeDelta kDefaultRtt = TimeDelta::Millis(100);
constexpr TimeDelta kMaxNackDelay = TimeDelta::Seconds(1);

}  // namespace

NackTracker::NackTracker(size_t max_nack_list_size)
    : max_nack_list_size_(max_nack_list_size),
      sample_rate_(kDefaultSampleRate) {}

NackTracker::~NackTracker() = default;

void NackTracker::UpdateSampleRate(int sample_rate_hz) {
  RTC_DCHECK_GT(sample_rate_hz, 0);
  Frequency sample_rate = Frequency::Hertz(sample_rate_hz);
  if (sample_rate != sample_rate_) {
    Reset();
    sample_rate_ = sample_rate;
  }
}

void NackTracker::UpdateLastReceivedPacket(uint16_t sequence_number,
                                           uint32_t timestamp) {
  if (!sequence_num_last_received_rtp_.has_value()) {
    sequence_num_last_received_rtp_ = sequence_number;
    timestamp_last_received_rtp_ = timestamp;
    return;
  }

  nack_list_.erase(sequence_number);

  if (!IsNewerSequenceNumber(sequence_number,
                             *sequence_num_last_received_rtp_)) {
    return;
  }

  UpdateList(sequence_number, timestamp);

  sequence_num_last_received_rtp_ = sequence_number;
  timestamp_last_received_rtp_ = timestamp;
  LimitNackListSize();
}

std::optional<int> NackTracker::GetSamplesPerPacket(
    uint16_t sequence_number_current_received_rtp,
    uint32_t timestamp_current_received_rtp) const {
  uint32_t timestamp_increase =
      timestamp_current_received_rtp - *timestamp_last_received_rtp_;
  uint16_t sequence_num_increase =
      sequence_number_current_received_rtp - *sequence_num_last_received_rtp_;

  int samples_per_packet = timestamp_increase / sequence_num_increase;
  if (samples_per_packet == 0 ||
      samples_per_packet > kMaxPacketDuration * sample_rate_) {
    // Not a valid samples per packet.
    return std::nullopt;
  }
  return samples_per_packet;
}

void NackTracker::UpdateList(uint16_t sequence_number_current_received_rtp,
                             uint32_t timestamp_current_received_rtp) {
  if (!IsNewerSequenceNumber(sequence_number_current_received_rtp,
                             *sequence_num_last_received_rtp_ + 1)) {
    return;
  }

  std::optional<int> samples_per_packet = GetSamplesPerPacket(
      sequence_number_current_received_rtp, timestamp_current_received_rtp);
  if (!samples_per_packet) {
    return;
  }

  for (uint16_t sequence_number = *sequence_num_last_received_rtp_ + 1;
       IsNewerSequenceNumber(sequence_number_current_received_rtp,
                             sequence_number);
       ++sequence_number) {
    nack_list_[sequence_number] =
        EstimateTimestamp(sequence_number, *samples_per_packet);
  }
}

uint32_t NackTracker::EstimateTimestamp(uint16_t sequence_num,
                                        int samples_per_packet) {
  uint16_t sequence_num_diff = sequence_num - *sequence_num_last_received_rtp_;
  return sequence_num_diff * samples_per_packet + *timestamp_last_received_rtp_;
}

void NackTracker::Reset() {
  nack_list_.clear();

  sequence_num_last_received_rtp_.reset();
  timestamp_last_received_rtp_.reset();
  sample_rate_ = kDefaultSampleRate;
}

void NackTracker::LimitNackListSize() {
  uint16_t limit = *sequence_num_last_received_rtp_ -
                   static_cast<uint16_t>(max_nack_list_size_) - 1;
  nack_list_.erase(nack_list_.begin(), nack_list_.upper_bound(limit));
}

std::vector<uint16_t> NackTracker::GetNackList(
    std::optional<TimeDelta> round_trip_time) {
  std::vector<uint16_t> sequence_numbers;
  if (!round_trip_time.has_value()) {
    round_trip_time = kDefaultRtt;
  }
  for (const auto [sequence_number, timestamp] : nack_list_) {
    if (Nack(timestamp, *round_trip_time)) {
      sequence_numbers.push_back(sequence_number);
    }
  }
  return sequence_numbers;
}

bool NackTracker::Nack(uint32_t timestamp, TimeDelta round_trip_time) {
  TimeDelta time_since_packet =
      (*timestamp_last_received_rtp_ - timestamp) / sample_rate_;
  return time_since_packet + round_trip_time < kMaxNackDelay;
}

}  // namespace webrtc
