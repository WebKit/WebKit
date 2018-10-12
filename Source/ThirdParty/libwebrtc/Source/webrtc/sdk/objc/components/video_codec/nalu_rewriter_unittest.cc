/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include <memory>

#include "common_video/h264/h264_common.h"
#include "rtc_base/arraysize.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"
#include "test/gtest.h"

namespace webrtc {

using H264::kSps;

static const uint8_t NALU_TEST_DATA_0[] = {0xAA, 0xBB, 0xCC};
static const uint8_t NALU_TEST_DATA_1[] = {0xDE, 0xAD, 0xBE, 0xEF};

TEST(H264VideoToolboxNaluTest, TestCreateVideoFormatDescription) {
  const uint8_t sps_pps_buffer[] = {
      // SPS nalu.
      0x00, 0x00, 0x00, 0x01, 0x27, 0x42, 0x00, 0x1E, 0xAB, 0x40, 0xF0, 0x28,
      0xD3, 0x70, 0x20, 0x20, 0x20, 0x20,
      // PPS nalu.
      0x00, 0x00, 0x00, 0x01, 0x28, 0xCE, 0x3C, 0x30};
  CMVideoFormatDescriptionRef description =
      CreateVideoFormatDescription(sps_pps_buffer, arraysize(sps_pps_buffer));
  EXPECT_TRUE(description);
  if (description) {
    CFRelease(description);
    description = nullptr;
  }

  const uint8_t sps_pps_not_at_start_buffer[] = {
      // Add some non-SPS/PPS NALUs at the beginning
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x01,
      0xAB, 0x33, 0x21,
      // SPS nalu.
      0x00, 0x00, 0x01, 0x27, 0x42, 0x00, 0x1E, 0xAB, 0x40, 0xF0, 0x28, 0xD3,
      0x70, 0x20, 0x20, 0x20, 0x20,
      // PPS nalu.
      0x00, 0x00, 0x01, 0x28, 0xCE, 0x3C, 0x30};
  description = CreateVideoFormatDescription(
      sps_pps_not_at_start_buffer, arraysize(sps_pps_not_at_start_buffer));
  EXPECT_TRUE(description);
  if (description) {
    CFRelease(description);
    description = nullptr;
  }

  const uint8_t other_buffer[] = {0x00, 0x00, 0x00, 0x01, 0x28};
  EXPECT_FALSE(
      CreateVideoFormatDescription(other_buffer, arraysize(other_buffer)));
}

TEST(AnnexBBufferReaderTest, TestReadEmptyInput) {
  const uint8_t annex_b_test_data[] = {0x00};
  AnnexBBufferReader reader(annex_b_test_data, 0);
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_FALSE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(nullptr, nalu);
  EXPECT_EQ(0u, nalu_length);
}

TEST(AnnexBBufferReaderTest, TestReadSingleNalu) {
  const uint8_t annex_b_test_data[] = {0x00, 0x00, 0x00, 0x01, 0xAA};
  AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  EXPECT_EQ(arraysize(annex_b_test_data), reader.BytesRemaining());
  EXPECT_TRUE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(annex_b_test_data + 4, nalu);
  EXPECT_EQ(1u, nalu_length);
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_FALSE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(nullptr, nalu);
  EXPECT_EQ(0u, nalu_length);
}

TEST(AnnexBBufferReaderTest, TestReadSingleNalu3ByteHeader) {
  const uint8_t annex_b_test_data[] = {0x00, 0x00, 0x01, 0xAA};
  AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  EXPECT_EQ(arraysize(annex_b_test_data), reader.BytesRemaining());
  EXPECT_TRUE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(annex_b_test_data + 3, nalu);
  EXPECT_EQ(1u, nalu_length);
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_FALSE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(nullptr, nalu);
  EXPECT_EQ(0u, nalu_length);
}

TEST(AnnexBBufferReaderTest, TestReadMissingNalu) {
  // clang-format off
  const uint8_t annex_b_test_data[] = {0x01,
                                       0x00, 0x01,
                                       0x00, 0x00, 0x00, 0xFF};
  // clang-format on
  AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_FALSE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(nullptr, nalu);
  EXPECT_EQ(0u, nalu_length);
}

TEST(AnnexBBufferReaderTest, TestReadMultipleNalus) {
  // clang-format off
  const uint8_t annex_b_test_data[] = {0x00, 0x00, 0x00, 0x01, 0xFF,
                                       0x01,
                                       0x00, 0x01,
                                       0x00, 0x00, 0x00, 0xFF,
                                       0x00, 0x00, 0x01, 0xAA, 0xBB};
  // clang-format on
  AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  EXPECT_EQ(arraysize(annex_b_test_data), reader.BytesRemaining());
  EXPECT_TRUE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(annex_b_test_data + 4, nalu);
  EXPECT_EQ(8u, nalu_length);
  EXPECT_EQ(6u, reader.BytesRemaining());
  EXPECT_TRUE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(annex_b_test_data + 16, nalu);
  EXPECT_EQ(2u, nalu_length);
  EXPECT_EQ(0u, reader.BytesRemaining());
  EXPECT_FALSE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(nullptr, nalu);
  EXPECT_EQ(0u, nalu_length);
}

TEST(AnnexBBufferReaderTest, TestFindNextNaluOfType) {
  const uint8_t notSps = 0x1F;
  const uint8_t annex_b_test_data[] = {
      0x00, 0x00,   0x00, 0x01,   kSps, 0x00,   0x00, 0x01, notSps,
      0x00, 0x00,   0x01, notSps, 0xDD, 0x00,   0x00, 0x01, notSps,
      0xEE, 0xFF,   0x00, 0x00,   0x00, 0xFF,   0x00, 0x00, 0x00,
      0x01, 0x00,   0x00, 0x00,   0x01, kSps,   0xBB, 0x00, 0x00,
      0x01, notSps, 0x00, 0x00,   0x01, notSps, 0xDD, 0x00, 0x00,
      0x01, notSps, 0xEE, 0xFF,   0x00, 0x00,   0x00, 0x01};

  AnnexBBufferReader reader(annex_b_test_data, arraysize(annex_b_test_data));
  const uint8_t* nalu = nullptr;
  size_t nalu_length = 0;
  EXPECT_EQ(arraysize(annex_b_test_data), reader.BytesRemaining());
  EXPECT_TRUE(reader.FindNextNaluOfType(kSps));
  EXPECT_TRUE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(annex_b_test_data + 4, nalu);
  EXPECT_EQ(1u, nalu_length);

  EXPECT_TRUE(reader.FindNextNaluOfType(kSps));
  EXPECT_TRUE(reader.ReadNalu(&nalu, &nalu_length));
  EXPECT_EQ(annex_b_test_data + 32, nalu);
  EXPECT_EQ(2u, nalu_length);

  EXPECT_FALSE(reader.FindNextNaluOfType(kSps));
  EXPECT_FALSE(reader.ReadNalu(&nalu, &nalu_length));
}

TEST(AvccBufferWriterTest, TestEmptyOutputBuffer) {
  const uint8_t expected_buffer[] = {0x00};
  const size_t buffer_size = 1;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memset(buffer.get(), 0, buffer_size);
  AvccBufferWriter writer(buffer.get(), 0);
  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_FALSE(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  EXPECT_EQ(0,
            memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

TEST(AvccBufferWriterTest, TestWriteSingleNalu) {
  const uint8_t expected_buffer[] = {
      0x00, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC,
  };
  const size_t buffer_size = arraysize(NALU_TEST_DATA_0) + 4;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  AvccBufferWriter writer(buffer.get(), buffer_size);
  EXPECT_EQ(buffer_size, writer.BytesRemaining());
  EXPECT_TRUE(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_FALSE(writer.WriteNalu(NALU_TEST_DATA_1, arraysize(NALU_TEST_DATA_1)));
  EXPECT_EQ(0,
            memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

TEST(AvccBufferWriterTest, TestWriteMultipleNalus) {
  // clang-format off
  const uint8_t expected_buffer[] = {
      0x00, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC,
      0x00, 0x00, 0x00, 0x04, 0xDE, 0xAD, 0xBE, 0xEF
  };
  // clang-format on
  const size_t buffer_size =
      arraysize(NALU_TEST_DATA_0) + arraysize(NALU_TEST_DATA_1) + 8;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  AvccBufferWriter writer(buffer.get(), buffer_size);
  EXPECT_EQ(buffer_size, writer.BytesRemaining());
  EXPECT_TRUE(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  EXPECT_EQ(buffer_size - (arraysize(NALU_TEST_DATA_0) + 4),
            writer.BytesRemaining());
  EXPECT_TRUE(writer.WriteNalu(NALU_TEST_DATA_1, arraysize(NALU_TEST_DATA_1)));
  EXPECT_EQ(0u, writer.BytesRemaining());
  EXPECT_EQ(0,
            memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

TEST(AvccBufferWriterTest, TestOverflow) {
  const uint8_t expected_buffer[] = {0x00, 0x00, 0x00};
  const size_t buffer_size = arraysize(NALU_TEST_DATA_0);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memset(buffer.get(), 0, buffer_size);
  AvccBufferWriter writer(buffer.get(), buffer_size);
  EXPECT_EQ(buffer_size, writer.BytesRemaining());
  EXPECT_FALSE(writer.WriteNalu(NALU_TEST_DATA_0, arraysize(NALU_TEST_DATA_0)));
  EXPECT_EQ(buffer_size, writer.BytesRemaining());
  EXPECT_EQ(0,
            memcmp(expected_buffer, buffer.get(), arraysize(expected_buffer)));
}

}  // namespace webrtc
