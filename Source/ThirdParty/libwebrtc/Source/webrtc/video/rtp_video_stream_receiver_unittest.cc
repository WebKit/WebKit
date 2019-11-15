/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver.h"

#include "absl/memory/memory.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_common.h"
#include "media/base/media_constants.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::SizeIs;
using ::testing::Values;

namespace webrtc {

namespace {

const uint8_t kH264StartCode[] = {0x00, 0x00, 0x00, 0x01};

class MockTransport : public Transport {
 public:
  MOCK_METHOD3(SendRtp,
               bool(const uint8_t* packet,
                    size_t length,
                    const PacketOptions& options));
  MOCK_METHOD2(SendRtcp, bool(const uint8_t* packet, size_t length));
};

class MockNackSender : public NackSender {
 public:
  MOCK_METHOD1(SendNack, void(const std::vector<uint16_t>& sequence_numbers));
  MOCK_METHOD2(SendNack,
               void(const std::vector<uint16_t>& sequence_numbers,
                    bool buffering_allowed));
};

class MockKeyFrameRequestSender : public KeyFrameRequestSender {
 public:
  MOCK_METHOD0(RequestKeyFrame, void());
};

class MockOnCompleteFrameCallback
    : public video_coding::OnCompleteFrameCallback {
 public:
  MockOnCompleteFrameCallback() : buffer_(rtc::ByteBuffer::ORDER_NETWORK) {}

  MOCK_METHOD1(DoOnCompleteFrame, void(video_coding::EncodedFrame* frame));
  MOCK_METHOD1(DoOnCompleteFrameFailNullptr,
               void(video_coding::EncodedFrame* frame));
  MOCK_METHOD1(DoOnCompleteFrameFailLength,
               void(video_coding::EncodedFrame* frame));
  MOCK_METHOD1(DoOnCompleteFrameFailBitstream,
               void(video_coding::EncodedFrame* frame));
  void OnCompleteFrame(std::unique_ptr<video_coding::EncodedFrame> frame) {
    if (!frame) {
      DoOnCompleteFrameFailNullptr(nullptr);
      return;
    }
    EXPECT_EQ(buffer_.Length(), frame->size());
    if (buffer_.Length() != frame->size()) {
      DoOnCompleteFrameFailLength(frame.get());
      return;
    }
    if (frame->size() != buffer_.Length() ||
        memcmp(buffer_.Data(), frame->data(), buffer_.Length()) != 0) {
      DoOnCompleteFrameFailBitstream(frame.get());
      return;
    }
    DoOnCompleteFrame(frame.get());
  }

  void ClearExpectedBitstream() { buffer_.Clear(); }

  void AppendExpectedBitstream(const uint8_t data[], size_t size_in_bytes) {
    // TODO(Johan): Let rtc::ByteBuffer handle uint8_t* instead of char*.
    buffer_.WriteBytes(reinterpret_cast<const char*>(data), size_in_bytes);
  }
  rtc::ByteBufferWriter buffer_;
};

class MockRtpPacketSink : public RtpPacketSinkInterface {
 public:
  MOCK_METHOD1(OnRtpPacket, void(const RtpPacketReceived&));
};

constexpr uint32_t kSsrc = 111;
constexpr uint16_t kSequenceNumber = 222;
std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceived(
    uint32_t ssrc = kSsrc,
    uint16_t sequence_number = kSequenceNumber) {
  auto packet = absl::make_unique<RtpPacketReceived>();
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(sequence_number);
  return packet;
}

MATCHER_P(SamePacketAs, other, "") {
  return arg.Ssrc() == other.Ssrc() &&
         arg.SequenceNumber() == other.SequenceNumber();
}

}  // namespace

class RtpVideoStreamReceiverTest : public ::testing::Test {
 public:
  RtpVideoStreamReceiverTest() : RtpVideoStreamReceiverTest("") {}
  explicit RtpVideoStreamReceiverTest(std::string field_trials)
      : override_field_trials_(field_trials),
        config_(CreateConfig()),
        process_thread_(ProcessThread::Create("TestThread")) {}

  void SetUp() {
    rtp_receive_statistics_ =
        ReceiveStatistics::Create(Clock::GetRealTimeClock());
    rtp_video_stream_receiver_ = absl::make_unique<RtpVideoStreamReceiver>(
        Clock::GetRealTimeClock(), &mock_transport_, nullptr, nullptr, &config_,
        rtp_receive_statistics_.get(), nullptr, process_thread_.get(),
        &mock_nack_sender_, &mock_key_frame_request_sender_,
        &mock_on_complete_frame_callback_, nullptr);
  }

