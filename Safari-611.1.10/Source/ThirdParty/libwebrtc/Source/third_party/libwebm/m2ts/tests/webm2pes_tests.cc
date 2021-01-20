// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "m2ts/webm2pes.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "common/file_util.h"
#include "common/libwebm_util.h"
#include "m2ts/vpxpes_parser.h"
#include "testing/test_util.h"

namespace {

class Webm2PesTests : public ::testing::Test {
 public:
  // Constants for validating known values from input data.
  const std::uint8_t kMinVideoStreamId = 0xE0;
  const std::uint8_t kMaxVideoStreamId = 0xEF;
  const int kPesHeaderSize = 6;
  const int kPesOptionalHeaderStartOffset = kPesHeaderSize;
  const int kPesOptionalHeaderSize = 9;
  const int kPesOptionalHeaderMarkerValue = 0x2;
  const int kWebm2PesOptHeaderRemainingSize = 6;
  const int kBcmvHeaderSize = 10;

  Webm2PesTests() = default;
  ~Webm2PesTests() = default;

  void CreateAndLoadTestInput() {
    libwebm::Webm2Pes converter(input_file_name_, temp_file_name_.name());
    ASSERT_TRUE(converter.ConvertToFile());
    ASSERT_TRUE(parser_.Open(pes_file_name()));
  }

  bool VerifyPacketStartCode(const libwebm::VpxPesParser::PesHeader& header) {
    // PES packets all start with the byte sequence 0x0 0x0 0x1.
    if (header.start_code[0] != 0 || header.start_code[1] != 0 ||
        header.start_code[2] != 1) {
      return false;
    }
    return true;
  }

  const std::string& pes_file_name() const { return temp_file_name_.name(); }
  libwebm::VpxPesParser* parser() { return &parser_; }

 private:
  const libwebm::TempFileDeleter temp_file_name_;
  const std::string input_file_name_ =
      test::GetTestFilePath("bbb_480p_vp9_opus_1second.webm");
  libwebm::VpxPesParser parser_;
};

TEST_F(Webm2PesTests, CreatePesFile) { CreateAndLoadTestInput(); }

TEST_F(Webm2PesTests, CanParseFirstPacket) {
  CreateAndLoadTestInput();
  libwebm::VpxPesParser::PesHeader header;
  libwebm::VideoFrame frame;
  ASSERT_TRUE(parser()->ParseNextPacket(&header, &frame));
  EXPECT_TRUE(VerifyPacketStartCode(header));

  //   9 bytes: PES optional header
  //  10 bytes: BCMV Header
  //  83 bytes: frame
  // 102 bytes total in packet length field:
  const std::size_t kPesPayloadLength = 102;
  EXPECT_EQ(kPesPayloadLength, header.packet_length);

  EXPECT_GE(header.stream_id, kMinVideoStreamId);
  EXPECT_LE(header.stream_id, kMaxVideoStreamId);

  // Test PesOptionalHeader values.
  EXPECT_EQ(kPesOptionalHeaderMarkerValue, header.opt_header.marker);
  EXPECT_EQ(kWebm2PesOptHeaderRemainingSize, header.opt_header.remaining_size);
  EXPECT_EQ(0, header.opt_header.scrambling);
  EXPECT_EQ(0, header.opt_header.priority);
  EXPECT_EQ(0, header.opt_header.data_alignment);
  EXPECT_EQ(0, header.opt_header.copyright);
  EXPECT_EQ(0, header.opt_header.original);
  EXPECT_EQ(1, header.opt_header.has_pts);
  EXPECT_EQ(0, header.opt_header.has_dts);
  EXPECT_EQ(0, header.opt_header.unused_fields);

  // Test the BCMV header.
  // Note: The length field of the BCMV header includes its own length.
  const std::size_t kBcmvBaseLength = 10;
  const std::size_t kFirstFrameLength = 83;
  const libwebm::VpxPesParser::BcmvHeader kFirstBcmvHeader(kFirstFrameLength +
                                                           kBcmvBaseLength);
  EXPECT_TRUE(header.bcmv_header.Valid());
  EXPECT_EQ(kFirstBcmvHeader, header.bcmv_header);

  // Parse the next packet to confirm correct parse and consumption of payload.
  EXPECT_TRUE(parser()->ParseNextPacket(&header, &frame));
}

TEST_F(Webm2PesTests, CanMuxLargeBuffers) {
  const std::size_t kBufferSize = 100 * 1024;
  const std::int64_t kFakeTimestamp = libwebm::kNanosecondsPerSecond;
  libwebm::VideoFrame fake_frame(kFakeTimestamp, libwebm::VideoFrame::kVP9);
  ASSERT_TRUE(fake_frame.Init(kBufferSize));
  std::memset(fake_frame.buffer().data.get(), 0x80, kBufferSize);
  ASSERT_TRUE(fake_frame.SetBufferLength(kBufferSize));
  libwebm::PacketDataBuffer pes_packet_buffer;
  ASSERT_TRUE(
      libwebm::Webm2Pes::WritePesPacket(fake_frame, &pes_packet_buffer));

  // TODO(tomfinegan): Change VpxPesParser so it can read from a buffer, and get
  // rid of this extra step.
  libwebm::FilePtr pes_file(std::fopen(pes_file_name().c_str(), "wb"),
                            libwebm::FILEDeleter());
  ASSERT_EQ(pes_packet_buffer.size(),
            fwrite(&pes_packet_buffer[0], 1, pes_packet_buffer.size(),
                   pes_file.get()));
  fclose(pes_file.get());
  pes_file.release();

  libwebm::VpxPesParser parser;
  ASSERT_TRUE(parser.Open(pes_file_name()));
  libwebm::VpxPesParser::PesHeader header;
  libwebm::VideoFrame parsed_frame;
  ASSERT_TRUE(parser.ParseNextPacket(&header, &parsed_frame));
  EXPECT_EQ(fake_frame.nanosecond_pts(), parsed_frame.nanosecond_pts());
  EXPECT_EQ(fake_frame.buffer().length, parsed_frame.buffer().length);
  EXPECT_EQ(0, std::memcmp(fake_frame.buffer().data.get(),
                           parsed_frame.buffer().data.get(), kBufferSize));
}

TEST_F(Webm2PesTests, ParserConsumesAllInput) {
  CreateAndLoadTestInput();
  libwebm::VpxPesParser::PesHeader header;
  libwebm::VideoFrame frame;
  while (parser()->ParseNextPacket(&header, &frame) == true) {
    EXPECT_TRUE(VerifyPacketStartCode(header));
  }
  EXPECT_EQ(0, parser()->BytesAvailable());
}

}  // namespace

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
