/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_PACKET_BUFFER_H_
#define MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_PACKET_BUFFER_H_

#include "modules/audio_coding/neteq/packet_buffer.h"
#include "test/gmock.h"

namespace webrtc {

class MockPacketBuffer : public PacketBuffer {
 public:
  MockPacketBuffer(size_t max_number_of_packets, const TickTimer* tick_timer)
      : PacketBuffer(max_number_of_packets, tick_timer) {}
  virtual ~MockPacketBuffer() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_METHOD0(Flush, void());
  MOCK_CONST_METHOD0(Empty, bool());
  int InsertPacket(Packet&& packet, StatisticsCalculator* stats) {
    return InsertPacketWrapped(&packet, stats);
  }
  // Since gtest does not properly support move-only types, InsertPacket is
  // implemented as a wrapper. You'll have to implement InsertPacketWrapped
  // instead and move from |*packet|.
  MOCK_METHOD2(InsertPacketWrapped,
               int(Packet* packet, StatisticsCalculator* stats));
  MOCK_METHOD5(InsertPacketList,
               int(PacketList* packet_list,
                   const DecoderDatabase& decoder_database,
                   absl::optional<uint8_t>* current_rtp_payload_type,
                   absl::optional<uint8_t>* current_cng_rtp_payload_type,
                   StatisticsCalculator* stats));
  MOCK_CONST_METHOD1(NextTimestamp, int(uint32_t* next_timestamp));
  MOCK_CONST_METHOD2(NextHigherTimestamp,
                     int(uint32_t timestamp, uint32_t* next_timestamp));
  MOCK_CONST_METHOD0(PeekNextPacket, const Packet*());
  MOCK_METHOD0(GetNextPacket, absl::optional<Packet>());
  MOCK_METHOD1(DiscardNextPacket, int(StatisticsCalculator* stats));
  MOCK_METHOD3(DiscardOldPackets,
               void(uint32_t timestamp_limit,
                    uint32_t horizon_samples,
                    StatisticsCalculator* stats));
  MOCK_METHOD2(DiscardAllOldPackets,
               void(uint32_t timestamp_limit, StatisticsCalculator* stats));
  MOCK_CONST_METHOD0(NumPacketsInBuffer, size_t());
  MOCK_METHOD1(IncrementWaitingTimes, void(int));
  MOCK_CONST_METHOD0(current_memory_bytes, int());
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_PACKET_BUFFER_H_
