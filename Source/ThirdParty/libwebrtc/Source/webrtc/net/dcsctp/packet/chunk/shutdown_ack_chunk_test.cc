/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/shutdown_ack_chunk.h"

#include <stdint.h>

#include <vector>

#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(ShutdownAckChunkTest, FromCapture) {
  /*
  SHUTDOWN_ACK chunk
      Chunk type: SHUTDOWN_ACK (8)
      Chunk flags: 0x00
      Chunk length: 4
  */

  uint8_t data[] = {0x08, 0x00, 0x00, 0x04};

  EXPECT_TRUE(ShutdownAckChunk::Parse(data).has_value());
}

TEST(ShutdownAckChunkTest, SerializeAndDeserialize) {
  ShutdownAckChunk chunk;

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  EXPECT_TRUE(ShutdownAckChunk::Parse(serialized).has_value());
}

}  // namespace
}  // namespace dcsctp
