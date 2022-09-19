/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FAKE_ENCODED_FRAME_H_
#define TEST_FAKE_ENCODED_FRAME_H_

#include <memory>
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#include <vector>

#include "api/rtp_packet_infos.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_rotation.h"
#include "test/gmock.h"

namespace webrtc {

// For test printing.
void PrintTo(const EncodedFrame& frame,
             std::ostream* os);  // no-presubmit-check TODO(webrtc:8982)

namespace test {

class FakeEncodedFrame : public EncodedFrame {
 public:
  // Always 10ms delay and on time.
  int64_t ReceivedTime() const override;
  int64_t RenderTime() const override;

  // Setters for protected variables.
  void SetReceivedTime(int64_t received_time);
  void SetPayloadType(int payload_type);

 private:
  int64_t received_time_;
};

MATCHER_P(WithId, id, "") {
  return ::testing::Matches(::testing::Eq(id))(arg.Id());
}

MATCHER_P(FrameWithSize, id, "") {
  return ::testing::Matches(::testing::Eq(id))(arg.size());
}

MATCHER_P(RtpTimestamp, ts, "") {
  return ts == arg.Timestamp();
}

class FakeFrameBuilder {
 public:
  FakeFrameBuilder& Time(uint32_t rtp_timestamp);
  FakeFrameBuilder& Id(int64_t frame_id);
  FakeFrameBuilder& AsLast();
  FakeFrameBuilder& Refs(const std::vector<int64_t>& references);
  FakeFrameBuilder& PlayoutDelay(VideoPlayoutDelay playout_delay);
  FakeFrameBuilder& SpatialLayer(int spatial_layer);
  FakeFrameBuilder& ReceivedTime(Timestamp receive_time);
  FakeFrameBuilder& Size(size_t size);
  FakeFrameBuilder& PayloadType(int payload_type);
  FakeFrameBuilder& NtpTime(Timestamp ntp_time);
  FakeFrameBuilder& Rotation(VideoRotation rotation);
  FakeFrameBuilder& PacketInfos(RtpPacketInfos packet_infos);
  std::unique_ptr<FakeEncodedFrame> Build();

 private:
  absl::optional<uint32_t> rtp_timestamp_;
  absl::optional<int64_t> frame_id_;
  absl::optional<VideoPlayoutDelay> playout_delay_;
  absl::optional<int> spatial_layer_;
  absl::optional<Timestamp> received_time_;
  absl::optional<int> payload_type_;
  absl::optional<Timestamp> ntp_time_;
  absl::optional<VideoRotation> rotation_;
  absl::optional<RtpPacketInfos> packet_infos_;
  std::vector<int64_t> references_;
  bool last_spatial_layer_ = false;
  size_t size_ = 10;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_ENCODED_FRAME_H_
