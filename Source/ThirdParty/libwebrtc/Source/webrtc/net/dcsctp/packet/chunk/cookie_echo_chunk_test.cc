/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/cookie_echo_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(CookieEchoChunkTest, FromCapture) {
  /*
  COOKIE_ECHO chunk (Cookie length: 256 bytes)
      Chunk type: COOKIE_ECHO (10)
      Chunk flags: 0x00
      Chunk length: 260
      Cookie: 12345678
  */

  uint8_t data[] = {0x0a, 0x00, 0x00, 0x08, 0x12, 0x34, 0x56, 0x78};

  ASSERT_HAS_VALUE_AND_ASSIGN(CookieEchoChunk chunk,
                              CookieEchoChunk::Parse(data));

  EXPECT_THAT(chunk.cookie(), ElementsAre(0x12, 0x34, 0x56, 0x78));
}

TEST(CookieEchoChunkTest, SerializeAndDeserialize) {
  uint8_t cookie[] = {1, 2, 3, 4};
  CookieEchoChunk chunk(cookie);

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(CookieEchoChunk deserialized,
                              CookieEchoChunk::Parse(serialized));

  EXPECT_THAT(deserialized.cookie(), ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(deserialized.ToString(), "COOKIE-ECHO");
}

}  // namespace
}  // namespace dcsctp
