/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "api/jsep.h"
#include "api/webrtc_sdp.h"
#include "pc/session_description.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(WebRtcSdpEncodingOptionsTest,
     SdpSerializeUsesWildcardWhenEncodingOptionEnabled) {
  const std::string sdp_string =
      "v=0\r\n"
      "o=- 18446744069414584320 18446462598732840960 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "m=audio 9 RTP/SAVPF 111\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "m=video 3457 RTP/SAVPF 101\r\n"
      "a=rtpmap:101 VP8/90000\r\n";

  std::unique_ptr<SessionDescriptionInterface> jdesc =
      SdpDeserialize(SdpType::kOffer, sdp_string);
  ASSERT_TRUE(jdesc);

  // Manually enable ccfb on the media descriptions.
  for (auto& content : jdesc->description()->contents()) {
    content.media_description()->set_rtcp_fb_ack_ccfb(true);
  }

  // Create a new description with encoding options.
  std::unique_ptr<SessionDescriptionInterface> jdesc_with_options =
      SessionDescriptionInterface::Create(
          jdesc->GetType(), jdesc->description()->Clone(), jdesc->id(),
          jdesc->version(), {}, {.use_wildcard = true});

  std::string serialized = SdpSerialize(*jdesc_with_options);

  // When use_wildcard is true, we expect wildcard a=rtcp-fb lines.
  EXPECT_THAT(serialized, HasSubstr("a=rtcp-fb:* ack ccfb"));
  // And NOT the per-payload type lines for ccfb
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:111 ack ccfb")));
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:101 ack ccfb")));
}

TEST(WebRtcSdpEncodingOptionsTest,
     SdpSerializeUsesWildcardForCommonFeedbackParams) {
  const std::string sdp_string =
      "v=0\r\n"
      "o=- 18446744069414584320 18446462598732840960 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "m=video 3457 RTP/SAVPF 101 102\r\n"
      "a=rtpmap:101 VP8/90000\r\n"
      "a=rtcp-fb:101 nack\r\n"
      "a=rtcp-fb:101 nack pli\r\n"
      "a=rtpmap:102 VP9/90000\r\n"
      "a=rtcp-fb:102 nack\r\n"
      "a=rtcp-fb:102 nack pli\r\n"
      "a=rtcp-fb:102 transport-cc\r\n";

  std::unique_ptr<SessionDescriptionInterface> jdesc =
      SdpDeserialize(SdpType::kOffer, sdp_string);
  ASSERT_TRUE(jdesc);

  // Create a new description with encoding options.
  std::unique_ptr<SessionDescriptionInterface> jdesc_with_options =
      SessionDescriptionInterface::Create(
          jdesc->GetType(), jdesc->description()->Clone(), jdesc->id(),
          jdesc->version(), {}, {.use_wildcard = true});

  std::string serialized = SdpSerialize(*jdesc_with_options);

  // Expect wildcarded common feedback params.
  EXPECT_THAT(serialized, HasSubstr("a=rtcp-fb:* nack"));
  EXPECT_THAT(serialized, HasSubstr("a=rtcp-fb:* nack pli"));

  // Expect non-common param to remain per-payload type.
  EXPECT_THAT(serialized, HasSubstr("a=rtcp-fb:102 transport-cc"));

  // And common ones NOT to be per-payload type.
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:101 nack")));
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:102 nack")));
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:101 nack pli")));
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:102 nack pli")));
}

TEST(WebRtcSdpEncodingOptionsTest,
     SdpSerializeDoesNotUseWildcardWhenEncodingOptionDisabled) {
  const std::string sdp_string =
      "v=0\r\n"
      "o=- 18446744069414584320 18446462598732840960 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "m=audio 9 RTP/SAVPF 111\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "m=video 3457 RTP/SAVPF 101\r\n"
      "a=rtpmap:101 VP8/90000\r\n";

  std::unique_ptr<SessionDescriptionInterface> jdesc =
      SdpDeserialize(SdpType::kOffer, sdp_string);
  ASSERT_TRUE(jdesc);

  // Manually enable ccfb on the media descriptions.
  for (auto& content : jdesc->description()->contents()) {
    content.media_description()->set_rtcp_fb_ack_ccfb(true);
  }

  // encoding_options.use_wildcard defaults to false.
  std::string serialized = SdpSerialize(*jdesc);

  // When use_wildcard is false, we expect per-payload type a=rtcp-fb lines.
  EXPECT_THAT(serialized, Not(HasSubstr("a=rtcp-fb:* ack ccfb")));
  EXPECT_THAT(serialized, HasSubstr("a=rtcp-fb:111 ack ccfb"));
  EXPECT_THAT(serialized, HasSubstr("a=rtcp-fb:101 ack ccfb"));
}

}  // namespace webrtc
