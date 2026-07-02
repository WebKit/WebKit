/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/codec_list.h"

#include <vector>

#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {


TEST(CodecList, StoreAndRecall) {
  CodecList empty_list = CodecList::CreateFromTrustedData(std::vector<Codec>{});
  EXPECT_TRUE(empty_list.empty());
  EXPECT_TRUE(empty_list.codecs().empty());
  Codec video_codec = CreateVideoCodec({SdpVideoFormat{"VP8"}});
  CodecList one_codec = CodecList::CreateFromTrustedData({{video_codec}});
  EXPECT_EQ(one_codec.size(), 1U);
  EXPECT_EQ(one_codec.codecs()[0], video_codec);
}

TEST(CodecList, RejectIllegalConstructorArguments) {
  std::vector<Codec> apt_without_number{CreateVideoCodec(
      {SdpVideoFormat{"rtx", CodecParameterMap{{"apt", "not-a-number"}}}})};
  apt_without_number[0].id = 96;
  RTCErrorOr<CodecList> checked_codec_list =
      CodecList::Create(apt_without_number);
  EXPECT_FALSE(checked_codec_list.ok());
  EXPECT_EQ(checked_codec_list.error().type(), RTCErrorType::INVALID_PARAMETER);
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(CodecList, CrashOnIllegalConstructorArguments) {
  // This tests initializing a CodecList with a sequence that doesn't
  // satisfy its expected invariants.
  // Those invariants are only checked in debug mode.
  // See CodecList::CheckInputConsistency for what checks are enabled.
  // Checks that can't be enabled log things instead.
  // Note: DCHECK is on in some release builds, so we can't use
  // EXPECT_DEBUG_DEATH here.
  std::vector<Codec> apt_without_number{CreateVideoCodec(
      {SdpVideoFormat{"rtx", CodecParameterMap{{"apt", "not-a-number"}}}})};
  apt_without_number[0].id = 96;
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(
      CodecList bad = CodecList::CreateFromTrustedData(apt_without_number),
      "CheckInputConsistency");
#else
  // Expect initialization to succeed.
  CodecList bad = CodecList::CreateFromTrustedData(apt_without_number);
  EXPECT_EQ(bad.size(), 1U);
#endif
}
#endif

}  // namespace
}  // namespace webrtc
