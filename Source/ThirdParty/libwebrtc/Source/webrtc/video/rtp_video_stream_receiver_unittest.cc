/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/gtest.h"
#include "webrtc/test/gmock.h"

#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet.h"
#include "webrtc/modules/video_coding/rtp_frame_reference_finder.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/field_trial_default.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/video/rtp_video_stream_receiver.h"

using testing::_;

namespace webrtc {

namespace {

const char kNewJitterBufferFieldTrialEnabled[] =
    "WebRTC-NewVideoJitterBuffer/Enabled/";
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
};

class MockKeyFrameRequestSender : public KeyFrameRequestSender {
 public:
  MOCK_METHOD0(RequestKeyFrame, void());
};

class MockOnCompleteFrameCallback
    : public video_coding::OnCompleteFrameCallback {
 public:
  MockOnCompleteFrameCallback() : buffer_(rtc::ByteBuffer::ORDER_NETWORK) {}

  MOCK_METHOD1(DoOnCompleteFrame, void(video_coding::FrameObject* frame));
  MOCK_METHOD1(DoOnCompleteFrameFailNullptr,
               void(video_coding::FrameObject* frame));
  MOCK_METHOD1(DoOnCompleteFrameFailLength,
               void(video_coding::FrameObject* frame));
  MOCK_METHOD1(DoOnCompleteFrameFailBitstream,
               void(video_coding::FrameObject* frame));
  void OnCompleteFrame(std::unique_ptr<video_coding::FrameObject> frame) {
    if (!frame) {
      DoOnCompleteFrameFailNullptr(nullptr);
      return;
    }
    EXPECT_EQ(buffer_.Length(), frame->size());
    if (buffer_.Length() != frame->size()) {
      DoOnCompleteFrameFailLength(frame.get());
      return;
    }
    std::vector<uint8_t> actual_data(frame->size());
    frame->GetBitstream(actual_data.data());
    if (memcmp(buffer_.Data(), actual_data.data(), buffer_.Length()) != 0) {
      DoOnCompleteFrameFailBitstream(frame.get());
      return;
    }
    DoOnCompleteFrame(frame.get());
  }
  void AppendExpectedBitstream(const uint8_t data[], size_t size_in_bytes) {
    // TODO(Johan): Let rtc::ByteBuffer handle uint8_t* instead of char*.
    buffer_.WriteBytes(reinterpret_cast<const char*>(data), size_in_bytes);
  }
  rtc::ByteBufferWriter buffer_;
};

}  // namespace

class RtpVideoStreamReceiverTest : public testing::Test {
 public:
  RtpVideoStreamReceiverTest()
      : config_(CreateConfig()),
        timing_(Clock::GetRealTimeClock()),
        process_thread_(ProcessThread::Create("TestThread")) {}

  void SetUp() {
    rtp_video_stream_receiver_.reset(new RtpVideoStreamReceiver(
        &mock_transport_, nullptr, &packet_router_, &config_,
        nullptr, process_thread_.get(), &mock_nack_sender_,
        &mock_key_frame_request_sender_, &mock_on_complete_frame_callback_,
        &timing_));
  }

  WebRtcRTPHeader GetDefaultPacket() {
    WebRtcRTPHeader packet;
    memset(&packet, 0, sizeof(packet));
    packet.type.Video.codec = kRtpVideoH264;
    return packet;
  }

  // TODO(Johan): refactor h264_sps_pps_tracker_unittests.cc to avoid duplicate
  // code.
  void AddSps(WebRtcRTPHeader* packet, int sps_id, std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kSps;
    info.sps_id = sps_id;
    info.pps_id = -1;
    info.offset = data->size();
    info.size = 2;
    data->push_back(H264::NaluType::kSps);
    data->push_back(sps_id);
    packet->type.Video.codecHeader.H264
        .nalus[packet->type.Video.codecHeader.H264.nalus_length++] = info;
  }

