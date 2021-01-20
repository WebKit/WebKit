/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_vp8.h"

#include <memory>

#include "modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr RtpPacketizer::PayloadSizeLimits kNoSizeLimits;

TEST(RtpPacketizerVp8Test, ResultPacketsAreAlmostEqualSize) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.pictureId = 200;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 12;  // Small enough to produce 4 packets.
  RtpPacketizerVp8 packetizer(helper.payload(), limits, hdr_info);

  const size_t kExpectedSizes[] = {11, 11, 12, 12};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

TEST(RtpPacketizerVp8Test, EqualSizeWithLastPacketReduction) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.pictureId = 200;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/43);

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 15;  // Small enough to produce 5 packets.
  limits.last_packet_reduction_len = 5;
  RtpPacketizerVp8 packetizer(helper.payload(), limits, hdr_info);

  // Calculated by hand. VP8 payload descriptors are 4 byte each. 5 packets is
  // minimum possible to fit 43 payload bytes into packets with capacity of
  // 15 - 4 = 11 and leave 5 free bytes in the last packet. All packets are
  // almost equal in size, even last packet if counted with free space (which
  // will be filled up the stack by extra long RTP header).
  const size_t kExpectedSizes[] = {13, 13, 14, 14, 9};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

// Verify that non-reference bit is set.
TEST(RtpPacketizerVp8Test, NonReferenceBit) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.nonReference = true;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 25;  // Small enough to produce two packets.
  RtpPacketizerVp8 packetizer(helper.payload(), limits, hdr_info);

  const size_t kExpectedSizes[] = {16, 16};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST(RtpPacketizerVp8Test, Tl0PicIdxAndTID) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.tl0PicIdx = 117;
  hdr_info.temporalIdx = 2;
  hdr_info.layerSync = true;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizerVp8 packetizer(helper.payload(), kNoSizeLimits, hdr_info);

  const size_t kExpectedSizes[1] = {helper.payload_size() + 4};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

TEST(RtpPacketizerVp8Test, KeyIdx) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.keyIdx = 17;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizerVp8 packetizer(helper.payload(), kNoSizeLimits, hdr_info);

  const size_t kExpectedSizes[1] = {helper.payload_size() + 3};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

// Verify TID field and KeyIdx field in combination.
TEST(RtpPacketizerVp8Test, TIDAndKeyIdx) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.temporalIdx = 1;
  hdr_info.keyIdx = 5;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizerVp8 packetizer(helper.payload(), kNoSizeLimits, hdr_info);

  const size_t kExpectedSizes[1] = {helper.payload_size() + 3};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

}  // namespace
}  // namespace webrtc
