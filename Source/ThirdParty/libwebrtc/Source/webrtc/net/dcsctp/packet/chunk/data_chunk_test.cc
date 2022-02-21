/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/data_chunk.h"

#include <cstdint>
#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(DataChunkTest, FromCapture) {
  /*
  DATA chunk(ordered, complete segment, TSN: 1426601532, SID: 2, SSN: 1,
    PPID: 53, payload length: 4 bytes)
      Chunk type: DATA (0)
      Chunk flags: 0x03
      Chunk length: 20
      Transmission sequence number: 1426601532
      Stream identifier: 0x0002
      Stream sequence number: 1
      Payload protocol identifier: WebRTC Binary (53)
  */

  uint8_t data[] = {0x00, 0x03, 0x00, 0x14, 0x55, 0x08, 0x36, 0x3c, 0x00, 0x02,
                    0x00, 0x01, 0x00, 0x00, 0x00, 0x35, 0x00, 0x01, 0x02, 0x03};

  ASSERT_HAS_VALUE_AND_ASSIGN(DataChunk chunk, DataChunk::Parse(data));
  EXPECT_EQ(*chunk.tsn(), 1426601532u);
  EXPECT_EQ(*chunk.stream_id(), 2u);
  EXPECT_EQ(*chunk.ssn(), 1u);
  EXPECT_EQ(*chunk.ppid(), 53u);
  EXPECT_TRUE(*chunk.options().is_beginning);
  EXPECT_TRUE(*chunk.options().is_end);
  EXPECT_FALSE(*chunk.options().is_unordered);
  EXPECT_FALSE(*chunk.options().immediate_ack);
  EXPECT_THAT(chunk.payload(), ElementsAre(0x0, 0x1, 0x2, 0x3));
}

TEST(DataChunkTest, SerializeAndDeserialize) {
  DataChunk chunk(TSN(123), StreamID(456), SSN(789), PPID(9090),
                  /*payload=*/{1, 2, 3, 4, 5},
                  /*options=*/{});

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(DataChunk deserialized,
                              DataChunk::Parse(serialized));
  EXPECT_EQ(*chunk.tsn(), 123u);
  EXPECT_EQ(*chunk.stream_id(), 456u);
  EXPECT_EQ(*chunk.ssn(), 789u);
  EXPECT_EQ(*chunk.ppid(), 9090u);
  EXPECT_THAT(chunk.payload(), ElementsAre(1, 2, 3, 4, 5));

  EXPECT_EQ(deserialized.ToString(),
            "DATA, type=ordered::middle, tsn=123, sid=456, ssn=789, ppid=9090, "
            "length=5");
}
}  // namespace
}  // namespace dcsctp
