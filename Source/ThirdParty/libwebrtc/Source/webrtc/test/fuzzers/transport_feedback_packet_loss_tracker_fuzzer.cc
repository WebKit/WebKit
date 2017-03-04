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

#include "webrtc/base/array_view.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

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

  bool GetNextTransportFeedback(rtcp::TransportFeedback* feedback) {
    uint16_t base_seq_num = 0;
    if (!ReadData<uint16_t>(&base_seq_num)) {
      return false;
    }
    constexpr int64_t kBaseTimeUs = 1234;  // Irrelevant to this test.
    feedback->SetBase(base_seq_num, kBaseTimeUs);

    uint16_t remaining_packets = 0;
    if (!ReadData<uint16_t>(&remaining_packets))
      return false;
    // Range is [0x00001 : 0x10000], but we keep it 0x0000 to 0xffff for now,
    // and add the last status as RECEIVED. That is because of a limitation
    // that says that the last status cannot be LOST.

    uint16_t seq_num = base_seq_num;
    while (remaining_packets > 0) {
      uint8_t status_byte = 0;
      if (!ReadData<uint8_t>(&status_byte)) {
        return false;
      }
      // Each status byte contains 8 statuses.
      for (size_t i = 0; i < 8 && remaining_packets > 0; ++i) {
        const bool received = (status_byte & (0x01 << i));
        if (received) {
          feedback->AddReceivedPacket(seq_num, kBaseTimeUs);
        }
        ++seq_num;
        --remaining_packets;
      }
    }

    // As mentioned above, all feedbacks must report with a received packet.
    feedback->AddReceivedPacket(seq_num, kBaseTimeUs);

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

  // 0x8000 >= max_window_size >= plr_min_num_packets > rplr_min_num_pairs >= 1
  // (The distribution isn't uniform, but it's enough; more would be overkill.)
  const size_t max_window_size = FuzzInRange(data, size, 2, kSeqNumHalf);
  const size_t plr_min_num_packets =
      FuzzInRange(data, size, 2, max_window_size);
  const size_t rplr_min_num_pairs =
      FuzzInRange(data, size, 1, plr_min_num_packets - 1);

  tracker->reset(new TransportFeedbackPacketLossTracker(
      max_window_size, plr_min_num_packets, rplr_min_num_pairs));

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

bool FuzzPacketSendBlock(
    std::unique_ptr<TransportFeedbackPacketLossTracker>& tracker,
    const uint8_t** data,
    size_t* size) {
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
  tracker->OnPacketAdded(seq_num);
  tracker->Validate();

  for (size_t i = 1; i < packet_block_len; i++) {
    uint16_t delta;
    bool may_continue = FuzzSequenceNumberDelta(data, size, &delta);
    if (!may_continue)
      return false;
    seq_num += delta;
    tracker->OnPacketAdded(seq_num);
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
    rtcp::TransportFeedback feedback;
    bool may_continue = feedback_generator.GetNextTransportFeedback(&feedback);
    if (!may_continue) {
      return false;
    }
    tracker->OnReceivedTransportFeedback(feedback);
    tracker->Validate();
  }

  return true;
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  std::unique_ptr<TransportFeedbackPacketLossTracker> tracker;
  bool may_continue;

  may_continue = Setup(&data, &size, &tracker);

  while (may_continue) {
    may_continue = FuzzPacketSendBlock(tracker, &data, &size);
    if (!may_continue) {
      return;
    }
    may_continue = FuzzTransportFeedbackBlock(tracker, &data, &size);
  }
}

}  // namespace webrtc
