/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/base/rtpdump.h"
#include "webrtc/media/base/rtputils.h"
#include "webrtc/media/base/testutils.h"

namespace cricket {

static const uint32_t kTestSsrc = 1;

// Test that we read the correct header fields from the RTP/RTCP packet.
TEST(RtpDumpTest, ReadRtpDumpPacket) {
  rtc::ByteBufferWriter rtp_buf;
  RtpTestUtility::kTestRawRtpPackets[0].WriteToByteBuffer(kTestSsrc, &rtp_buf);
  RtpDumpPacket rtp_packet(rtp_buf.Data(), rtp_buf.Length(), 0, false);

  int payload_type;
  int seq_num;
  uint32_t ts;
  uint32_t ssrc;
  int rtcp_type;
  EXPECT_FALSE(rtp_packet.is_rtcp());
  EXPECT_TRUE(rtp_packet.IsValidRtpPacket());
  EXPECT_FALSE(rtp_packet.IsValidRtcpPacket());
  EXPECT_TRUE(rtp_packet.GetRtpPayloadType(&payload_type));
  EXPECT_EQ(0, payload_type);
  EXPECT_TRUE(rtp_packet.GetRtpSeqNum(&seq_num));
  EXPECT_EQ(0, seq_num);
  EXPECT_TRUE(rtp_packet.GetRtpTimestamp(&ts));
  EXPECT_EQ(0U, ts);
  EXPECT_TRUE(rtp_packet.GetRtpSsrc(&ssrc));
  EXPECT_EQ(kTestSsrc, ssrc);
  EXPECT_FALSE(rtp_packet.GetRtcpType(&rtcp_type));

  rtc::ByteBufferWriter rtcp_buf;
  RtpTestUtility::kTestRawRtcpPackets[0].WriteToByteBuffer(&rtcp_buf);
  RtpDumpPacket rtcp_packet(rtcp_buf.Data(), rtcp_buf.Length(), 0, true);

  EXPECT_TRUE(rtcp_packet.is_rtcp());
  EXPECT_FALSE(rtcp_packet.IsValidRtpPacket());
  EXPECT_TRUE(rtcp_packet.IsValidRtcpPacket());
  EXPECT_TRUE(rtcp_packet.GetRtcpType(&rtcp_type));
  EXPECT_EQ(0, rtcp_type);
}

// Test that we read only the RTP dump file.
TEST(RtpDumpTest, ReadRtpDumpFile) {
  RtpDumpPacket packet;
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  std::unique_ptr<RtpDumpReader> reader;

  // Write a RTP packet to the stream, which is a valid RTP dump. Next, we will
  // change the first line to make the RTP dump valid or invalid.
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(1, false, kTestSsrc, &writer));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(rtc::SR_SUCCESS, reader->ReadPacket(&packet));

  // The first line is correct.
  stream.Rewind();
  const char new_line[] = "#!rtpplay1.0 1.1.1.1/1\n";
  EXPECT_EQ(rtc::SR_SUCCESS,
            stream.WriteAll(new_line, strlen(new_line), NULL, NULL));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(rtc::SR_SUCCESS, reader->ReadPacket(&packet));

  // The first line is not correct: not started with #!rtpplay1.0.
  stream.Rewind();
  const char new_line2[] = "#!rtpplaz1.0 0.0.0.0/0\n";
  EXPECT_EQ(rtc::SR_SUCCESS,
            stream.WriteAll(new_line2, strlen(new_line2), NULL, NULL));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(rtc::SR_ERROR, reader->ReadPacket(&packet));

  // The first line is not correct: no port.
  stream.Rewind();
  const char new_line3[] = "#!rtpplay1.0 0.0.0.0//\n";
  EXPECT_EQ(rtc::SR_SUCCESS,
            stream.WriteAll(new_line3, strlen(new_line3), NULL, NULL));
  stream.Rewind();
  reader.reset(new RtpDumpReader(&stream));
  EXPECT_EQ(rtc::SR_ERROR, reader->ReadPacket(&packet));
}

// Test that we read the same RTP packets that rtp dump writes.
TEST(RtpDumpTest, WriteReadSameRtp) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));

  // Check stream has only RtpTestUtility::GetTestPacketCount() packets.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(rtc::SR_SUCCESS, reader.ReadPacket(&packet));
    uint32_t ssrc;
    EXPECT_TRUE(GetRtpSsrc(&packet.data[0], packet.data.size(), &ssrc));
    EXPECT_EQ(kTestSsrc, ssrc);
  }
  // No more packets to read.
  EXPECT_EQ(rtc::SR_EOS, reader.ReadPacket(&packet));

  // Rewind the stream and read again with a specified ssrc.
  stream.Rewind();
  RtpDumpReader reader_w_ssrc(&stream);
  const uint32_t send_ssrc = kTestSsrc + 1;
  reader_w_ssrc.SetSsrc(send_ssrc);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(rtc::SR_SUCCESS, reader_w_ssrc.ReadPacket(&packet));
    EXPECT_FALSE(packet.is_rtcp());
    EXPECT_EQ(packet.original_data_len, packet.data.size());
    uint32_t ssrc;
    EXPECT_TRUE(GetRtpSsrc(&packet.data[0], packet.data.size(), &ssrc));
    EXPECT_EQ(send_ssrc, ssrc);
  }
  // No more packets to read.
  EXPECT_EQ(rtc::SR_EOS, reader_w_ssrc.ReadPacket(&packet));
}

