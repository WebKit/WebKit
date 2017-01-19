/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/webrtcmediaengine.h"
#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.h"
#include "webrtc/test/gtest.h"

using webrtc::RtpExtension;

namespace cricket {
namespace {

std::vector<RtpExtension> MakeUniqueExtensions() {
  std::vector<RtpExtension> result;
  char name[] = "a";
  for (int i = 0; i < 7; ++i) {
    result.push_back(RtpExtension(name, 1 + i));
    name[0]++;
    result.push_back(RtpExtension(name, 14 - i));
    name[0]++;
  }
  return result;
}

std::vector<RtpExtension> MakeRedundantExtensions() {
  std::vector<RtpExtension> result;
  char name[] = "a";
  for (int i = 0; i < 7; ++i) {
    result.push_back(RtpExtension(name, 1 + i));
    result.push_back(RtpExtension(name, 14 - i));
    name[0]++;
  }
  return result;
}

bool SupportedExtensions1(const std::string& name) {
  return name == "c" || name == "i";
}

bool SupportedExtensions2(const std::string& name) {
  return name != "a" && name != "n";
}

bool IsSorted(const std::vector<webrtc::RtpExtension>& extensions) {
  const std::string* last = nullptr;
  for (const auto& extension : extensions) {
    if (last && *last > extension.uri) {
      return false;
    }
    last = &extension.uri;
  }
  return true;
}
}  // namespace

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_EmptyList) {
  std::vector<RtpExtension> extensions;
  EXPECT_TRUE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_AllGood) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  EXPECT_TRUE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OutOfRangeId_Low) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 0));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OutOfRangeId_High) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 15));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OverlappingIds_StartOfSet) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 1));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, ValidateRtpExtensions_OverlappingIds_EndOfSet) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  extensions.push_back(RtpExtension("foo", 14));
  EXPECT_FALSE(ValidateRtpExtensions(extensions));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_EmptyList) {
  std::vector<RtpExtension> extensions;
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions1, true);
  EXPECT_EQ(0, filtered.size());
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_IncludeOnlySupported) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions1, false);
  EXPECT_EQ(2, filtered.size());
  EXPECT_EQ("c", filtered[0].uri);
  EXPECT_EQ("i", filtered[1].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_SortedByName_1) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, false);
  EXPECT_EQ(12, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_SortedByName_2) {
  std::vector<RtpExtension> extensions = MakeUniqueExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(12, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_DontRemoveRedundant) {
  std::vector<RtpExtension> extensions = MakeRedundantExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, false);
  EXPECT_EQ(12, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_EQ(filtered[0].uri, filtered[1].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundant) {
  std::vector<RtpExtension> extensions = MakeRedundantExtensions();
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(6, filtered.size());
  EXPECT_TRUE(IsSorted(filtered));
  EXPECT_NE(filtered[0].uri, filtered[1].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_1) {
  std::vector<RtpExtension> extensions;
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 3));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 9));
  extensions.push_back(RtpExtension(RtpExtension::kAbsSendTimeUri, 6));
  extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 1));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 14));
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1, filtered.size());
  EXPECT_EQ(RtpExtension::kTransportSequenceNumberUri, filtered[0].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_2) {
  std::vector<RtpExtension> extensions;
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 1));
  extensions.push_back(RtpExtension(RtpExtension::kAbsSendTimeUri, 14));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 7));
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1, filtered.size());
  EXPECT_EQ(RtpExtension::kAbsSendTimeUri, filtered[0].uri);
}

TEST(WebRtcMediaEngineTest, FilterRtpExtensions_RemoveRedundantBwe_3) {
  std::vector<RtpExtension> extensions;
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 2));
  extensions.push_back(RtpExtension(RtpExtension::kTimestampOffsetUri, 14));
  std::vector<webrtc::RtpExtension> filtered =
      FilterRtpExtensions(extensions, SupportedExtensions2, true);
  EXPECT_EQ(1, filtered.size());
  EXPECT_EQ(RtpExtension::kTimestampOffsetUri, filtered[0].uri);
}

TEST(WebRtcMediaEngineFactoryTest, CreateOldApi) {
  std::unique_ptr<MediaEngineInterface> engine(
      WebRtcMediaEngineFactory::Create(nullptr, nullptr, nullptr));
  EXPECT_TRUE(engine);
}

TEST(WebRtcMediaEngineFactoryTest, CreateWithBuiltinDecoders) {
  std::unique_ptr<MediaEngineInterface> engine(WebRtcMediaEngineFactory::Create(
      nullptr, webrtc::CreateBuiltinAudioDecoderFactory(), nullptr, nullptr));
  EXPECT_TRUE(engine);
}

}  // namespace cricket