  RTPVideoHeader GetDefaultH264VideoHeader() {
    RTPVideoHeader video_header;
    video_header.codec = kVideoCodecH264;
    video_header.video_type_header.emplace<RTPVideoHeaderH264>();
    return video_header;
  }

  // TODO(Johan): refactor h264_sps_pps_tracker_unittests.cc to avoid duplicate
  // code.
  void AddSps(RTPVideoHeader* video_header,
              uint8_t sps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kSps;
    info.sps_id = sps_id;
    info.pps_id = -1;
    data->push_back(H264::NaluType::kSps);
    data->push_back(sps_id);
    auto& h264 = absl::get<RTPVideoHeaderH264>(video_header->video_type_header);
    h264.nalus[h264.nalus_length++] = info;
  }

  void AddPps(RTPVideoHeader* video_header,
              uint8_t sps_id,
              uint8_t pps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kPps;
    info.sps_id = sps_id;
    info.pps_id = pps_id;
    data->push_back(H264::NaluType::kPps);
    data->push_back(pps_id);
    auto& h264 = absl::get<RTPVideoHeaderH264>(video_header->video_type_header);
    h264.nalus[h264.nalus_length++] = info;
  }

  void AddIdr(RTPVideoHeader* video_header, int pps_id) {
    NaluInfo info;
    info.type = H264::NaluType::kIdr;
    info.sps_id = -1;
    info.pps_id = pps_id;
    auto& h264 = absl::get<RTPVideoHeaderH264>(video_header->video_type_header);
    h264.nalus[h264.nalus_length++] = info;
  }

 protected:
  static VideoReceiveStream::Config CreateConfig() {
    VideoReceiveStream::Config config(nullptr);
    config.rtp.remote_ssrc = 1111;
    config.rtp.local_ssrc = 2222;
    return config;
  }