// Test that we read the same RTCP packets that rtp dump writes.
TEST(RtpDumpTest, WriteReadSameRtcp) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));

  // Check stream has only RtpTestUtility::GetTestPacketCount() packets.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  reader.SetSsrc(kTestSsrc + 1);  // Does not affect RTCP packet.
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(rtc::SR_SUCCESS, reader.ReadPacket(&packet));
    EXPECT_TRUE(packet.is_rtcp());
    EXPECT_EQ(0U, packet.original_data_len);
  }
  // No more packets to read.
  EXPECT_EQ(rtc::SR_EOS, reader.ReadPacket(&packet));
}

// Test dumping only RTP packet headers.
TEST(RtpDumpTest, WriteReadRtpHeadersOnly) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  writer.set_packet_filter(PF_RTPHEADER);

  // Write some RTP and RTCP packets. RTP packets should only have headers;
  // RTCP packets should be eaten.
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  stream.Rewind();

  // Check that only RTP packet headers are present.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(rtc::SR_SUCCESS, reader.ReadPacket(&packet));
    EXPECT_FALSE(packet.is_rtcp());
    size_t len = 0;
    packet.GetRtpHeaderLen(&len);
    EXPECT_EQ(len, packet.data.size());
    EXPECT_GT(packet.original_data_len, packet.data.size());
  }
  // No more packets to read.
  EXPECT_EQ(rtc::SR_EOS, reader.ReadPacket(&packet));
}

// Test dumping only RTCP packets.
TEST(RtpDumpTest, WriteReadRtcpOnly) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  writer.set_packet_filter(PF_RTCPPACKET);

  // Write some RTP and RTCP packets. RTP packets should be eaten.
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  stream.Rewind();

  // Check that only RTCP packets are present.
  RtpDumpPacket packet;
  RtpDumpReader reader(&stream);
  for (size_t i = 0; i < RtpTestUtility::GetTestPacketCount(); ++i) {
    EXPECT_EQ(rtc::SR_SUCCESS, reader.ReadPacket(&packet));
    EXPECT_TRUE(packet.is_rtcp());
    EXPECT_EQ(0U, packet.original_data_len);
  }
  // No more packets to read.
  EXPECT_EQ(rtc::SR_EOS, reader.ReadPacket(&packet));
}

// Test that RtpDumpLoopReader reads RTP packets continously and the elapsed
// time, the sequence number, and timestamp are maintained properly.
TEST(RtpDumpTest, LoopReadRtp) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), false, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      3 * RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));
}

// Test that RtpDumpLoopReader reads RTCP packets continously and the elapsed
// time is maintained properly.
TEST(RtpDumpTest, LoopReadRtcp) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(
      RtpTestUtility::GetTestPacketCount(), true, kTestSsrc, &writer));
  EXPECT_TRUE(RtpTestUtility::VerifyTestPacketsFromStream(
      3 * RtpTestUtility::GetTestPacketCount(), &stream, kTestSsrc));
}

// Test that RtpDumpLoopReader reads continously from stream with a single RTP
// packets.
TEST(RtpDumpTest, LoopReadSingleRtp) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(1, false, kTestSsrc, &writer));

  // The regular reader can read only one packet.
  RtpDumpPacket packet;
  stream.Rewind();
  RtpDumpReader reader(&stream);
  EXPECT_EQ(rtc::SR_SUCCESS, reader.ReadPacket(&packet));
  EXPECT_EQ(rtc::SR_EOS, reader.ReadPacket(&packet));

  // The loop reader reads three packets from the input stream.
  stream.Rewind();
  RtpDumpLoopReader loop_reader(&stream);
  EXPECT_EQ(rtc::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(rtc::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(rtc::SR_SUCCESS, loop_reader.ReadPacket(&packet));
}

// Test that RtpDumpLoopReader reads continously from stream with a single RTCP
// packets.
TEST(RtpDumpTest, LoopReadSingleRtcp) {
  rtc::MemoryStream stream;
  RtpDumpWriter writer(&stream);
  ASSERT_TRUE(RtpTestUtility::WriteTestPackets(1, true, kTestSsrc, &writer));

  // The regular reader can read only one packet.
  RtpDumpPacket packet;
  stream.Rewind();
  RtpDumpReader reader(&stream);
  EXPECT_EQ(rtc::SR_SUCCESS, reader.ReadPacket(&packet));
  EXPECT_EQ(rtc::SR_EOS, reader.ReadPacket(&packet));

  // The loop reader reads three packets from the input stream.
  stream.Rewind();
  RtpDumpLoopReader loop_reader(&stream);
  EXPECT_EQ(rtc::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(rtc::SR_SUCCESS, loop_reader.ReadPacket(&packet));
  EXPECT_EQ(rtc::SR_SUCCESS, loop_reader.ReadPacket(&packet));
}

}  // namespace cricket
