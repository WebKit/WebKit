/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_NACK_TRACKER_H_
#define AUDIO_NACK_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <vector>

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "modules/include/module_common_types_public.h"

//
// The NackTracker class keeps track of the lost packets.
//
// Every time a packet is received, UpdateLastReceivedPacket() has to be
// called to update the NACK list.
//
// If packet N is received, any packet prior to N which has not arrived is
// considered lost, and should be labeled as "missing" (the size of
// the list might be limited and older packet eliminated from the list).
//
// The NackTracker class has to know about the sample rate of the packets to
// compute how old a packet is. So sample rate should be set as soon as the
// first packet is received. If there is a change in the receive codec (sender
// changes codec) then NackTracker should be reset. This is because NetEQ would
// flush its buffer and re-transmission is meaning less for old packet.
// Therefore, in that case, after reset the sampling rate has to be updated.
//
// Thread Safety
// =============
// Please note that this class in not thread safe. The class must be protected
// if different APIs are called from different threads.
//
namespace webrtc {

class NackTracker {
 public:
  // `max_nack_list_size` is the maximum size of the NACK list. If the last
  // received packet has sequence number of N, then NACK list will not contain
  // any element with sequence number earlier than N - `max_nack_list_size`.
  explicit NackTracker(size_t max_nack_list_size);

  ~NackTracker();

  // Set the sampling rate.
  // Resets the state if the sampling rate changes.
  void UpdateSampleRate(int sample_rate_hz);

  // Update the sequence number and the timestamp of the last received RTP. This
  // API should be called every time a packet pushed into ACM.
  void UpdateLastReceivedPacket(uint16_t sequence_number, uint32_t timestamp);

  // Get a list of "missing" packets which have expected time-to-play larger
  // than the given round-trip-time.
  // Note: Late packets are not included.
  // Calling this method multiple times may give different results, since the
  // internal nack list may get flushed if never_nack_multiple_times_ is true.
  std::vector<uint16_t> GetNackList(std::optional<TimeDelta> round_trip_time);

  // Reset to default values. The NACK list is cleared.
  void Reset();

 private:
  class NackListCompare {
   public:
    bool operator()(uint16_t sequence_number_old,
                    uint16_t sequence_number_new) const {
      return IsNewerSequenceNumber(sequence_number_new, sequence_number_old);
    }
  };

  // Map between sequence number and estimated timestamp.
  typedef std::map<uint16_t, uint32_t, NackListCompare> NackList;

  // Returns a valid number of samples per packet given the current received
  // sequence number and timestamp or nullopt of none could be computed.
  std::optional<int> GetSamplesPerPacket(
      uint16_t sequence_number_current_received_rtp,
      uint32_t timestamp_current_received_rtp) const;

  // Given the `sequence_number_current_received_rtp` of currently received RTP
  // update the list. Packets that are older than the received packet are added
  // to the nack list.
  void UpdateList(uint16_t sequence_number_current_received_rtp,
                  uint32_t timestamp_current_received_rtp);

  // Packets which have sequence number older that
  // `sequence_num_last_received_rtp_` - `max_nack_list_size_` are removed
  // from the NACK list.
  void LimitNackListSize();

  // Estimate timestamp of a missing packet given its sequence number.
  uint32_t EstimateTimestamp(uint16_t sequence_number, int samples_per_packet);

  bool Nack(uint32_t timestamp, TimeDelta round_trip_time);

  // NACK list will not keep track of missing packets prior to
  // `sequence_num_last_received_rtp_` - `max_nack_list_size_`.
  const size_t max_nack_list_size_;

  // Valid if a packet is received.
  std::optional<uint16_t> sequence_num_last_received_rtp_;
  std::optional<uint32_t> timestamp_last_received_rtp_;

  Frequency sample_rate_;

  // A list of missing packets to be retransmitted. Components of the list
  // contain the sequence number of missing packets and the estimated time that
  // each pack is going to be played out.
  NackList nack_list_;
};

}  // namespace webrtc

#endif  // AUDIO_NACK_TRACKER_H_
