/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/shutdown_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"

namespace dcsctp {
namespace {
TEST(ShutdownChunkTest, FromCapture) {
  /*
  SHUTDOWN chunk (Cumulative TSN ack: 101831101)
      Chunk type: SHUTDOWN (7)
      Chunk flags: 0x00
      Chunk length: 8
      Cumulative TSN Ack: 101831101
  */

  uint8_t data[] = {0x07, 0x00, 0x00, 0x08, 0x06, 0x11, 0xd1, 0xbd};

  ASSERT_HAS_VALUE_AND_ASSIGN(ShutdownChunk chunk, ShutdownChunk::Parse(data));
  EXPECT_EQ(chunk.cumulative_tsn_ack(), TSN(101831101u));
}

TEST(ShutdownChunkTest, SerializeAndDeserialize) {
  ShutdownChunk chunk(TSN(12345678));

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(ShutdownChunk deserialized,
                              ShutdownChunk::Parse(serialized));

  EXPECT_EQ(deserialized.cumulative_tsn_ack(), TSN(12345678u));
}

}  // namespace
}  // namespace dcsctp