  const webrtc::test::ScopedFieldTrials override_field_trials_;
  VideoReceiveStream::Config config_;
  MockNackSender mock_nack_sender_;
  MockKeyFrameRequestSender mock_key_frame_request_sender_;
  MockTransport mock_transport_;
  MockOnCompleteFrameCallback mock_on_complete_frame_callback_;
  std::unique_ptr<ProcessThread> process_thread_;
  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<RtpVideoStreamReceiver> rtp_video_stream_receiver_;
};

TEST_F(RtpVideoStreamReceiverTest, CacheColorSpaceFromLastPacketOfKeyframe) {
  // Test that color space is cached from the last packet of a key frame and
  // that it's not reset by padding packets without color space.
  constexpr int kPayloadType = 99;
  const ColorSpace kColorSpace(
      ColorSpace::PrimaryID::kFILM, ColorSpace::TransferID::kBT2020_12,
      ColorSpace::MatrixID::kBT2020_NCL, ColorSpace::RangeID::kFull);
  const std::vector<uint8_t> kKeyFramePayload = {0, 1, 2, 3, 4, 5,
                                                 6, 7, 8, 9, 10};
  const std::vector<uint8_t> kDeltaFramePayload = {0, 1, 2, 3, 4};

  // Anonymous helper class that generates received packets.
  class {
   public:
    void SetPayload(const std::vector<uint8_t>& payload,
                    VideoFrameType video_frame_type) {
      video_frame_type_ = video_frame_type;
      RtpPacketizer::PayloadSizeLimits pay_load_size_limits;
      // Reduce max payload length to make sure the key frame generates two
      // packets.
      pay_load_size_limits.max_payload_len = 8;
      RTPVideoHeader rtp_video_header;
      RTPVideoHeaderVP9 rtp_video_header_vp9;
      rtp_video_header_vp9.InitRTPVideoHeaderVP9();
      rtp_video_header_vp9.inter_pic_predicted =
          (video_frame_type == VideoFrameType::kVideoFrameDelta);
      rtp_video_header.video_type_header = rtp_video_header_vp9;
      rtp_packetizer_ = RtpPacketizer::Create(
          kVideoCodecVP9, rtc::MakeArrayView(payload.data(), payload.size()),
          pay_load_size_limits, rtp_video_header, video_frame_type, nullptr);
    }

    size_t NumPackets() { return rtp_packetizer_->NumPackets(); }
    void SetColorSpace(const ColorSpace& color_space) {
      color_space_ = color_space;
    }

    RtpPacketReceived NextPacket() {
      RtpHeaderExtensionMap extension_map;
      extension_map.Register<ColorSpaceExtension>(1);
      RtpPacketToSend packet_to_send(&extension_map);
      packet_to_send.SetSequenceNumber(sequence_number_++);
      packet_to_send.SetSsrc(kSsrc);
      packet_to_send.SetPayloadType(kPayloadType);
      bool include_color_space =
          (rtp_packetizer_->NumPackets() == 1u &&
           video_frame_type_ == VideoFrameType::kVideoFrameKey);
      if (include_color_space) {
        EXPECT_TRUE(
            packet_to_send.SetExtension<ColorSpaceExtension>(color_space_));
      }
      rtp_packetizer_->NextPacket(&packet_to_send);

      RtpPacketReceived received_packet(&extension_map);
      received_packet.Parse(packet_to_send.data(), packet_to_send.size());
      return received_packet;
    }

   private:
    uint16_t sequence_number_ = 0;
    VideoFrameType video_frame_type_;
    ColorSpace color_space_;
    std::unique_ptr<RtpPacketizer> rtp_packetizer_;
  } received_packet_generator;
  received_packet_generator.SetColorSpace(kColorSpace);

  // Prepare the receiver for VP9.
  VideoCodec codec;
  codec.plType = kPayloadType;
  codec.codecType = kVideoCodecVP9;
  std::map<std::string, std::string> codec_params;
  rtp_video_stream_receiver_->AddReceiveCodec(codec, codec_params,
                                              /*raw_payload=*/false);

  // Generate key frame packets.
  received_packet_generator.SetPayload(kKeyFramePayload,
                                       VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(received_packet_generator.NumPackets(), 2u);
  RtpPacketReceived key_frame_packet1 = received_packet_generator.NextPacket();
  RtpPacketReceived key_frame_packet2 = received_packet_generator.NextPacket();

  // Generate delta frame packet.
  received_packet_generator.SetPayload(kDeltaFramePayload,
                                       VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(received_packet_generator.NumPackets(), 1u);
  RtpPacketReceived delta_frame_packet = received_packet_generator.NextPacket();

  rtp_video_stream_receiver_->StartReceive();
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kKeyFramePayload.data(), kKeyFramePayload.size());

  // Send the key frame and expect a callback with color space information.
  EXPECT_FALSE(key_frame_packet1.GetExtension<ColorSpaceExtension>());
  EXPECT_TRUE(key_frame_packet2.GetExtension<ColorSpaceExtension>());
  rtp_video_stream_receiver_->OnRtpPacket(key_frame_packet1);
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_))
      .WillOnce(Invoke([kColorSpace](video_coding::EncodedFrame* frame) {
        ASSERT_TRUE(frame->EncodedImage().ColorSpace());
        EXPECT_EQ(*frame->EncodedImage().ColorSpace(), kColorSpace);
      }));
  rtp_video_stream_receiver_->OnRtpPacket(key_frame_packet2);
  // Resend the first key frame packet to simulate padding for example.
  rtp_video_stream_receiver_->OnRtpPacket(key_frame_packet1);

  mock_on_complete_frame_callback_.ClearExpectedBitstream();
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kDeltaFramePayload.data(), kDeltaFramePayload.size());

  // Expect delta frame to have color space set even though color space not
  // included in the RTP packet.
  EXPECT_FALSE(delta_frame_packet.GetExtension<ColorSpaceExtension>());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_))
      .WillOnce(Invoke([kColorSpace](video_coding::EncodedFrame* frame) {
        ASSERT_TRUE(frame->EncodedImage().ColorSpace());
        EXPECT_EQ(*frame->EncodedImage().ColorSpace(), kColorSpace);
      }));
  rtp_video_stream_receiver_->OnRtpPacket(delta_frame_packet);
}

TEST_F(RtpVideoStreamReceiverTest, GenericKeyFrame) {
  RTPHeader rtp_header;
  RTPVideoHeader video_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  rtp_header.sequenceNumber = 1;
  video_header.is_first_packet_in_frame = true;
  video_header.is_last_packet_in_frame = true;
  video_header.codec = kVideoCodecGeneric;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);
}

