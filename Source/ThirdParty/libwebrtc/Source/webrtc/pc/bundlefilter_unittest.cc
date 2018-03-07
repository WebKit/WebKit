/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/bundlefilter.h"
#include "rtc_base/gunit.h"

using cricket::StreamParams;

static const int kPayloadType1 = 0x11;
static const int kPayloadType2 = 0x22;
static const int kPayloadType3 = 0x33;

// SSRC = 0x1111, Payload type = 0x11
static const unsigned char kRtpPacketPt1Ssrc1[] = {
    0x80, kPayloadType1, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x11,
};

// SSRC = 0x2222, Payload type = 0x22
static const unsigned char kRtpPacketPt2Ssrc2[] = {
    0x80, 0x80 + kPayloadType2, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22,
};

// SSRC = 0x2222, Payload type = 0x33
static const unsigned char kRtpPacketPt3Ssrc2[] = {
    0x80, kPayloadType3, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22,
    0x22,
};

// An SCTP packet.
static const unsigned char kSctpPacket[] = {
    0x00, 0x01, 0x00, 0x01,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00,
};

TEST(BundleFilterTest, RtpPacketTest) {
  cricket::BundleFilter bundle_filter;
  bundle_filter.AddPayloadType(kPayloadType1);
  EXPECT_TRUE(bundle_filter.DemuxPacket(kRtpPacketPt1Ssrc1,
                                        sizeof(kRtpPacketPt1Ssrc1)));
  bundle_filter.AddPayloadType(kPayloadType2);
  EXPECT_TRUE(bundle_filter.DemuxPacket(kRtpPacketPt2Ssrc2,
                                        sizeof(kRtpPacketPt2Ssrc2)));

  // Payload type 0x33 is not added.
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt3Ssrc2,
                                         sizeof(kRtpPacketPt3Ssrc2)));
  // Size is too small.
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt1Ssrc1, 11));

  bundle_filter.ClearAllPayloadTypes();
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt1Ssrc1,
                                         sizeof(kRtpPacketPt1Ssrc1)));
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt2Ssrc2,
                                         sizeof(kRtpPacketPt2Ssrc2)));
}

TEST(BundleFilterTest, InvalidRtpPacket) {
  cricket::BundleFilter bundle_filter;
  EXPECT_FALSE(bundle_filter.DemuxPacket(kSctpPacket, sizeof(kSctpPacket)));
}
