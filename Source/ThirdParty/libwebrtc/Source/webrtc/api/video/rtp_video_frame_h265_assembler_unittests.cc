/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/video/encoded_frame.h"
#include "api/video/rtp_video_frame_assembler.h"
#include "api/video/video_frame_type.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using PayloadFormat = RtpVideoFrameAssembler::PayloadFormat;

class PacketBuilder {
 public:
  PacketBuilder() : packet_to_send_(&extension_manager_) {}

  PacketBuilder& WithSeqNum(uint16_t seq_num) {
    seq_num_ = seq_num;
    return *this;
  }

  PacketBuilder& WithPayload(ArrayView<const uint8_t> payload) {
    payload_.assign(payload.begin(), payload.end());
    return *this;
  }

  PacketBuilder& WithVideoHeader(const RTPVideoHeader& video_header) {
    video_header_ = video_header;
    return *this;
  }

  template <typename T, typename... Args>
  PacketBuilder& WithExtension(int id, const Args&... args) {
    extension_manager_.Register<T>(id);
    packet_to_send_.IdentifyExtensions(extension_manager_);
    packet_to_send_.SetExtension<T>(std::forward<const Args>(args)...);
    return *this;
  }

  RtpPacketReceived Build() {
    auto packetizer = RtpPacketizer::Create(
        RtpPacketizer::PacketizationFormat::kH265, payload_, {}, video_header_);
    packetizer->NextPacket(&packet_to_send_);
    packet_to_send_.SetSequenceNumber(seq_num_);

    RtpPacketReceived received(&extension_manager_);
    received.Parse(packet_to_send_.Buffer());
    return received;
  }

 private:
  uint16_t seq_num_ = 0;
  std::vector<uint8_t> payload_;
  RTPVideoHeader video_header_;
  RtpPacketReceived::ExtensionManager extension_manager_;
  RtpPacketToSend packet_to_send_;
};

void AppendFrames(RtpVideoFrameAssembler::FrameVector&& from,
                  RtpVideoFrameAssembler::FrameVector& to) {
  to.insert(to.end(), std::make_move_iterator(from.begin()),
            std::make_move_iterator(from.end()));
}

ArrayView<int64_t> References(const std::unique_ptr<EncodedFrame>& frame) {
  return MakeArrayView(frame->references, frame->num_references);
}

ArrayView<const uint8_t> Payload(const std::unique_ptr<EncodedFrame>& frame) {
  return *frame->GetEncodedData();
}

TEST(RtpVideoFrameH265Assembler, H265Packetization) {
  RtpVideoFrameAssembler assembler(RtpVideoFrameAssembler::kH265);
  RtpVideoFrameAssembler::FrameVector frames;

  // Key and delta frames generated on linux with ffmpeg command:
  // `ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720 camera.h265`,
  // truncated for test.
  // IDR_N_LP(key) frame with start code included.
  uint8_t kIdrPayload[] = {0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf,
                           0x08, 0x4a, 0x31, 0x11, 0x15, 0xe5, 0xc0};
  // TRAIL_R(delta) frame with start code included.
  uint8_t kDeltaPayload[] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0xd0,
                             0x09, 0x7e, 0x10, 0xc6, 0x1c, 0x8c, 0x17};

  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  RtpVideoFrameAssembler::FrameVector idr_frames =
      assembler.InsertPacket(PacketBuilder()
                                 .WithPayload(kIdrPayload)
                                 .WithVideoHeader(video_header)
                                 .WithSeqNum(10)
                                 .Build());
  AppendFrames(std::move(idr_frames), frames);

  RtpVideoFrameAssembler::FrameVector delta_frames = assembler.InsertPacket(
      PacketBuilder().WithPayload(kDeltaPayload).WithSeqNum(11).Build());
  AppendFrames(std::move(delta_frames), frames);
  ASSERT_THAT(frames, SizeIs(2));

  auto first_frame = frames[0].ExtractFrame();
  EXPECT_THAT(first_frame->Id(), Eq(10));
  EXPECT_THAT(Payload(first_frame), ElementsAreArray(kIdrPayload));
  EXPECT_THAT(References(first_frame), IsEmpty());

  auto second_frame = frames[1].ExtractFrame();
  EXPECT_THAT(second_frame->Id(), Eq(11));
  EXPECT_THAT(Payload(second_frame), ElementsAreArray(kDeltaPayload));
  EXPECT_THAT(References(second_frame), UnorderedElementsAre(10));
}

}  // namespace
}  // namespace webrtc
