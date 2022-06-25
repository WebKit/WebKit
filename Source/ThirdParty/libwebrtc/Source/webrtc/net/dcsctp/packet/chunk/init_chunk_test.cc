/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/init_chunk.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "net/dcsctp/packet/parameter/forward_tsn_supported_parameter.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/parameter/supported_extensions_parameter.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(InitChunkTest, FromCapture) {
  /*
  INIT chunk (Outbound streams: 1000, inbound streams: 1000)
      Chunk type: INIT (1)
      Chunk flags: 0x00
      Chunk length: 90
      Initiate tag: 0xde7a1690
      Advertised receiver window credit (a_rwnd): 131072
      Number of outbound streams: 1000
      Number of inbound streams: 1000
      Initial TSN: 621623272
      ECN parameter
          Parameter type: ECN (0x8000)
          Parameter length: 4
      Forward TSN supported parameter
          Parameter type: Forward TSN supported (0xc000)
          Parameter length: 4
      Supported Extensions parameter (Supported types: FORWARD_TSN, AUTH,
  ASCONF, ASCONF_ACK, RE_CONFIG) Parameter type: Supported Extensions (0x8008)
          Parameter length: 9
          Supported chunk type: FORWARD_TSN (192)
          Supported chunk type: AUTH (15)
          Supported chunk type: ASCONF (193)
          Supported chunk type: ASCONF_ACK (128)
          Supported chunk type: RE_CONFIG (130)
          Parameter padding: 000000
      Random parameter
          Parameter type: Random (0x8002)
          Parameter length: 36
          Random number: ab314462121a1513fd5a5f69efaa06e9abd748cc3bd14b60…
      Requested HMAC Algorithm parameter (Supported HMACs: SHA-1)
          Parameter type: Requested HMAC Algorithm (0x8004)
          Parameter length: 6
          HMAC identifier: SHA-1 (1)
          Parameter padding: 0000
      Authenticated Chunk list parameter (Chunk types to be authenticated:
  ASCONF_ACK, ASCONF) Parameter type: Authenticated Chunk list (0x8003)
          Parameter length: 6
          Chunk type: ASCONF_ACK (128)
          Chunk type: ASCONF (193)
  */

  uint8_t data[] = {
      0x01, 0x00, 0x00, 0x5a, 0xde, 0x7a, 0x16, 0x90, 0x00, 0x02, 0x00, 0x00,
      0x03, 0xe8, 0x03, 0xe8, 0x25, 0x0d, 0x37, 0xe8, 0x80, 0x00, 0x00, 0x04,
      0xc0, 0x00, 0x00, 0x04, 0x80, 0x08, 0x00, 0x09, 0xc0, 0x0f, 0xc1, 0x80,
      0x82, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x24, 0xab, 0x31, 0x44, 0x62,
      0x12, 0x1a, 0x15, 0x13, 0xfd, 0x5a, 0x5f, 0x69, 0xef, 0xaa, 0x06, 0xe9,
      0xab, 0xd7, 0x48, 0xcc, 0x3b, 0xd1, 0x4b, 0x60, 0xed, 0x7f, 0xa6, 0x44,
      0xce, 0x4d, 0xd2, 0xad, 0x80, 0x04, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00,
      0x80, 0x03, 0x00, 0x06, 0x80, 0xc1, 0x00, 0x00};

  ASSERT_HAS_VALUE_AND_ASSIGN(InitChunk chunk, InitChunk::Parse(data));

  EXPECT_EQ(chunk.initiate_tag(), VerificationTag(0xde7a1690));
  EXPECT_EQ(chunk.a_rwnd(), 131072u);
  EXPECT_EQ(chunk.nbr_outbound_streams(), 1000u);
  EXPECT_EQ(chunk.nbr_inbound_streams(), 1000u);
  EXPECT_EQ(chunk.initial_tsn(), TSN(621623272u));
  EXPECT_TRUE(
      chunk.parameters().get<ForwardTsnSupportedParameter>().has_value());
  EXPECT_TRUE(
      chunk.parameters().get<SupportedExtensionsParameter>().has_value());
}

TEST(InitChunkTest, SerializeAndDeserialize) {
  InitChunk chunk(VerificationTag(123), /*a_rwnd=*/456,
                  /*nbr_outbound_streams=*/65535,
                  /*nbr_inbound_streams=*/65534, /*initial_tsn=*/TSN(789),
                  /*parameters=*/Parameters::Builder().Build());

  std::vector<uint8_t> serialized;
  chunk.SerializeTo(serialized);

  ASSERT_HAS_VALUE_AND_ASSIGN(InitChunk deserialized,
                              InitChunk::Parse(serialized));

  EXPECT_EQ(deserialized.initiate_tag(), VerificationTag(123u));
  EXPECT_EQ(deserialized.a_rwnd(), 456u);
  EXPECT_EQ(deserialized.nbr_outbound_streams(), 65535u);
  EXPECT_EQ(deserialized.nbr_inbound_streams(), 65534u);
  EXPECT_EQ(deserialized.initial_tsn(), TSN(789u));
  EXPECT_EQ(deserialized.ToString(),
            "INIT, initiate_tag=0x7b, initial_tsn=789");
}
}  // namespace
}  // namespace dcsctp
