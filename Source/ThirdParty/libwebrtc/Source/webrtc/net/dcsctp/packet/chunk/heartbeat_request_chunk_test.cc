/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/heartbeat_request_chunk.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/parameter/heartbeat_info_parameter.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;

TEST(HeartbeatRequestChunkTest, FromCapture) {
  /*
  HEARTBEAT chunk (Information: 40 bytes)
      Chunk type: HEARTBEAT (4)
      Chunk flags: 0x00
      Chunk length: 44
      Heartbeat info parameter (Information: 36 bytes)
          Parameter type: Heartbeat info (0x0001)
          Parameter length: 40
          Heartbeat information: ad2436603726070000000000000000007b10000001…
  */

  uint8_t data[] = {0x04, 0x00, 0x00, 0x2c, 0x00, 0x01, 0x00, 0x28, 0xad,
                    0x24, 0x36, 0x60, 0x37, 0x26, 0x07, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0x10, 0x00,
                    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  ASSERT_HAS_VALUE_AND_ASSIGN(HeartbeatRequestChunk chunk,
                              HeartbeatRequestChunk::Parse(data));

  ASSERT_HAS_VALUE_AND_ASSIGN(HeartbeatInfoParameter info, chunk.info());

  EXPECT_THAT(
      info.info(),
      ElementsAre(0xad, 0x24, 0x36, 0x60, 0x37, 0x26, 0x07, 0x00, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0x10, 0x00, 0x00,
                  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00));
}

TEST(HeartbeatRequestChunkTest, SerializeAndDeserialize) {
  uint8_t info_data[] = {1, 2, 3, 4};
  Parameters parameters =
      Parameters::Builder().Add(HeartbeatInfoParameter(info_data)).Build();
  HeartbeatRequestChunk chunk(std::move(parameters));

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(HeartbeatRequestChunk deserialized,
                              HeartbeatRequestChunk::Parse(serialized));

  ASSERT_HAS_VALUE_AND_ASSIGN(HeartbeatInfoParameter info, deserialized.info());

  EXPECT_THAT(info.info(), ElementsAre(1, 2, 3, 4));

  EXPECT_EQ(deserialized.ToString(), "HEARTBEAT");
}

}  // namespace
}  // namespace dcsctp