TEST_F(RtpVideoStreamReceiverTest, NoInfiniteRecursionOnEncapsulatedRedPacket) {
  const uint8_t kRedPayloadType = 125;
  VideoCodec codec;
  codec.plType = kRedPayloadType;
  rtp_video_stream_receiver_->AddReceiveCodec(codec, {}, /*raw_payload=*/false);
  const std::vector<uint8_t> data({
      0x80,              // RTP version.
      kRedPayloadType,   // Payload type.
      0, 0, 0, 0, 0, 0,  // Don't care.
      0, 0, 0x4, 0x57,   // SSRC
      kRedPayloadType,   // RED header.
      0, 0, 0, 0, 0      // Don't care.
  });
  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(data.data(), data.size()));
  rtp_video_stream_receiver_->StartReceive();
  rtp_video_stream_receiver_->OnRtpPacket(packet);
}

TEST_F(RtpVideoStreamReceiverTest,
       DropsPacketWithRedPayloadTypeAndEmptyPayload) {
  const uint8_t kRedPayloadType = 125;
  config_.rtp.red_payload_type = kRedPayloadType;
  SetUp();  // re-create rtp_video_stream_receiver with red payload type.
  // clang-format off
  const uint8_t data[] = {
      0x80,              // RTP version.
      kRedPayloadType,   // Payload type.
      0, 0, 0, 0, 0, 0,  // Don't care.
      0, 0, 0x4, 0x57,   // SSRC
      // Empty rtp payload.
  };
  // clang-format on
  RtpPacketReceived packet;
  // Manually convert to CopyOnWriteBuffer to be sure capacity == size
  // and asan bot can catch read buffer overflow.
  EXPECT_TRUE(packet.Parse(rtc::CopyOnWriteBuffer(data)));
  rtp_video_stream_receiver_->StartReceive();
  rtp_video_stream_receiver_->OnRtpPacket(packet);
  // Expect asan doesn't find anything.
}

TEST_F(RtpVideoStreamReceiverTest, GenericKeyFrameBitstreamError) {
  RTPHeader rtp_header;
  RTPVideoHeader video_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  rtp_header.sequenceNumber = 1;
  video_header.is_first_packet_in_frame = true;
  video_header.is_last_packet_in_frame = true;
  video_header.codec = kVideoCodecGeneric;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  constexpr uint8_t expected_bitsteam[] = {1, 2, 3, 0xff};
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      expected_bitsteam, sizeof(expected_bitsteam));
  EXPECT_CALL(mock_on_complete_frame_callback_,
              DoOnCompleteFrameFailBitstream(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);
}

class RtpVideoStreamReceiverTestH264
    : public RtpVideoStreamReceiverTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  RtpVideoStreamReceiverTestH264() : RtpVideoStreamReceiverTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(SpsPpsIdrIsKeyframe,
                         RtpVideoStreamReceiverTestH264,
                         Values("", "WebRTC-SpsPpsIdrIsH264Keyframe/Enabled/"));

TEST_P(RtpVideoStreamReceiverTestH264, InBandSpsPps) {
  std::vector<uint8_t> sps_data;
  RTPHeader rtp_header;
  RTPVideoHeader sps_video_header = GetDefaultH264VideoHeader();
  AddSps(&sps_video_header, 0, &sps_data);
  rtp_header.sequenceNumber = 0;
  sps_video_header.is_first_packet_in_frame = true;
  sps_video_header.frame_type = VideoFrameType::kEmptyFrame;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(sps_data.data(),
                                                           sps_data.size());
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      sps_data.data(), sps_data.size(), rtp_header, sps_video_header,
      absl::nullopt, false);

  std::vector<uint8_t> pps_data;
  RTPVideoHeader pps_video_header = GetDefaultH264VideoHeader();
  AddPps(&pps_video_header, 0, 1, &pps_data);
  rtp_header.sequenceNumber = 1;
  pps_video_header.is_first_packet_in_frame = true;
  pps_video_header.frame_type = VideoFrameType::kEmptyFrame;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(pps_data.data(),
                                                           pps_data.size());
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      pps_data.data(), pps_data.size(), rtp_header, pps_video_header,
      absl::nullopt, false);

  std::vector<uint8_t> idr_data;
  RTPVideoHeader idr_video_header = GetDefaultH264VideoHeader();
  AddIdr(&idr_video_header, 1);
  rtp_header.sequenceNumber = 2;
  idr_video_header.is_first_packet_in_frame = true;
  idr_video_header.is_last_packet_in_frame = true;
  idr_video_header.frame_type = VideoFrameType::kVideoFrameKey;
  idr_data.insert(idr_data.end(), {0x65, 1, 2, 3});
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(idr_data.data(),
                                                           idr_data.size());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      idr_data.data(), idr_data.size(), rtp_header, idr_video_header,
      absl::nullopt, false);
}

