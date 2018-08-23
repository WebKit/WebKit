/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "api/array_view.h"
#include "audio/transport_feedback_packet_loss_tracker.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace webrtc {

namespace {

template <typename T>
T FuzzInput(const uint8_t** data, size_t* size) {
  RTC_CHECK_GE(*size, sizeof(T));
  T rc = ByteReader<T>::ReadBigEndian(*data);
  *data += sizeof(T);
  *size -= sizeof(T);
  return rc;
}

size_t FuzzInRange(const uint8_t** data,
                   size_t* size,
                   size_t lower,
                   size_t upper) {
  // Achieve a close-to-uniform distribution.
  RTC_CHECK_LE(lower, upper);
  RTC_CHECK_LT(upper - lower, 1 << (8 * sizeof(uint16_t)));
  const size_t range = upper - lower;
  const uint16_t fuzzed = FuzzInput<uint16_t>(data, size);
  const size_t offset = (static_cast<float>(fuzzed) / 0x10000) * (range + 1);
  RTC_CHECK_LE(offset, range);  // (fuzzed <= 0xffff) -> (offset < range + 1)
  return lower + offset;
}

class TransportFeedbackGenerator {
 public:
  explicit TransportFeedbackGenerator(const uint8_t** data, size_t* size)
      : data_(data), size_(size) {}

  bool GetNextTransportFeedbackVector(
      std::vector<PacketFeedback>* feedback_vector) {
    RTC_CHECK(feedback_vector->empty());

    uint16_t remaining_packets = 0;
    if (!ReadData<uint16_t>(&remaining_packets)) {
      return false;
    }

    if (remaining_packets == 0) {
      return true;
    }

    uint16_t seq_num;
    if (!ReadData<uint16_t>(&seq_num)) {  // Fuzz base sequence number.
      return false;
    }

    while (remaining_packets > 0) {
      uint8_t status_byte = 0;
      if (!ReadData<uint8_t>(&status_byte)) {
        return false;
      }
      // Each status byte contains 8 statuses.
      for (size_t i = 0; i < 8 && remaining_packets > 0; ++i) {
        // Any positive integer signals reception. kNotReceived signals loss.
        // Other values are just illegal.
        constexpr int64_t kArrivalTimeMs = 1234;

        const bool received = (status_byte & (0x01 << i));
        feedback_vector->emplace_back(PacketFeedback(
            received ? kArrivalTimeMs : PacketFeedback::kNotReceived,
            seq_num++));
        --remaining_packets;
      }
    }

    return true;
  }

 private:
  template <typename T>
  bool ReadData(T* value) {
    if (*size_ < sizeof(T)) {
      return false;
    } else {
      *value = FuzzInput<T>(data_, size_);
      return true;
    }
  }

