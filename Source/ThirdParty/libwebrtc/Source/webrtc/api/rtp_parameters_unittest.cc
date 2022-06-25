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

TEST(RtpExtensionTest, DeduplicateHeaderExtensions) {
  std::vector<RtpExtension> extensions;
  std::vector<RtpExtension> filtered;

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kDiscardEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kPreferEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1Encrypted}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kRequireEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1Encrypted}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kDiscardEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kPreferEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1Encrypted}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kRequireEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ(std::vector<RtpExtension>{kExtension1Encrypted}, filtered);

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension2);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kDiscardEncryptedExtension);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ(extensions, filtered);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kPreferEncryptedExtension);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ(extensions, filtered);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kRequireEncryptedExtension);
  EXPECT_EQ(0u, filtered.size());

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension2);
  extensions.push_back(kExtension1Encrypted);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kDiscardEncryptedExtension);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ((std::vector<RtpExtension>{kExtension1, kExtension2}), filtered);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kPreferEncryptedExtension);
  EXPECT_EQ(2u, filtered.size());
  EXPECT_EQ((std::vector<RtpExtension>{kExtension1Encrypted, kExtension2}),
            filtered);
  filtered = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kRequireEncryptedExtension);
  EXPECT_EQ(1u, filtered.size());
  EXPECT_EQ((std::vector<RtpExtension>{kExtension1Encrypted}), filtered);
}

TEST(RtpExtensionTest, FindHeaderExtensionByUriAndEncryption) {
  std::vector<RtpExtension> extensions;

  extensions.clear();
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUriAndEncryption(
                         extensions, kExtensionUri1, false));

  extensions.clear();
  extensions.push_back(kExtension1);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUriAndEncryption(
                             extensions, kExtensionUri1, false));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUriAndEncryption(
                         extensions, kExtensionUri1, true));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUriAndEncryption(
                         extensions, kExtensionUri2, false));

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension2);
  extensions.push_back(kExtension1Encrypted);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUriAndEncryption(
                             extensions, kExtensionUri1, false));
  EXPECT_EQ(kExtension2, *RtpExtension::FindHeaderExtensionByUriAndEncryption(
                             extensions, kExtensionUri2, false));
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUriAndEncryption(
                extensions, kExtensionUri1, true));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUriAndEncryption(
                         extensions, kExtensionUri2, true));
}

TEST(RtpExtensionTest, FindHeaderExtensionByUri) {
  std::vector<RtpExtension> extensions;

  extensions.clear();
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri1,
                         RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri1,
                         RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri1,
                         RtpExtension::Filter::kRequireEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri1,
                         RtpExtension::Filter::kRequireEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri2,
                         RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri2,
                         RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri2,
                         RtpExtension::Filter::kRequireEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kDiscardEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUri(
                extensions, kExtensionUri1,
                RtpExtension::Filter::kPreferEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension1Encrypted);
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUri(
                extensions, kExtensionUri1,
                RtpExtension::Filter::kRequireEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kDiscardEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUri(
                extensions, kExtensionUri1,
                RtpExtension::Filter::kPreferEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1Encrypted);
  extensions.push_back(kExtension1);
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUri(
                extensions, kExtensionUri1,
                RtpExtension::Filter::kRequireEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension2);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri1,
                         RtpExtension::Filter::kRequireEncryptedExtension));
  EXPECT_EQ(kExtension2, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri2,
                             RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(kExtension2, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri2,
                             RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri2,
                         RtpExtension::Filter::kRequireEncryptedExtension));

  extensions.clear();
  extensions.push_back(kExtension1);
  extensions.push_back(kExtension2);
  extensions.push_back(kExtension1Encrypted);
  EXPECT_EQ(kExtension1, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri1,
                             RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUri(
                extensions, kExtensionUri1,
                RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(kExtension1Encrypted,
            *RtpExtension::FindHeaderExtensionByUri(
                extensions, kExtensionUri1,
                RtpExtension::Filter::kRequireEncryptedExtension));
  EXPECT_EQ(kExtension2, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri2,
                             RtpExtension::Filter::kDiscardEncryptedExtension));
  EXPECT_EQ(kExtension2, *RtpExtension::FindHeaderExtensionByUri(
                             extensions, kExtensionUri2,
                             RtpExtension::Filter::kPreferEncryptedExtension));
  EXPECT_EQ(nullptr, RtpExtension::FindHeaderExtensionByUri(
                         extensions, kExtensionUri2,
                         RtpExtension::Filter::kRequireEncryptedExtension));
}
}  // namespace webrtc