TEST_P(RtpVideoStreamReceiverTestH264, OutOfBandFmtpSpsPps) {
  constexpr int kPayloadType = 99;
  VideoCodec codec;
  codec.plType = kPayloadType;
  std::map<std::string, std::string> codec_params;
  // Example parameter sets from https://tools.ietf.org/html/rfc3984#section-8.2
  // .
  codec_params.insert(
      {cricket::kH264FmtpSpropParameterSets, "Z0IACpZTBYmI,aMljiA=="});
  rtp_video_stream_receiver_->AddReceiveCodec(codec, codec_params,
                                              /*raw_payload=*/false);
  const uint8_t binary_sps[] = {0x67, 0x42, 0x00, 0x0a, 0x96,
                                0x53, 0x05, 0x89, 0x88};
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(binary_sps,
                                                           sizeof(binary_sps));
  const uint8_t binary_pps[] = {0x68, 0xc9, 0x63, 0x88};
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(binary_pps,
                                                           sizeof(binary_pps));

  std::vector<uint8_t> data;
  RTPHeader rtp_header;
  RTPVideoHeader video_header = GetDefaultH264VideoHeader();
  AddIdr(&video_header, 0);
  rtp_header.payloadType = kPayloadType;
  rtp_header.sequenceNumber = 2;
  video_header.is_first_packet_in_frame = true;
  video_header.is_last_packet_in_frame = true;
  video_header.codec = kVideoCodecH264;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  data.insert(data.end(), {1, 2, 3});
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);
}

TEST_F(RtpVideoStreamReceiverTest, PaddingInMediaStream) {
  RTPHeader rtp_header;
  RTPVideoHeader video_header = GetDefaultH264VideoHeader();
  std::vector<uint8_t> data;
  data.insert(data.end(), {1, 2, 3});
  rtp_header.payloadType = 99;
  rtp_header.sequenceNumber = 2;
  video_header.is_first_packet_in_frame = true;
  video_header.is_last_packet_in_frame = true;
  video_header.codec = kVideoCodecGeneric;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);

  rtp_header.sequenceNumber = 3;
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      nullptr, 0, rtp_header, video_header, absl::nullopt, false);

  rtp_header.sequenceNumber = 4;
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);

  rtp_header.sequenceNumber = 6;
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_header.sequenceNumber = 5;
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      nullptr, 0, rtp_header, video_header, absl::nullopt, false);
}

TEST_F(RtpVideoStreamReceiverTest, RequestKeyframeIfFirstFrameIsDelta) {
  RTPHeader rtp_header;
  RTPVideoHeader video_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  rtp_header.sequenceNumber = 1;
  video_header.is_first_packet_in_frame = true;
  video_header.is_last_packet_in_frame = true;
  video_header.codec = kVideoCodecGeneric;
  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_CALL(mock_key_frame_request_sender_, RequestKeyFrame());
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);
}

TEST_F(RtpVideoStreamReceiverTest, RequestKeyframeWhenPacketBufferGetsFull) {
  constexpr int kPacketBufferMaxSize = 2048;

  RTPHeader rtp_header;
  RTPVideoHeader video_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  video_header.is_first_packet_in_frame = true;
  // Incomplete frames so that the packet buffer is filling up.
  video_header.is_last_packet_in_frame = false;
  video_header.codec = kVideoCodecGeneric;
  video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  uint16_t start_sequence_number = 1234;
  rtp_header.sequenceNumber = start_sequence_number;
  while (rtp_header.sequenceNumber - start_sequence_number <
         kPacketBufferMaxSize) {
    rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                      rtp_header, video_header,
                                                      absl::nullopt, false);
    rtp_header.sequenceNumber += 2;
  }

  EXPECT_CALL(mock_key_frame_request_sender_, RequestKeyFrame());
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      data.data(), data.size(), rtp_header, video_header, absl::nullopt, false);
}

TEST_F(RtpVideoStreamReceiverTest, SecondarySinksGetRtpNotifications) {
  rtp_video_stream_receiver_->StartReceive();

  MockRtpPacketSink secondary_sink_1;
  MockRtpPacketSink secondary_sink_2;

  rtp_video_stream_receiver_->AddSecondarySink(&secondary_sink_1);
  rtp_video_stream_receiver_->AddSecondarySink(&secondary_sink_2);

  auto rtp_packet = CreateRtpPacketReceived();
  EXPECT_CALL(secondary_sink_1, OnRtpPacket(SamePacketAs(*rtp_packet)));
  EXPECT_CALL(secondary_sink_2, OnRtpPacket(SamePacketAs(*rtp_packet)));

  rtp_video_stream_receiver_->OnRtpPacket(*rtp_packet);

  // Test tear-down.
  rtp_video_stream_receiver_->StopReceive();
  rtp_video_stream_receiver_->RemoveSecondarySink(&secondary_sink_1);
  rtp_video_stream_receiver_->RemoveSecondarySink(&secondary_sink_2);
}

