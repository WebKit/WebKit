/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/base64.h"

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::TestWithParam;

TEST(Base64Test, Encode) {
  std::string data{0x64, 0x65, 0x66};
  EXPECT_THAT(Base64Encode(data), Eq("ZGVm"));
}

TEST(Base64Test, EncodeDecode) {
  std::string data{0x01, 0x02, 0x03, 0x04, 0x05};
  EXPECT_THAT(Base64Decode(Base64Encode(data)), Optional(Eq(data)));
}

TEST(Base64Test, DecodeCertificate) {
  // Certificate data often contains newlines, which are not valid base64
  // characters but parsable using the forgiving option.
  constexpr absl::string_view kExampleCertificateData =
      "MIIB6TCCAVICAQYwDQYJKoZIhvcNAQEEBQAwWzELMAkGA1UEBhMCQVUxEzARBgNV\n"
      "BAgTClF1ZWVuc2xhbmQxGjAYBgNVBAoTEUNyeXB0U29mdCBQdHkgTHRkMRswGQYD\n"
      "VQQDExJUZXN0IENBICgxMDI0IGJpdCkwHhcNMDAxMDE2MjIzMTAzWhcNMDMwMTE0\n"
      "MjIzMTAzWjBjMQswCQYDVQQGEwJBVTETMBEGA1UECBMKUXVlZW5zbGFuZDEaMBgG\n"
      "A1UEChMRQ3J5cHRTb2Z0IFB0eSBMdGQxIzAhBgNVBAMTGlNlcnZlciB0ZXN0IGNl\n"
      "cnQgKDUxMiBiaXQpMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAJ+zw4Qnlf8SMVIP\n"
      "Fe9GEcStgOY2Ww/dgNdhjeD8ckUJNP5VZkVDTGiXav6ooKXfX3j/7tdkuD8Ey2//\n"
      "Kv7+ue0CAwEAATANBgkqhkiG9w0BAQQFAAOBgQCT0grFQeZaqYb5EYfk20XixZV4\n"
      "GmyAbXMftG1Eo7qGiMhYzRwGNWxEYojf5PZkYZXvSqZ/ZXHXa4g59jK/rJNnaVGM\n"
      "k+xIX8mxQvlV0n5O9PIha5BX5teZnkHKgL8aKKLKW1BK7YTngsfSzzaeame5iKfz\n"
      "itAE+OjGF+PFKbwX8Q==\n";

  EXPECT_THAT(
      Base64Decode(kExampleCertificateData, Base64DecodeOptions::kForgiving),
      Optional(SizeIs(493)));
  EXPECT_THAT(
      Base64Decode(kExampleCertificateData, Base64DecodeOptions::kStrict),
      Eq(std::nullopt));
}

struct Base64DecodeTestCase {
  std::string name;
  std::string data;
  std::optional<std::string> result;
};

const Base64DecodeTestCase kBase64DecodeTestCases[] = {
    {"InvalidCharacters", "invalid;;;", std::nullopt},
    {"InvalidLength", "abcde", std::nullopt},
    {"ValidInput", "abcd", "i\xB7\x1D"},
    {"ValidInputPadding", "abc=", "i\xB7"},
    {"EmptyInput", "", ""},
};

using Base64DecodeTest = TestWithParam<Base64DecodeTestCase>;
INSTANTIATE_TEST_SUITE_P(
    Base64DecodeTest,
    Base64DecodeTest,
    testing::ValuesIn<Base64DecodeTestCase>(kBase64DecodeTestCases),
    [](const auto& info) { return info.param.name; });

TEST_P(Base64DecodeTest, TestDecodeStrict) {
  absl::string_view data = GetParam().data;
  EXPECT_THAT(Base64Decode(data, Base64DecodeOptions::kStrict),
              Eq(GetParam().result));
}

TEST_P(Base64DecodeTest, TestDecodeForgiving) {
  // Test default value is strict.
  EXPECT_THAT(Base64Decode(GetParam().data), Eq(GetParam().result));
}

const Base64DecodeTestCase kBase64DecodeForgivingTestCases[] = {
    {
        "ForgivingPadding",
        "abc",
        "i\xB7",
    },
    {
        "WhitespaceForgivenTab",
        "ab\tcd",
        "i\xB7\x1D",
    },
    {
        "WhitespaceForgivenSpace",
        "a bc d",
        "i\xB7\x1D",
    },
    {
        "WhitespaceForgivenNewline",
        "a\nbc\nd",
        "i\xB7\x1D",
    },
    {
        "WhitespaceForgivenCarriageReturn",
        "a\r\nbc\rd",
        "i\xB7\x1D",
    },
    {"WhitespaceForgivenLineFeed", "a\fbcd", "i\xB7\x1D"},
};

using Base64DecodeForgivingTest = TestWithParam<Base64DecodeTestCase>;
INSTANTIATE_TEST_SUITE_P(
    Base64DecodeTest,
    Base64DecodeForgivingTest,
    testing::ValuesIn<Base64DecodeTestCase>(kBase64DecodeForgivingTestCases),
    [](const auto& info) { return info.param.name; });

TEST_P(Base64DecodeForgivingTest, TestDecodeForgiving) {
  EXPECT_THAT(Base64Decode(GetParam().data, Base64DecodeOptions::kForgiving),
              Eq(GetParam().result));
}

TEST_P(Base64DecodeForgivingTest, TestDecodeStrictFails) {
  // Test default value is strict.
  EXPECT_THAT(Base64Decode(GetParam().data), Eq(std::nullopt));
}

}  // namespace
}  // namespace webrtc
