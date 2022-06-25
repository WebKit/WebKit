/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_header_extension_size.h"

#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "test/gtest.h"

namespace {

using ::webrtc::RtpExtensionSize;
using ::webrtc::RtpHeaderExtensionMap;
using ::webrtc::RtpHeaderExtensionSize;
using ::webrtc::RtpMid;
using ::webrtc::RtpStreamId;

// id for 1-byte header extension. actual value is irrelevant for these tests.
constexpr int kId = 1;
// id that forces to use 2-byte header extension.
constexpr int kIdForceTwoByteHeader = 15;

TEST(RtpHeaderExtensionSizeTest, ReturnsZeroIfNoExtensionsAreRegistered) {
  constexpr RtpExtensionSize kExtensionSizes[] = {{RtpMid::kId, 3}};
  // Register different extension than ask size for.
  RtpHeaderExtensionMap registered;
  registered.Register<RtpStreamId>(kId);

  EXPECT_EQ(RtpHeaderExtensionSize(kExtensionSizes, registered), 0);
}

TEST(RtpHeaderExtensionSizeTest, IncludesSizeOfExtensionHeaders) {
  constexpr RtpExtensionSize kExtensionSizes[] = {{RtpMid::kId, 3}};
  RtpHeaderExtensionMap registered;
  registered.Register<RtpMid>(kId);

  // 4 bytes for extension block header + 1 byte for individual extension header
  // + 3 bytes for the value.
  EXPECT_EQ(RtpHeaderExtensionSize(kExtensionSizes, registered), 8);
}

TEST(RtpHeaderExtensionSizeTest, RoundsUpTo32bitAlignmant) {
  constexpr RtpExtensionSize kExtensionSizes[] = {{RtpMid::kId, 5}};
  RtpHeaderExtensionMap registered;
  registered.Register<RtpMid>(kId);

  // 10 bytes of data including headers + 2 bytes of padding.
  EXPECT_EQ(RtpHeaderExtensionSize(kExtensionSizes, registered), 12);
}

TEST(RtpHeaderExtensionSizeTest, SumsSeveralExtensions) {
  constexpr RtpExtensionSize kExtensionSizes[] = {{RtpMid::kId, 16},
                                                  {RtpStreamId::kId, 2}};
  RtpHeaderExtensionMap registered;
  registered.Register<RtpMid>(kId);
  registered.Register<RtpStreamId>(14);

  // 4 bytes for extension block header + 18 bytes of value +
  // 2 bytes for two headers
  EXPECT_EQ(RtpHeaderExtensionSize(kExtensionSizes, registered), 24);
}

TEST(RtpHeaderExtensionSizeTest, LargeIdForce2BytesHeader) {
  constexpr RtpExtensionSize kExtensionSizes[] = {{RtpMid::kId, 3},
                                                  {RtpStreamId::kId, 2}};
  RtpHeaderExtensionMap registered;
  registered.Register<RtpMid>(kId);
  registered.Register<RtpStreamId>(kIdForceTwoByteHeader);

  // 4 bytes for extension block header + 5 bytes of value +
  // 2*2 bytes for two headers + 3 bytes of padding.
  EXPECT_EQ(RtpHeaderExtensionSize(kExtensionSizes, registered), 16);
}

TEST(RtpHeaderExtensionSizeTest, LargeValueForce2BytesHeader) {
  constexpr RtpExtensionSize kExtensionSizes[] = {{RtpMid::kId, 17},
                                                  {RtpStreamId::kId, 4}};
  RtpHeaderExtensionMap registered;
  registered.Register<RtpMid>(1);
  registered.Register<RtpStreamId>(2);

  // 4 bytes for extension block header + 21 bytes of value +
  // 2*2 bytes for two headers + 3 byte of padding.
  EXPECT_EQ(RtpHeaderExtensionSize(kExtensionSizes, registered), 32);
}

}  // namespace