TEST_F(RtpVideoStreamReceiverTest, RemovedSecondarySinksGetNoRtpNotifications) {
  rtp_video_stream_receiver_->StartReceive();

  MockRtpPacketSink secondary_sink;

  rtp_video_stream_receiver_->AddSecondarySink(&secondary_sink);
  rtp_video_stream_receiver_->RemoveSecondarySink(&secondary_sink);

  auto rtp_packet = CreateRtpPacketReceived();

  EXPECT_CALL(secondary_sink, OnRtpPacket(_)).Times(0);

  rtp_video_stream_receiver_->OnRtpPacket(*rtp_packet);

  // Test tear-down.
  rtp_video_stream_receiver_->StopReceive();
}

TEST_F(RtpVideoStreamReceiverTest,
       OnlyRemovedSecondarySinksExcludedFromNotifications) {
  rtp_video_stream_receiver_->StartReceive();

  MockRtpPacketSink kept_secondary_sink;
  MockRtpPacketSink removed_secondary_sink;

  rtp_video_stream_receiver_->AddSecondarySink(&kept_secondary_sink);
  rtp_video_stream_receiver_->AddSecondarySink(&removed_secondary_sink);
  rtp_video_stream_receiver_->RemoveSecondarySink(&removed_secondary_sink);

  auto rtp_packet = CreateRtpPacketReceived();
  EXPECT_CALL(kept_secondary_sink, OnRtpPacket(SamePacketAs(*rtp_packet)));

  rtp_video_stream_receiver_->OnRtpPacket(*rtp_packet);

  // Test tear-down.
  rtp_video_stream_receiver_->StopReceive();
  rtp_video_stream_receiver_->RemoveSecondarySink(&kept_secondary_sink);
}

TEST_F(RtpVideoStreamReceiverTest,
       SecondariesOfNonStartedStreamGetNoNotifications) {
  // Explicitly showing that the stream is not in the |started| state,
  // regardless of whether streams start out |started| or |stopped|.
  rtp_video_stream_receiver_->StopReceive();

  MockRtpPacketSink secondary_sink;
  rtp_video_stream_receiver_->AddSecondarySink(&secondary_sink);

  auto rtp_packet = CreateRtpPacketReceived();
  EXPECT_CALL(secondary_sink, OnRtpPacket(_)).Times(0);

  rtp_video_stream_receiver_->OnRtpPacket(*rtp_packet);

  // Test tear-down.
  rtp_video_stream_receiver_->RemoveSecondarySink(&secondary_sink);
}

