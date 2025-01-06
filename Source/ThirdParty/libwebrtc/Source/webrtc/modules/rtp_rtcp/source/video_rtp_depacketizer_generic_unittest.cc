/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_generic.h"

#include <stdint.h>

#include <optional>

#include "rtc_base/copy_on_write_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::SizeIs;

TEST(VideoRtpDepacketizerGeneric, NonExtendedHeaderNoFrameId) {
  const size_t kRtpPayloadSize = 10;
  const uint8_t kPayload[kRtpPayloadSize] = {0x01};
  rtc::CopyOnWriteBuffer rtp_payload(kPayload);

  VideoRtpDepacketizerGeneric depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);

  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->video_header.generic, std::nullopt);
  EXPECT_THAT(parsed->video_payload, SizeIs(kRtpPayloadSize - 1));
}

TEST(VideoRtpDepacketizerGeneric, ExtendedHeaderParsesFrameId) {
  const size_t kRtpPayloadSize = 10;
  const uint8_t kPayload[kRtpPayloadSize] = {0x05, 0x13, 0x37};
  rtc::CopyOnWriteBuffer rtp_payload(kPayload);

  VideoRtpDepacketizerGeneric depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);

  ASSERT_TRUE(parsed);
  const auto* generic_header = absl::get_if<RTPVideoHeaderLegacyGeneric>(
      &parsed->video_header.video_type_header);
  ASSERT_TRUE(generic_header);
  EXPECT_EQ(generic_header->picture_id, 0x1337);
  EXPECT_THAT(parsed->video_payload, SizeIs(kRtpPayloadSize - 3));
}

TEST(VideoRtpDepacketizerGeneric, PassRtpPayloadAsVideoPayload) {
  const uint8_t kPayload[] = {0x01, 0x25, 0x52};
  rtc::CopyOnWriteBuffer rtp_payload(kPayload);

  VideoRtpDepacketizerGeneric depacketizer;
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed =
      depacketizer.Parse(rtp_payload);

  ASSERT_TRUE(parsed);
  // Check there was no memcpy involved by verifying return and original buffers
  // point to the same buffer.
  EXPECT_EQ(parsed->video_payload.cdata(), rtp_payload.cdata() + 1);
}

}  // namespace
}  // namespace webrtc
