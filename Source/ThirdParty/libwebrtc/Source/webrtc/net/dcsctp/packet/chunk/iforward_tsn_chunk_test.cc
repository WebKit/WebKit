/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"

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

TEST(IForwardTsnChunkTest, FromCapture) {
  /*
  I_FORWARD_TSN chunk(Cumulative TSN: 3094631148)
      Chunk type: I_FORWARD_TSN (194)
      Chunk flags: 0x00
      Chunk length: 16
      New cumulative TSN: 3094631148
      Stream identifier: 1
      Flags: 0x0000
      Message identifier: 2
  */

  uint8_t data[] = {0xc2, 0x00, 0x00, 0x10, 0xb8, 0x74, 0x52, 0xec,
                    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

  ASSERT_HAS_VALUE_AND_ASSIGN(IForwardTsnChunk chunk,
                              IForwardTsnChunk::Parse(data));
  EXPECT_EQ(*chunk.new_cumulative_tsn(), 3094631148u);
  EXPECT_THAT(chunk.skipped_streams(),
              ElementsAre(IForwardTsnChunk::SkippedStream(
                  IsUnordered(false), StreamID(1), MID(2))));
}

TEST(IForwardTsnChunkTest, SerializeAndDeserialize) {
  IForwardTsnChunk chunk(
      TSN(123), {IForwardTsnChunk::SkippedStream(IsUnordered(false),
                                                 StreamID(1), MID(23)),
                 IForwardTsnChunk::SkippedStream(IsUnordered(true),
                                                 StreamID(42), MID(99))});

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(IForwardTsnChunk deserialized,
                              IForwardTsnChunk::Parse(serialized));
  EXPECT_EQ(*deserialized.new_cumulative_tsn(), 123u);
  EXPECT_THAT(deserialized.skipped_streams(),
              ElementsAre(IForwardTsnChunk::SkippedStream(IsUnordered(false),
                                                          StreamID(1), MID(23)),
                          IForwardTsnChunk::SkippedStream(
                              IsUnordered(true), StreamID(42), MID(99))));

  EXPECT_EQ(deserialized.ToString(), "I-FORWARD-TSN, new_cumulative_tsn=123");
}

}  // namespace
}  // namespace dcsctp