class RtpVideoStreamReceiverGenericDescriptorTest
    : public RtpVideoStreamReceiverTest,
      public ::testing::WithParamInterface<int> {
 public:
  void RegisterRtpGenericFrameDescriptorExtension(
      RtpHeaderExtensionMap* extension_map,
      int version) {
    constexpr int kId00 = 5;
    constexpr int kId01 = 6;
    switch (version) {
      case 0:
        extension_map->Register<RtpGenericFrameDescriptorExtension00>(kId00);
        return;
      case 1:
        extension_map->Register<RtpGenericFrameDescriptorExtension01>(kId01);
        return;
    }
    RTC_NOTREACHED();
  }

  bool SetExtensionRtpGenericFrameDescriptorExtension(
      const RtpGenericFrameDescriptor& generic_descriptor,
      RtpPacketReceived* rtp_packet,
      int version) {
    switch (version) {
      case 0:
        return rtp_packet->SetExtension<RtpGenericFrameDescriptorExtension00>(
            generic_descriptor);
      case 1:
        return rtp_packet->SetExtension<RtpGenericFrameDescriptorExtension01>(
            generic_descriptor);
    }
    RTC_NOTREACHED();
    return false;
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         RtpVideoStreamReceiverGenericDescriptorTest,
                         Values(0, 1));

TEST_P(RtpVideoStreamReceiverGenericDescriptorTest,
       ParseGenericDescriptorOnePacket) {
  const int version = GetParam();

  const std::vector<uint8_t> data = {0, 1, 2, 3, 4};
  const int kPayloadType = 123;
  const int kSpatialIndex = 1;

  VideoCodec codec;
  codec.plType = kPayloadType;
  rtp_video_stream_receiver_->AddReceiveCodec(codec, {}, /*raw_payload=*/false);
  rtp_video_stream_receiver_->StartReceive();

  RtpHeaderExtensionMap extension_map;
  RegisterRtpGenericFrameDescriptorExtension(&extension_map, version);
  RtpPacketReceived rtp_packet(&extension_map);

  RtpGenericFrameDescriptor generic_descriptor;
  generic_descriptor.SetFirstPacketInSubFrame(true);
  generic_descriptor.SetLastPacketInSubFrame(true);
  generic_descriptor.SetFrameId(100);
  generic_descriptor.SetSpatialLayersBitmask(1 << kSpatialIndex);
  generic_descriptor.AddFrameDependencyDiff(90);
  generic_descriptor.AddFrameDependencyDiff(80);
  ASSERT_TRUE(SetExtensionRtpGenericFrameDescriptorExtension(
      generic_descriptor, &rtp_packet, version));

  uint8_t* payload = rtp_packet.SetPayloadSize(data.size());
  memcpy(payload, data.data(), data.size());
  // The first byte is the header, so we ignore the first byte of |data|.
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data() + 1,
                                                           data.size() - 1);

  rtp_packet.SetMarker(true);
  rtp_packet.SetPayloadType(kPayloadType);
  rtp_packet.SetSequenceNumber(1);

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame)
      .WillOnce(Invoke([kSpatialIndex](video_coding::EncodedFrame* frame) {
        EXPECT_EQ(frame->num_references, 2U);
        EXPECT_EQ(frame->references[0], frame->id.picture_id - 90);
        EXPECT_EQ(frame->references[1], frame->id.picture_id - 80);
        EXPECT_EQ(frame->id.spatial_layer, kSpatialIndex);
        EXPECT_THAT(frame->PacketInfos(), SizeIs(1));
      }));

  rtp_video_stream_receiver_->OnRtpPacket(rtp_packet);
}

TEST_P(RtpVideoStreamReceiverGenericDescriptorTest,
       ParseGenericDescriptorTwoPackets) {
  const int version = GetParam();

  const std::vector<uint8_t> data = {0, 1, 2, 3, 4};
  const int kPayloadType = 123;
  const int kSpatialIndex = 1;

  VideoCodec codec;
  codec.plType = kPayloadType;
  rtp_video_stream_receiver_->AddReceiveCodec(codec, {}, /*raw_payload=*/false);
  rtp_video_stream_receiver_->StartReceive();

  RtpHeaderExtensionMap extension_map;
  RegisterRtpGenericFrameDescriptorExtension(&extension_map, version);
  RtpPacketReceived first_packet(&extension_map);

  RtpGenericFrameDescriptor first_packet_descriptor;
  first_packet_descriptor.SetFirstPacketInSubFrame(true);
  first_packet_descriptor.SetLastPacketInSubFrame(false);
  first_packet_descriptor.SetFrameId(100);
  first_packet_descriptor.SetSpatialLayersBitmask(1 << kSpatialIndex);
  first_packet_descriptor.SetResolution(480, 360);
  ASSERT_TRUE(SetExtensionRtpGenericFrameDescriptorExtension(
      first_packet_descriptor, &first_packet, version));

  uint8_t* first_packet_payload = first_packet.SetPayloadSize(data.size());
  memcpy(first_packet_payload, data.data(), data.size());
  // The first byte is the header, so we ignore the first byte of |data|.
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data() + 1,
                                                           data.size() - 1);

  first_packet.SetPayloadType(kPayloadType);
  first_packet.SetSequenceNumber(1);
  rtp_video_stream_receiver_->OnRtpPacket(first_packet);

  RtpPacketReceived second_packet(&extension_map);
  RtpGenericFrameDescriptor second_packet_descriptor;
  second_packet_descriptor.SetFirstPacketInSubFrame(false);
  second_packet_descriptor.SetLastPacketInSubFrame(true);
  ASSERT_TRUE(SetExtensionRtpGenericFrameDescriptorExtension(
      second_packet_descriptor, &second_packet, version));

  second_packet.SetMarker(true);
  second_packet.SetPayloadType(kPayloadType);
  second_packet.SetSequenceNumber(2);

  uint8_t* second_packet_payload = second_packet.SetPayloadSize(data.size());
  memcpy(second_packet_payload, data.data(), data.size());
  // The first byte is the header, so we ignore the first byte of |data|.
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data() + 1,
                                                           data.size() - 1);

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame)
      .WillOnce(Invoke([kSpatialIndex](video_coding::EncodedFrame* frame) {
        EXPECT_EQ(frame->num_references, 0U);
        EXPECT_EQ(frame->id.spatial_layer, kSpatialIndex);
        EXPECT_EQ(frame->EncodedImage()._encodedWidth, 480u);
        EXPECT_EQ(frame->EncodedImage()._encodedHeight, 360u);
        EXPECT_THAT(frame->PacketInfos(), SizeIs(2));
      }));

  rtp_video_stream_receiver_->OnRtpPacket(second_packet);
}

