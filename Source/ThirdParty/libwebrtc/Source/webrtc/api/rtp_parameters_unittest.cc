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

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
RtpParameters CreateRtpParametersWithCodecs(
    const std::vector<bool>& active,
    const std::vector<std::optional<RtpCodec>>& codecs) {
  RTC_DCHECK_EQ(active.size(), codecs.size());

  RtpParameters parameters;
  for (size_t i = 0; i < codecs.size(); ++i) {
    RtpEncodingParameters encoding;
    encoding.active = active[i];
    encoding.codec = codecs[i];
    parameters.encodings.push_back(encoding);
  }
  return parameters;
}
}  // namespace

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

// Test that the filtered vector is sorted so that for a given unsorted array of
// extensions, the filtered vector will always be laied out the same (for easy
// comparison).
TEST(RtpExtensionTest, DeduplicateHeaderExtensionsSorted) {
  const std::vector<RtpExtension> extensions = {
      RtpExtension("cde1", 11, false), RtpExtension("cde2", 12, true),
      RtpExtension("abc1", 3, false),  RtpExtension("abc2", 4, true),
      RtpExtension("cde3", 9, true),   RtpExtension("cde4", 10, false),
      RtpExtension("abc3", 1, true),   RtpExtension("abc4", 2, false),
      RtpExtension("bcd3", 7, false),  RtpExtension("bcd1", 8, true),
      RtpExtension("bcd2", 5, true),   RtpExtension("bcd4", 6, false),
  };

  auto encrypted = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kRequireEncryptedExtension);

  const std::vector<RtpExtension> expected_sorted_encrypted = {
      RtpExtension("abc2", 4, true),  RtpExtension("abc3", 1, true),
      RtpExtension("bcd1", 8, true),  RtpExtension("bcd2", 5, true),
      RtpExtension("cde2", 12, true), RtpExtension("cde3", 9, true)};
  EXPECT_EQ(expected_sorted_encrypted, encrypted);

  auto unencypted = RtpExtension::DeduplicateHeaderExtensions(
      extensions, RtpExtension::Filter::kDiscardEncryptedExtension);

  const std::vector<RtpExtension> expected_sorted_unencrypted = {
      RtpExtension("abc1", 3, false),  RtpExtension("abc4", 2, false),
      RtpExtension("bcd3", 7, false),  RtpExtension("bcd4", 6, false),
      RtpExtension("cde1", 11, false), RtpExtension("cde4", 10, false)};
  EXPECT_EQ(expected_sorted_unencrypted, unencypted);
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

TEST(CodecParameterMap, ParsesKeyValueFmtpParameterSet) {
  std::string params = "key1=value1;key2=value2";
  CodecParameterMap codec_params;
  ASSERT_TRUE(ParseFmtpParameterSet(params, codec_params).ok());
  EXPECT_EQ(2U, codec_params.size());
  EXPECT_EQ(codec_params["key1"], "value1");
  EXPECT_EQ(codec_params["key2"], "value2");
}

TEST(CodecParameterMap, ParsesNonKeyValueFmtpParameterSet) {
  std::string params = "not-in-key-value-format";
  CodecParameterMap codec_params;
  ASSERT_TRUE(ParseFmtpParameterSet(params, codec_params).ok());
  EXPECT_EQ(1U, codec_params.size());
  EXPECT_EQ(codec_params[""], "not-in-key-value-format");
}

TEST(RtpParametersTest, IsMixedCodec) {
  RtpParameters parameters;
  RtpCodec codec1, codec2;
  codec1.name = "codec1";
  codec2.name = "codec2";

  parameters = CreateRtpParametersWithCodecs({}, {});
  EXPECT_FALSE(parameters.IsMixedCodec());

  parameters = CreateRtpParametersWithCodecs({true}, {codec1});
  EXPECT_FALSE(parameters.IsMixedCodec());

  parameters = CreateRtpParametersWithCodecs({true}, {std::nullopt});
  EXPECT_FALSE(parameters.IsMixedCodec());

  parameters = CreateRtpParametersWithCodecs({true, true}, {codec1, codec2});
  EXPECT_TRUE(parameters.IsMixedCodec());

  // Inactive encoding parameters are ignored.
  parameters = CreateRtpParametersWithCodecs({false, true}, {codec1, codec2});
  EXPECT_FALSE(parameters.IsMixedCodec());

  // Even if some codecs are nullopt, differing codec presence/values
  // among active encodings is considered mixed.
  parameters =
      CreateRtpParametersWithCodecs({true, true}, {std::nullopt, codec2});
  EXPECT_TRUE(parameters.IsMixedCodec());

  parameters =
      CreateRtpParametersWithCodecs({true, true}, {std::nullopt, std::nullopt});
  EXPECT_FALSE(parameters.IsMixedCodec());

  parameters = CreateRtpParametersWithCodecs({true, true, true},
                                             {std::nullopt, codec1, codec2});
  EXPECT_TRUE(parameters.IsMixedCodec());
}

}  // namespace webrtc