  void AddPps(WebRtcRTPHeader* packet,
              int sps_id,
              int pps_id,
              std::vector<uint8_t>* data) {
    NaluInfo info;
    info.type = H264::NaluType::kPps;
    info.sps_id = sps_id;
    info.pps_id = pps_id;
    info.offset = data->size();
    info.size = 2;
    data->push_back(H264::NaluType::kPps);
    data->push_back(pps_id);
    packet->type.Video.codecHeader.H264
        .nalus[packet->type.Video.codecHeader.H264.nalus_length++] = info;
  }

  void AddIdr(WebRtcRTPHeader* packet, int pps_id) {
    NaluInfo info;
    info.type = H264::NaluType::kIdr;
    info.sps_id = -1;
    info.pps_id = pps_id;
    packet->type.Video.codecHeader.H264
        .nalus[packet->type.Video.codecHeader.H264.nalus_length++] = info;
  }

 protected:
  static VideoReceiveStream::Config CreateConfig() {
    VideoReceiveStream::Config config(nullptr);
    config.rtp.remote_ssrc = 1111;
    config.rtp.local_ssrc = 2222;
    return config;
  }

  webrtc::test::ScopedFieldTrials override_field_trials_{
      kNewJitterBufferFieldTrialEnabled};
  VideoReceiveStream::Config config_;
  MockNackSender mock_nack_sender_;
  MockKeyFrameRequestSender mock_key_frame_request_sender_;
  MockTransport mock_transport_;
  MockOnCompleteFrameCallback mock_on_complete_frame_callback_;
  PacketRouter packet_router_;
  VCMTiming timing_;
  std::unique_ptr<ProcessThread> process_thread_;
  std::unique_ptr<RtpVideoStreamReceiver> rtp_video_stream_receiver_;
};

TEST_F(RtpVideoStreamReceiverTest, GenericKeyFrame) {
  WebRtcRTPHeader rtp_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  memset(&rtp_header, 0, sizeof(rtp_header));
  rtp_header.header.sequenceNumber = 1;
  rtp_header.header.markerBit = 1;
  rtp_header.type.Video.is_first_packet_in_frame = true;
  rtp_header.frameType = kVideoFrameKey;
  rtp_header.type.Video.codec = kRtpVideoGeneric;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &rtp_header);
}

TEST_F(RtpVideoStreamReceiverTest, GenericKeyFrameBitstreamError) {
  WebRtcRTPHeader rtp_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  memset(&rtp_header, 0, sizeof(rtp_header));
  rtp_header.header.sequenceNumber = 1;
  rtp_header.header.markerBit = 1;
  rtp_header.type.Video.is_first_packet_in_frame = true;
  rtp_header.frameType = kVideoFrameKey;
  rtp_header.type.Video.codec = kRtpVideoGeneric;
  constexpr uint8_t expected_bitsteam[] = {1, 2, 3, 0xff};
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      expected_bitsteam, sizeof(expected_bitsteam));
  EXPECT_CALL(mock_on_complete_frame_callback_,
              DoOnCompleteFrameFailBitstream(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &rtp_header);
}

TEST_F(RtpVideoStreamReceiverTest, InBandSpsPps) {
  std::vector<uint8_t> sps_data;
  WebRtcRTPHeader sps_packet = GetDefaultPacket();
  AddSps(&sps_packet, 0, &sps_data);
  sps_packet.header.sequenceNumber = 0;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(sps_data.data(),
                                                           sps_data.size());
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      sps_data.data(), sps_data.size(), &sps_packet);

  std::vector<uint8_t> pps_data;
  WebRtcRTPHeader pps_packet = GetDefaultPacket();
  AddPps(&pps_packet, 0, 1, &pps_data);
  pps_packet.header.sequenceNumber = 1;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(pps_data.data(),
                                                           pps_data.size());
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      pps_data.data(), pps_data.size(), &pps_packet);

  std::vector<uint8_t> idr_data;
  WebRtcRTPHeader idr_packet = GetDefaultPacket();
  AddIdr(&idr_packet, 1);
  idr_packet.type.Video.is_first_packet_in_frame = true;
  idr_packet.header.sequenceNumber = 2;
  idr_packet.header.markerBit = 1;
  idr_packet.type.Video.is_first_packet_in_frame = true;
  idr_packet.frameType = kVideoFrameKey;
  idr_packet.type.Video.codec = kRtpVideoH264;
  idr_data.insert(idr_data.end(), {0x65, 1, 2, 3});
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(idr_data.data(),
                                                           idr_data.size());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(
      idr_data.data(), idr_data.size(), &idr_packet);
}