TEST_F(RtpVideoStreamReceiverGenericDescriptorTest,
       DropPacketsWithMultipleVersionsOfExtension) {
  const std::vector<uint8_t> data = {0, 1, 2, 3, 4};
  const int kPayloadType = 123;

  VideoCodec codec;
  codec.plType = kPayloadType;
  rtp_video_stream_receiver_->AddReceiveCodec(codec, {}, /*raw_payload=*/false);
  rtp_video_stream_receiver_->StartReceive();

  RtpHeaderExtensionMap extension_map;
  RegisterRtpGenericFrameDescriptorExtension(&extension_map, 0);
  RegisterRtpGenericFrameDescriptorExtension(&extension_map, 1);
  RtpPacketReceived rtp_packet(&extension_map);

  RtpGenericFrameDescriptor generic_descriptors[2];
  for (size_t i = 0; i < 2; ++i) {
    generic_descriptors[i].SetFirstPacketInSubFrame(true);
    generic_descriptors[i].SetLastPacketInSubFrame(true);
    generic_descriptors[i].SetFrameId(100);
    ASSERT_TRUE(SetExtensionRtpGenericFrameDescriptorExtension(
        generic_descriptors[i], &rtp_packet, i));
  }

  uint8_t* payload = rtp_packet.SetPayloadSize(data.size());
  memcpy(payload, data.data(), data.size());
  // The first byte is the header, so we ignore the first byte of |data|.
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data() + 1,
                                                           data.size() - 1);

  rtp_packet.SetMarker(true);
  rtp_packet.SetPayloadType(kPayloadType);
  rtp_packet.SetSequenceNumber(1);

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame).Times(0);

  rtp_video_stream_receiver_->OnRtpPacket(rtp_packet);
}

TEST_P(RtpVideoStreamReceiverGenericDescriptorTest,
       ParseGenericDescriptorRawPayload) {
  const int version = GetParam();

  const std::vector<uint8_t> data = {0, 1, 2, 3, 4};
  const int kPayloadType = 123;

  VideoCodec codec;
  codec.plType = kPayloadType;
  rtp_video_stream_receiver_->AddReceiveCodec(codec, {}, /*raw_payload=*/true);
  rtp_video_stream_receiver_->StartReceive();

  RtpHeaderExtensionMap extension_map;
  RegisterRtpGenericFrameDescriptorExtension(&extension_map, version);
  RtpPacketReceived rtp_packet(&extension_map);

  RtpGenericFrameDescriptor generic_descriptor;
  generic_descriptor.SetFirstPacketInSubFrame(true);
  generic_descriptor.SetLastPacketInSubFrame(true);
  ASSERT_TRUE(SetExtensionRtpGenericFrameDescriptorExtension(
      generic_descriptor, &rtp_packet, version));

  uint8_t* payload = rtp_packet.SetPayloadSize(data.size());
  memcpy(payload, data.data(), data.size());
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());

  rtp_packet.SetMarker(true);
  rtp_packet.SetPayloadType(kPayloadType);
  rtp_packet.SetSequenceNumber(1);

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame);
  rtp_video_stream_receiver_->OnRtpPacket(rtp_packet);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(RtpVideoStreamReceiverTest, RepeatedSecondarySinkDisallowed) {
  MockRtpPacketSink secondary_sink;

  rtp_video_stream_receiver_->AddSecondarySink(&secondary_sink);
  EXPECT_DEATH(rtp_video_stream_receiver_->AddSecondarySink(&secondary_sink),
               "");

  // Test tear-down.
  rtp_video_stream_receiver_->RemoveSecondarySink(&secondary_sink);
}
#endif

}  // namespace webrtc
