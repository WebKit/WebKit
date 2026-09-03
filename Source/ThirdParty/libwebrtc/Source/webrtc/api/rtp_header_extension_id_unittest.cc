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

#include "test/gtest.h"

namespace webrtc {
namespace {

enum Unscoped { kUnscopedVal = 1 };
enum class Scoped { kScopedVal = 1 };

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
  RtpHeaderExtensionId t4(Scoped::kScopedVal);  // Explicit scoped enum
  ConvertTo c;
  RtpHeaderExtensionId t5(c);  // Explicit other convertible

  (void)t1;
  (void)t2;
  (void)t3;
  (void)t4;
  (void)t5;

  // These should compile when deprecation warnings are ignored.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  RtpHeaderExtensionId t6 = 5;             // Implicit int
  RtpHeaderExtensionId t7 = kUnscopedVal;  // Implicit unscoped enum
  RtpHeaderExtensionId t8 = c;             // Implicit other convertible
#pragma clang diagnostic pop
  (void)t6;
  (void)t7;
  (void)t8;
}

}  // namespace
}  // namespace webrtc