  const uint8_t** data_;
  size_t* size_;
};

bool Setup(const uint8_t** data,
           size_t* size,
           std::unique_ptr<TransportFeedbackPacketLossTracker>* tracker) {
  if (*size < 3 * sizeof(uint16_t)) {
    return false;
  }

  constexpr size_t kSeqNumHalf = 0x8000u;

  const int64_t max_window_size_ms = FuzzInRange(data, size, 1, 1 << 16);
  const size_t plr_min_num_packets = FuzzInRange(data, size, 1, kSeqNumHalf);
  const size_t rplr_min_num_pairs = FuzzInRange(data, size, 1, kSeqNumHalf - 1);

  tracker->reset(new TransportFeedbackPacketLossTracker(
      max_window_size_ms, plr_min_num_packets, rplr_min_num_pairs));

  return true;
}

bool FuzzSequenceNumberDelta(const uint8_t** data,
                             size_t* size,
                             uint16_t* delta) {
  // Fuzz with a higher likelihood for immediately consecutive pairs
  // than you would by just fuzzing 1-256.
  // Note: Values higher than 256 still possible, but that would be in a new
  // packet-sending block.
  // * Fuzzed value in [0 : 127]   (50% chance) -> delta is 1.
  // * Fuzzed value in [128 : 255] (50% chance) -> delta in range [2 : 129].
  if (*size < sizeof(uint8_t)) {
    return false;
  }
  uint8_t fuzzed = FuzzInput<uint8_t>(data, size);
  *delta = (fuzzed < 128) ? 1 : (fuzzed - 128 + 2);
  return true;
}

bool FuzzClockAdvancement(const uint8_t** data,
                          size_t* size,
                          int64_t* time_ms) {
  // Fuzzing 64-bit worth of delta would be extreme overkill, as 32-bit is
  // already ~49 days long. We'll fuzz deltas up to a smaller value, and this
  // way also guarantee that wrap-around is impossible, as in real life.

  // Higher likelihood for more likely cases:
  //  5% chance of delta = 0.
  // 20% chance of delta in range [1 : 10] (uniformly distributed)
  // 55% chance of delta in range [11 : 500] (uniformly distributed)
  // 20% chance of delta in range [501 : 10000] (uniformly distributed)
  struct ProbabilityDistribution {
    float probability;
    size_t lower;
    size_t higher;
  };
  constexpr ProbabilityDistribution clock_probability_distribution[] = {
      {0.05, 0, 0}, {0.20, 1, 10}, {0.55, 11, 500}, {0.20, 501, 10000}};

  if (*size < sizeof(uint8_t)) {
    return false;
  }
  const float fuzzed = FuzzInput<uint8_t>(data, size) / 256.0f;

  float cumulative_probability = 0;
  for (const auto& dist : clock_probability_distribution) {
    cumulative_probability += dist.probability;
    if (fuzzed < cumulative_probability) {
      if (dist.lower == dist.higher) {
        *time_ms += dist.lower;
        return true;
      } else if (*size < sizeof(uint16_t)) {
        return false;
      } else {
        *time_ms += FuzzInRange(data, size, dist.lower, dist.higher);
        return true;
      }
    }
  }

  RTC_NOTREACHED();
  return false;
}

bool FuzzPacketSendBlock(
    std::unique_ptr<TransportFeedbackPacketLossTracker>& tracker,
    const uint8_t** data,
    size_t* size,
    int64_t* time_ms) {
  // We want to test with block lengths between 1 and 2^16, inclusive.
  if (*size < sizeof(uint8_t)) {
    return false;
  }
  size_t packet_block_len = 1 + FuzzInput<uint8_t>(data, size);

  // First sent sequence number uniformly selected.
  if (*size < sizeof(uint16_t)) {
    return false;
  }
  uint16_t seq_num = FuzzInput<uint16_t>(data, size);
  tracker->OnPacketAdded(seq_num, *time_ms);
  tracker->Validate();

  bool may_continue = FuzzClockAdvancement(data, size, time_ms);
  if (!may_continue) {
    return false;
  }

  for (size_t i = 1; i < packet_block_len; i++) {
    uint16_t delta;
    may_continue = FuzzSequenceNumberDelta(data, size, &delta);
    if (!may_continue)
      return false;
    may_continue = FuzzClockAdvancement(data, size, time_ms);
    if (!may_continue)
      return false;
    seq_num += delta;
    tracker->OnPacketAdded(seq_num, *time_ms);
    tracker->Validate();
  }

  return true;
}

bool FuzzTransportFeedbackBlock(
    std::unique_ptr<TransportFeedbackPacketLossTracker>& tracker,
    const uint8_t** data,
    size_t* size) {
  // Fuzz the number of back-to-back feedbacks. At least one, or this would
  // be meaningless - we'd go straight back to fuzzing another packet
  // transmission block.
  if (*size < sizeof(uint8_t)) {
    return false;
  }

  size_t feedbacks_num = 1 + (FuzzInput<uint8_t>(data, size) & 0x3f);
  TransportFeedbackGenerator feedback_generator(data, size);

  for (size_t i = 0; i < feedbacks_num; i++) {
    std::vector<PacketFeedback> feedback_vector;
    bool may_continue =
        feedback_generator.GetNextTransportFeedbackVector(&feedback_vector);
    if (!may_continue) {
      return false;
    }
    tracker->OnPacketFeedbackVector(feedback_vector);
    tracker->Validate();
  }

  return true;
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  std::unique_ptr<TransportFeedbackPacketLossTracker> tracker;
  bool may_continue;

  may_continue = Setup(&data, &size, &tracker);

  // We never expect this to wrap around, so it makes sense to just start with
  // a sane value, and keep on incrementing by a fuzzed delta.
  if (size < sizeof(uint32_t)) {
    return;
  }
  int64_t time_ms = FuzzInput<uint32_t>(&data, &size);

  while (may_continue) {
    may_continue = FuzzPacketSendBlock(tracker, &data, &size, &time_ms);
    if (!may_continue) {
      return;
    }
    may_continue = FuzzTransportFeedbackBlock(tracker, &data, &size);
  }
}

}  // namespace webrtc
