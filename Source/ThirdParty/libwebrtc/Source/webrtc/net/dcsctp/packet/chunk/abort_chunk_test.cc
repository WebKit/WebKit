/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/abort_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "net/dcsctp/packet/error_cause/error_cause.h"
#include "net/dcsctp/packet/error_cause/user_initiated_abort_cause.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(AbortChunkTest, FromCapture) {
  /*
  ABORT chunk
      Chunk type: ABORT (6)
      Chunk flags: 0x00
      Chunk length: 8
      User initiated ABORT cause
  */

  uint8_t data[] = {0x06, 0x00, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x04};

  ASSERT_HAS_VALUE_AND_ASSIGN(AbortChunk chunk, AbortChunk::Parse(data));

  ASSERT_HAS_VALUE_AND_ASSIGN(
      UserInitiatedAbortCause cause,
      chunk.error_causes().get<UserInitiatedAbortCause>());

  EXPECT_EQ(cause.upper_layer_abort_reason(), "");
}

TEST(AbortChunkTest, SerializeAndDeserialize) {
  AbortChunk chunk(/*filled_in_verification_tag=*/true,
                   Parameters::Builder()
                       .Add(UserInitiatedAbortCause("Close called"))
                       .Build());

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(AbortChunk deserialized,
                              AbortChunk::Parse(serialized));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UserInitiatedAbortCause cause,
      deserialized.error_causes().get<UserInitiatedAbortCause>());

  EXPECT_EQ(cause.upper_layer_abort_reason(), "Close called");
}

// Validates that AbortChunk doesn't make any alignment assumptions.
TEST(AbortChunkTest, SerializeAndDeserializeOneChar) {
  AbortChunk chunk(
      /*filled_in_verification_tag=*/true,
      Parameters::Builder().Add(UserInitiatedAbortCause("!")).Build());

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(AbortChunk deserialized,
                              AbortChunk::Parse(serialized));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UserInitiatedAbortCause cause,
      deserialized.error_causes().get<UserInitiatedAbortCause>());

  EXPECT_EQ(cause.upper_layer_abort_reason(), "!");
}

}  // namespace
}  // namespace dcsctp