TEST_F(RtpVideoStreamReceiverTest, OutOfBandFmtpSpsPps) {
  constexpr int kPayloadType = 99;
  VideoCodec codec;
  codec.plType = kPayloadType;
  std::map<std::string, std::string> codec_params;
  // Example parameter sets from https://tools.ietf.org/html/rfc3984#section-8.2
  // .
  codec_params.insert(
      {cricket::kH264FmtpSpropParameterSets, "Z0IACpZTBYmI,aMljiA=="});
  rtp_video_stream_receiver_->AddReceiveCodec(codec, codec_params);
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
  WebRtcRTPHeader idr_packet = GetDefaultPacket();
  AddIdr(&idr_packet, 0);
  idr_packet.header.payloadType = kPayloadType;
  idr_packet.type.Video.is_first_packet_in_frame = true;
  idr_packet.header.sequenceNumber = 2;
  idr_packet.header.markerBit = 1;
  idr_packet.type.Video.is_first_packet_in_frame = true;
  idr_packet.frameType = kVideoFrameKey;
  idr_packet.type.Video.codec = kRtpVideoH264;
  data.insert(data.end(), {1, 2, 3});
  mock_on_complete_frame_callback_.AppendExpectedBitstream(
      kH264StartCode, sizeof(kH264StartCode));
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &idr_packet);
}

TEST_F(RtpVideoStreamReceiverTest, PaddingInMediaStream) {
  WebRtcRTPHeader header = GetDefaultPacket();
  std::vector<uint8_t> data;
  data.insert(data.end(), {1, 2, 3});
  header.header.payloadType = 99;
  header.type.Video.is_first_packet_in_frame = true;
  header.header.sequenceNumber = 2;
  header.header.markerBit = true;
  header.frameType = kVideoFrameKey;
  header.type.Video.codec = kRtpVideoGeneric;
  mock_on_complete_frame_callback_.AppendExpectedBitstream(data.data(),
                                                           data.size());

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &header);

  header.header.sequenceNumber = 3;
  rtp_video_stream_receiver_->OnReceivedPayloadData(nullptr, 0, &header);

  header.frameType = kVideoFrameDelta;
  header.header.sequenceNumber = 4;
  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &header);

  header.header.sequenceNumber = 6;
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &header);

  EXPECT_CALL(mock_on_complete_frame_callback_, DoOnCompleteFrame(_));
  header.header.sequenceNumber = 5;
  rtp_video_stream_receiver_->OnReceivedPayloadData(nullptr, 0, &header);
}

TEST_F(RtpVideoStreamReceiverTest, RequestKeyframeIfFirstFrameIsDelta) {
  WebRtcRTPHeader rtp_header;
  const std::vector<uint8_t> data({1, 2, 3, 4});
  memset(&rtp_header, 0, sizeof(rtp_header));
  rtp_header.header.sequenceNumber = 1;
  rtp_header.header.markerBit = 1;
  rtp_header.type.Video.is_first_packet_in_frame = true;
  rtp_header.frameType = kVideoFrameDelta;
  rtp_header.type.Video.codec = kRtpVideoGeneric;

  EXPECT_CALL(mock_key_frame_request_sender_, RequestKeyFrame());
  rtp_video_stream_receiver_->OnReceivedPayloadData(data.data(), data.size(),
                                                    &rtp_header);
}

}  // namespace webrtc
