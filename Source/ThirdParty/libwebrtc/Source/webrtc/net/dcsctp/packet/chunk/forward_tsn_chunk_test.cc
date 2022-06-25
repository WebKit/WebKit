/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(ForwardTsnChunkTest, FromCapture) {
  /*
  FORWARD_TSN chunk(Cumulative TSN: 1905748778)
      Chunk type: FORWARD_TSN (192)
      Chunk flags: 0x00
      Chunk length: 8
      New cumulative TSN: 1905748778
  */

  uint8_t data[] = {0xc0, 0x00, 0x00, 0x08, 0x71, 0x97, 0x6b, 0x2a};

  ASSERT_HAS_VALUE_AND_ASSIGN(ForwardTsnChunk chunk,
                              ForwardTsnChunk::Parse(data));
  EXPECT_EQ(*chunk.new_cumulative_tsn(), 1905748778u);
}

TEST(ForwardTsnChunkTest, SerializeAndDeserialize) {
  ForwardTsnChunk chunk(
      TSN(123), {ForwardTsnChunk::SkippedStream(StreamID(1), SSN(23)),
                 ForwardTsnChunk::SkippedStream(StreamID(42), SSN(99))});

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(ForwardTsnChunk deserialized,
                              ForwardTsnChunk::Parse(serialized));
  EXPECT_EQ(*deserialized.new_cumulative_tsn(), 123u);
  EXPECT_THAT(
      deserialized.skipped_streams(),
      ElementsAre(ForwardTsnChunk::SkippedStream(StreamID(1), SSN(23)),
                  ForwardTsnChunk::SkippedStream(StreamID(42), SSN(99))));

  EXPECT_EQ(deserialized.ToString(), "FORWARD-TSN, new_cumulative_tsn=123");
}

}  // namespace
}  // namespace dcsctp
