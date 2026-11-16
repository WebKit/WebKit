/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_header_extension_id.h"

#include <cstdint>
#include <optional>

#include "test/gtest.h"

namespace webrtc {
namespace {

enum Unscoped { kUnscopedVal = 1 };

class ConvertTo {
 public:
  operator int() const { return 5; }
};

TEST(RtpHeaderExtensionId, AppropriateConstructorsChosen) {
  // All these should compile without warnings (errors).
  RtpHeaderExtensionId value;  // default constructor
  (void)value;
  RtpHeaderExtensionId t1(5);                   // Explicit int
  RtpHeaderExtensionId t2(uint8_t{5});          // Explicit uint8_t
  RtpHeaderExtensionId t3(kUnscopedVal);        // Explicit unscoped enum
  ConvertTo c;
  RtpHeaderExtensionId t4(c);  // Explicit other convertible

  (void)t1;
  (void)t2;
  (void)t3;
  (void)t4;
}

TEST(RtpHeaderExtensionId, Create) {
  EXPECT_EQ(RtpHeaderExtensionId::Create(-1), std::nullopt);
  EXPECT_EQ(RtpHeaderExtensionId::Create(0), std::nullopt);

  EXPECT_EQ(RtpHeaderExtensionId::Create(1), RtpHeaderExtensionId(1));
  EXPECT_EQ(RtpHeaderExtensionId::Create(15), RtpHeaderExtensionId(15));
  EXPECT_EQ(RtpHeaderExtensionId::Create(255), RtpHeaderExtensionId(255));

  EXPECT_EQ(RtpHeaderExtensionId::Create(256), std::nullopt);
  EXPECT_EQ(RtpHeaderExtensionId::Create(257), std::nullopt);
}

}  // namespace
}  // namespace webrtc
