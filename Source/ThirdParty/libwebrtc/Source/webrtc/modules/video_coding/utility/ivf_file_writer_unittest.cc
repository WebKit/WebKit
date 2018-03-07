/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/ivf_file_writer.h"

#include <memory>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/timeutils.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

namespace {
static const int kHeaderSize = 32;
static const int kFrameHeaderSize = 12;
static uint8_t dummy_payload[4] = {0, 1, 2, 3};
}  // namespace

class IvfFileWriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    file_name_ =
        webrtc::test::TempFilename(webrtc::test::OutputPath(), "test_file");
  }
  void TearDown() override { rtc::RemoveFile(file_name_); }

  bool WriteDummyTestFrames(VideoCodecType codec_type,
                            int width,
                            int height,
                            int num_frames,
                            bool use_capture_tims_ms) {
    EncodedImage frame;
    frame._buffer = dummy_payload;
    frame._encodedWidth = width;
    frame._encodedHeight = height;
    for (int i = 1; i <= num_frames; ++i) {
      frame._length = i % sizeof(dummy_payload);
      if (use_capture_tims_ms) {
        frame.capture_time_ms_ = i;
      } else {
        frame._timeStamp = i;
      }
      if (!file_writer_->WriteFrame(frame, codec_type))
        return false;
    }
    return true;
  }

  void VerifyIvfHeader(rtc::File* file,
                       const uint8_t fourcc[4],
                       int width,
                       int height,
                       uint32_t num_frames,
                       bool use_capture_tims_ms) {
    ASSERT_TRUE(file->IsOpen());
    uint8_t data[kHeaderSize];
    ASSERT_EQ(static_cast<size_t>(kHeaderSize), file->Read(data, kHeaderSize));

    uint8_t dkif[4] = {'D', 'K', 'I', 'F'};
    EXPECT_EQ(0, memcmp(dkif, data, 4));
    EXPECT_EQ(0u, ByteReader<uint16_t>::ReadLittleEndian(&data[4]));
    EXPECT_EQ(32u, ByteReader<uint16_t>::ReadLittleEndian(&data[6]));
    EXPECT_EQ(0, memcmp(fourcc, &data[8], 4));
    EXPECT_EQ(width, ByteReader<uint16_t>::ReadLittleEndian(&data[12]));
    EXPECT_EQ(height, ByteReader<uint16_t>::ReadLittleEndian(&data[14]));
    EXPECT_EQ(use_capture_tims_ms ? 1000u : 90000u,
              ByteReader<uint32_t>::ReadLittleEndian(&data[16]));
    EXPECT_EQ(1u, ByteReader<uint32_t>::ReadLittleEndian(&data[20]));
    EXPECT_EQ(num_frames, ByteReader<uint32_t>::ReadLittleEndian(&data[24]));
    EXPECT_EQ(0u, ByteReader<uint32_t>::ReadLittleEndian(&data[28]));
  }

  void VerifyDummyTestFrames(rtc::File* file, uint32_t num_frames) {
    const int kMaxFrameSize = 4;
    for (uint32_t i = 1; i <= num_frames; ++i) {
      uint8_t frame_header[kFrameHeaderSize];
      ASSERT_EQ(static_cast<unsigned int>(kFrameHeaderSize),
                file->Read(frame_header, kFrameHeaderSize));
      uint32_t frame_length =
          ByteReader<uint32_t>::ReadLittleEndian(&frame_header[0]);
      EXPECT_EQ(i % 4, frame_length);
      uint64_t timestamp =
          ByteReader<uint64_t>::ReadLittleEndian(&frame_header[4]);
      EXPECT_EQ(i, timestamp);

      uint8_t data[kMaxFrameSize] = {};
      ASSERT_EQ(frame_length,
                static_cast<uint32_t>(file->Read(data, frame_length)));
      EXPECT_EQ(0, memcmp(data, dummy_payload, frame_length));
    }
  }

  void RunBasicFileStructureTest(VideoCodecType codec_type,
                                 const uint8_t fourcc[4],
                                 bool use_capture_tims_ms) {
    file_writer_ = IvfFileWriter::Wrap(rtc::File::Open(file_name_), 0);
    ASSERT_TRUE(file_writer_.get());
    const int kWidth = 320;
    const int kHeight = 240;
    const int kNumFrames = 257;
    ASSERT_TRUE(WriteDummyTestFrames(codec_type, kWidth, kHeight, kNumFrames,
                                     use_capture_tims_ms));
    EXPECT_TRUE(file_writer_->Close());

    rtc::File out_file = rtc::File::Open(file_name_);
    VerifyIvfHeader(&out_file, fourcc, kWidth, kHeight, kNumFrames,
                    use_capture_tims_ms);
    VerifyDummyTestFrames(&out_file, kNumFrames);

    out_file.Close();
  }

  std::string file_name_;
  std::unique_ptr<IvfFileWriter> file_writer_;
};

TEST_F(IvfFileWriterTest, WritesBasicVP8FileNtpTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '8', '0'};
  RunBasicFileStructureTest(kVideoCodecVP8, fourcc, false);
}

TEST_F(IvfFileWriterTest, WritesBasicVP8FileMsTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '8', '0'};
  RunBasicFileStructureTest(kVideoCodecVP8, fourcc, true);
}

TEST_F(IvfFileWriterTest, WritesBasicVP9FileNtpTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '9', '0'};
  RunBasicFileStructureTest(kVideoCodecVP9, fourcc, false);
}

TEST_F(IvfFileWriterTest, WritesBasicVP9FileMsTimestamp) {
  const uint8_t fourcc[4] = {'V', 'P', '9', '0'};
  RunBasicFileStructureTest(kVideoCodecVP9, fourcc, true);
}

TEST_F(IvfFileWriterTest, WritesBasicH264FileNtpTimestamp) {
  const uint8_t fourcc[4] = {'H', '2', '6', '4'};
  RunBasicFileStructureTest(kVideoCodecH264, fourcc, false);
}

TEST_F(IvfFileWriterTest, WritesBasicH264FileMsTimestamp) {
  const uint8_t fourcc[4] = {'H', '2', '6', '4'};
  RunBasicFileStructureTest(kVideoCodecH264, fourcc, true);
}

TEST_F(IvfFileWriterTest, ClosesWhenReachesLimit) {
  const uint8_t fourcc[4] = {'V', 'P', '8', '0'};
  const int kWidth = 320;
  const int kHeight = 240;
  const int kNumFramesToWrite = 2;
  const int kNumFramesToFit = 1;

  file_writer_ = IvfFileWriter::Wrap(
      rtc::File::Open(file_name_),
      kHeaderSize +
          kNumFramesToFit * (kFrameHeaderSize + sizeof(dummy_payload)));
  ASSERT_TRUE(file_writer_.get());

  ASSERT_FALSE(WriteDummyTestFrames(kVideoCodecVP8, kWidth, kHeight,
                                    kNumFramesToWrite, true));
  ASSERT_FALSE(file_writer_->Close());

  rtc::File out_file = rtc::File::Open(file_name_);
  VerifyIvfHeader(&out_file, fourcc, kWidth, kHeight, kNumFramesToFit, true);
  VerifyDummyTestFrames(&out_file, kNumFramesToFit);

  out_file.Close();
}

}  // namespace webrtc
