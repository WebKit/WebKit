/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <memory>

#include "test/gtest.h"
#include "test/rtp_file_reader.h"
#include "test/rtp_file_writer.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

class RtpFileWriterTest : public ::testing::Test {
 public:
  void Init(const std::string& filename) {
    filename_ = test::OutputPath() + filename;
    rtp_writer_.reset(
        test::RtpFileWriter::Create(test::RtpFileWriter::kRtpDump, filename_));
  }

  void WriteRtpPackets(int num_packets) {
    ASSERT_TRUE(rtp_writer_.get() != NULL);
    test::RtpPacket packet;
    for (int i = 1; i <= num_packets; ++i) {
      packet.length = i;
      packet.original_length = i;
      packet.time_ms = i;
      memset(packet.data, i, packet.length);
      EXPECT_TRUE(rtp_writer_->WritePacket(&packet));
    }
  }

  void CloseOutputFile() { rtp_writer_.reset(); }

  void VerifyFileContents(int expected_packets) {
    ASSERT_TRUE(rtp_writer_.get() == NULL)
        << "Must call CloseOutputFile before VerifyFileContents";
    std::unique_ptr<test::RtpFileReader> rtp_reader(
        test::RtpFileReader::Create(test::RtpFileReader::kRtpDump, filename_));
    ASSERT_TRUE(rtp_reader.get() != NULL);
    test::RtpPacket packet;
    int i = 0;
    while (rtp_reader->NextPacket(&packet)) {
      ++i;
      EXPECT_EQ(static_cast<size_t>(i), packet.length);
      EXPECT_EQ(static_cast<size_t>(i), packet.original_length);
      EXPECT_EQ(static_cast<uint32_t>(i), packet.time_ms);
      for (int j = 0; j < i; ++j) {
        EXPECT_EQ(i, packet.data[j]);
      }
    }
    EXPECT_EQ(expected_packets, i);
  }

 private:
  std::unique_ptr<test::RtpFileWriter> rtp_writer_;
  std::string filename_;
};

TEST_F(RtpFileWriterTest, WriteToRtpDump) {
  Init("test_rtp_file_writer.rtp");
  WriteRtpPackets(10);
  CloseOutputFile();
  VerifyFileContents(10);
}

}  // namespace webrtc
