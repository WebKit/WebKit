/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_parameters.h"

#include "test/gtest.h"

namespace webrtc {

using webrtc::RtpExtension;

static const char kExtensionUri1[] = "extension-uri1";
static const char kExtensionUri2[] = "extension-uri2";

static const RtpExtension kExtension1(kExtensionUri1, 1);
static const RtpExtension kExtension1Encrypted(kExtensionUri1, 10, true);
static const RtpExtension kExtension2(kExtensionUri2, 2);

TEST(RtpExtensionTest, FilterDuplicateNonEncrypted) {
  std::vector<RtpExtension> extensions;
  std::vector<RtpExtension> filtered;

  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  filtered = RtpExtension::FilterDuplicateNonEncrypted(extensions);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1Encrypted}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  filtered = RtpExtension::FilterDuplicateNonEncrypted(extensions);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1Encrypted}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension2);
  filtered = RtpExtension::FilterDuplicateNonEncrypted(extensions);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ(extensions, filtered);
}
}  // namespace webrtc
