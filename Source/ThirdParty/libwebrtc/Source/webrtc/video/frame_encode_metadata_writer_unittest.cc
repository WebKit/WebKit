/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_encode_metadata_writer.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/rtp_packet_infos.h"
#include "api/scoped_refptr.h"
#include "api/video/color_space.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/h264/h264_common.h"
#include "common_video/test/utilities.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;

namespace webrtc {
namespace test {
namespace {

const scoped_refptr<I420Buffer> kFrameBuffer = I420Buffer::Create(4, 4);

inline size_t FrameSize(const size_t& min_frame_size,
                        const size_t& max_frame_size,
                        const int& s,
                        const int& i) {
  return min_frame_size + (s + 1) * i % (max_frame_size - min_frame_size);
}

struct DroppedFrameInfo {
  uint32_t rtp_timestamp;
  int spatial_id;
  bool is_end_of_temporal_unit;
};

class FakeEncodedImageCallback : public EncodedImageCallback {
 public:
  FakeEncodedImageCallback() : num_frames_dropped_(0) {}
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    return Result(Result::OK);
  }
  void OnFrameDropped(uint32_t rtp_timestamp,
                      int spatial_id,
                      bool is_end_of_temporal_unit) override {
    ++num_frames_dropped_;
    dropped_frames_.push_back(
        {rtp_timestamp, spatial_id, is_end_of_temporal_unit});
  }
  size_t GetNumFramesDropped() { return num_frames_dropped_; }
  const std::vector<DroppedFrameInfo>& GetDroppedFrames() const {
    return dropped_frames_;
  }

 private:
  size_t num_frames_dropped_;
  std::vector<DroppedFrameInfo> dropped_frames_;
};

enum class FrameType {
  kNormal,
  kTiming,
  kDropped,
};

bool IsTimingFrame(const EncodedImage& image) {
  return image.timing_.flags != VideoSendTiming::kInvalid &&
         image.timing_.flags != VideoSendTiming::kNotTriggered;
}

// Emulates `num_frames` on `num_streams` frames with capture timestamps
// increased by 1 from 0. Size of each frame is between
// `min_frame_size` and `max_frame_size`, outliers are counted relatevely to
// `average_frame_sizes[]` for each stream.
std::vector<std::vector<FrameType>> GetTimingFrames(
    const int64_t delay_ms,
    const size_t min_frame_size,
    const size_t max_frame_size,
    std::vector<size_t> average_frame_sizes,
    const int num_streams,
    const int num_frames) {
  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  VideoCodec codec_settings;
  codec_settings.numberOfSimulcastStreams = num_streams;
  codec_settings.timing_frame_thresholds = {
      .delay_ms = delay_ms,
      .outlier_ratio_percent = kDefaultOutlierFrameSizePercent};
  encode_timer.OnEncoderInit(codec_settings);
  const size_t kFramerate = 30;
  VideoBitrateAllocation bitrate_allocation;
  for (int si = 0; si < num_streams; ++si) {
    bitrate_allocation.SetBitrate(si, 0,
                                  average_frame_sizes[si] * 8 * kFramerate);
  }
  encode_timer.OnSetRates(bitrate_allocation, kFramerate);

  std::vector<std::vector<FrameType>> result(num_streams);
  int64_t current_timestamp = 0;
  for (int i = 0; i < num_frames; ++i) {
    current_timestamp += 1;
    VideoFrame frame = VideoFrame::Builder()
                           .set_rtp_timestamp(current_timestamp * 90)
                           .set_timestamp_ms(current_timestamp)
                           .set_video_frame_buffer(kFrameBuffer)
                           .build();
    encode_timer.OnEncodeStarted(frame);
    for (int si = 0; si < num_streams; ++si) {
      // every (5+s)-th frame is dropped on s-th stream by design.
      bool dropped = i % (5 + si) == 0;

      EncodedImage image;
      image.SetEncodedData(EncodedImageBuffer::Create(max_frame_size));
      image.set_size(FrameSize(min_frame_size, max_frame_size, si, i));
      image.capture_time_ms_ = current_timestamp;
      image.SetRtpTimestamp(static_cast<uint32_t>(current_timestamp * 90));
      image.SetSpatialIndex(si);

      if (dropped) {
        result[si].push_back(FrameType::kDropped);
        continue;
      }

      encode_timer.FillMetadataAndTimingInfo(si, &image);

      if (IsTimingFrame(image)) {
        result[si].push_back(FrameType::kTiming);
      } else {
        result[si].push_back(FrameType::kNormal);
      }
    }
  }
  return result;
}
}  // namespace

TEST(FrameEncodeMetadataWriterTest, MarksTimingFramesPeriodicallyTogether) {
  const int64_t kDelayMs = 29;
  const size_t kMinFrameSize = 10;
  const size_t kMaxFrameSize = 20;
  const int kNumFrames = 1000;
  const int kNumStreams = 3;
  // No outliers as 1000 is larger than anything from range [10,20].
  const std::vector<size_t> kAverageSize = {1000, 1000, 1000};
  auto frames = GetTimingFrames(kDelayMs, kMinFrameSize, kMaxFrameSize,
                                kAverageSize, kNumStreams, kNumFrames);
  // Timing frames should be tirggered every delayMs.
  // As no outliers are expected, frames on all streams have to be
  // marked together.
  int last_timing_frame = -1;
  for (int i = 0; i < kNumFrames; ++i) {
    int num_normal = 0;
    int num_timing = 0;
    int num_dropped = 0;
    for (int s = 0; s < kNumStreams; ++s) {
      if (frames[s][i] == FrameType::kTiming) {
        ++num_timing;
      } else if (frames[s][i] == FrameType::kNormal) {
        ++num_normal;
      } else {
        ++num_dropped;
      }
    }
    // Can't have both normal and timing frames at the same timstamp.
    EXPECT_TRUE(num_timing == 0 || num_normal == 0);
    if (num_dropped < kNumStreams) {
      if (last_timing_frame == -1 || i >= last_timing_frame + kDelayMs) {
        // If didn't have timing frames for a period, current sent frame has to
        // be one. No normal frames should be sent.
        EXPECT_EQ(num_normal, 0);
      } else {
        // No unneeded timing frames should be sent.
        EXPECT_EQ(num_timing, 0);
      }
    }
    if (num_timing > 0)
      last_timing_frame = i;
  }
}

TEST(FrameEncodeMetadataWriterTest, MarksOutliers) {
  const int64_t kDelayMs = 29;
  const size_t kMinFrameSize = 2495;
  const size_t kMaxFrameSize = 2505;
  const int kNumFrames = 1000;
  const int kNumStreams = 3;
  // Possible outliers as 1000 lies in range [995, 1005].
  const std::vector<size_t> kAverageSize = {998, 1000, 1004};
  auto frames = GetTimingFrames(kDelayMs, kMinFrameSize, kMaxFrameSize,
                                kAverageSize, kNumStreams, kNumFrames);
  // All outliers should be marked.
  for (int i = 0; i < kNumFrames; ++i) {
    for (int s = 0; s < kNumStreams; ++s) {
      if (FrameSize(kMinFrameSize, kMaxFrameSize, s, i) >=
          kAverageSize[s] * kDefaultOutlierFrameSizePercent / 100) {
        // Too big frame. May be dropped or timing, but not normal.
        EXPECT_NE(frames[s][i], FrameType::kNormal);
      }
    }
  }
}

TEST(FrameEncodeMetadataWriterTest, NoTimingFrameIfNoEncodeStartTime) {
  int64_t timestamp = 1;
  constexpr size_t kFrameSize = 500;
  EncodedImage image;
  image.SetEncodedData(EncodedImageBuffer::Create(kFrameSize));
  image.capture_time_ms_ = timestamp;
  image.SetRtpTimestamp(static_cast<uint32_t>(timestamp * 90));

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  VideoCodec codec_settings;
  // Make all frames timing frames.
  codec_settings.timing_frame_thresholds.delay_ms = 1;
  encode_timer.OnEncoderInit(codec_settings);
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  // Verify a single frame works with encode start time set.
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(timestamp)
                         .set_rtp_timestamp(timestamp * 90)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_TRUE(IsTimingFrame(image));

  // New frame, now skip OnEncodeStarted. Should not result in timing frame.
  image.capture_time_ms_ = ++timestamp;
  image.SetRtpTimestamp(static_cast<uint32_t>(timestamp * 90));
  image.timing_ = EncodedImage::Timing();
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_FALSE(IsTimingFrame(image));
}

TEST(FrameEncodeMetadataWriterTest, NotifiesAboutDroppedFrames) {
  const int64_t kTimestampMs1 = 47721840;
  const int64_t kTimestampMs2 = 47721850;
  const int64_t kTimestampMs3 = 47721860;
  const int64_t kTimestampMs4 = 47721870;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  EncodedImage image;
  VideoFrame frame = VideoFrame::Builder()
                         .set_rtp_timestamp(kTimestampMs1 * 90)
                         .set_timestamp_ms(kTimestampMs1)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();

  image.capture_time_ms_ = kTimestampMs1;
  image.SetRtpTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  frame.set_rtp_timestamp(image.capture_time_ms_ * 90);
  frame.set_timestamp_us(image.capture_time_ms_ * 1000);
  encode_timer.OnEncodeStarted(frame);

  EXPECT_EQ(0u, sink.GetNumFramesDropped());
  encode_timer.FillMetadataAndTimingInfo(0, &image);

  image.capture_time_ms_ = kTimestampMs2;
  image.SetRtpTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  image.timing_ = EncodedImage::Timing();
  frame.set_rtp_timestamp(image.capture_time_ms_ * 90);
  frame.set_timestamp_us(image.capture_time_ms_ * 1000);
  encode_timer.OnEncodeStarted(frame);
  // No OnEncodedImageCall for timestamp2. Yet, at this moment it's not known
  // that frame with timestamp2 was dropped.
  EXPECT_EQ(0u, sink.GetNumFramesDropped());

  image.capture_time_ms_ = kTimestampMs3;
  image.SetRtpTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  image.timing_ = EncodedImage::Timing();
  frame.set_rtp_timestamp(image.capture_time_ms_ * 90);
  frame.set_timestamp_us(image.capture_time_ms_ * 1000);
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_EQ(1u, sink.GetNumFramesDropped());

  image.capture_time_ms_ = kTimestampMs4;
  image.SetRtpTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  image.timing_ = EncodedImage::Timing();
  frame.set_rtp_timestamp(image.capture_time_ms_ * 90);
  frame.set_timestamp_us(image.capture_time_ms_ * 1000);
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_EQ(1u, sink.GetNumFramesDropped());
}

TEST(FrameEncodeMetadataWriterTest, RestoresCaptureTimestamps) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  image.capture_time_ms_ = kTimestampMs;  // Correct timestamp.
  image.SetRtpTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(image.capture_time_ms_)
                         .set_rtp_timestamp(image.capture_time_ms_ * 90)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  image.capture_time_ms_ = 0;  // Incorrect timestamp.
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_EQ(kTimestampMs, image.capture_time_ms_);
}

TEST(FrameEncodeMetadataWriterTest, CopiesRotation) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(kTimestampMs)
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_rotation(kVideoRotation_180)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_EQ(kVideoRotation_180, image.rotation_);
}

TEST(FrameEncodeMetadataWriterTest, SetsContentType) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  VideoCodec codec;
  codec.mode = VideoCodecMode::kScreensharing;
  encode_timer.OnEncoderInit(codec);
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(kTimestampMs)
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_rotation(kVideoRotation_180)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_EQ(VideoContentType::SCREENSHARE, image.content_type_);
}

TEST(FrameEncodeMetadataWriterTest, CopiesColorSpace) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  ColorSpace color_space = CreateTestColorSpace(/*with_hdr_metadata=*/true);
  image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(kTimestampMs)
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_color_space(color_space)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  ASSERT_NE(image.ColorSpace(), nullptr);
  EXPECT_EQ(color_space, *image.ColorSpace());
}

TEST(FrameEncodeMetadataWriterTest, SetsIsSteadyStateRefreshFrame) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
  VideoFrame not_refresh_frame = VideoFrame::Builder()
                                     .set_timestamp_ms(kTimestampMs)
                                     .set_rtp_timestamp(kTimestampMs * 90)
                                     .set_video_frame_buffer(kFrameBuffer)
                                     .build();
  encode_timer.OnEncodeStarted(not_refresh_frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_FALSE(image.IsSteadyStateRefreshFrame());

  VideoFrame::UpdateRect empty_update_rect;
  empty_update_rect.MakeEmptyUpdate();
  VideoFrame refresh_frame = VideoFrame::Builder()
                                 .set_timestamp_ms(kTimestampMs)
                                 .set_rtp_timestamp(kTimestampMs * 90)
                                 .set_update_rect(empty_update_rect)
                                 .set_video_frame_buffer(kFrameBuffer)
                                 .build();
  encode_timer.OnEncodeStarted(refresh_frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_TRUE(image.IsSteadyStateRefreshFrame());
}

TEST(FrameEncodeMetadataWriterTest, KeyFramesAreNotMarkedAsSteadyStateRefresh) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));

  VideoFrame::UpdateRect empty_update_rect;
  empty_update_rect.MakeEmptyUpdate();
  VideoFrame refresh_frame = VideoFrame::Builder()
                                 .set_timestamp_ms(kTimestampMs)
                                 .set_rtp_timestamp(kTimestampMs * 90)
                                 .set_update_rect(empty_update_rect)
                                 .set_video_frame_buffer(kFrameBuffer)
                                 .build();
  encode_timer.OnEncodeStarted(refresh_frame);
  image.set_frame_type(VideoFrameType::kVideoFrameKey);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_FALSE(image.IsSteadyStateRefreshFrame());
}

TEST(FrameEncodeMetadataWriterTest, CopiesPacketInfos) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  RtpPacketInfos packet_infos = CreatePacketInfos(3);
  image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(kTimestampMs)
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_packet_infos(packet_infos)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  encode_timer.FillMetadataAndTimingInfo(0, &image);
  EXPECT_EQ(image.PacketInfos().size(), 3U);
}

TEST(FrameEncodeMetadataWriterTest, DoesNotRewriteBitstreamWithoutCodecInfo) {
  uint8_t buffer[] = {1, 2, 3};
  auto image_buffer = EncodedImageBuffer::Create(buffer, sizeof(buffer));
  EncodedImage image;
  image.SetEncodedData(image_buffer);

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_metadata_writer(CreateTestEnvironment(),
                                                   &sink);
  encode_metadata_writer.UpdateBitstream(nullptr, &image);
  EXPECT_EQ(image.GetEncodedData(), image_buffer);
  EXPECT_EQ(image.size(), sizeof(buffer));
}

TEST(FrameEncodeMetadataWriterTest, DoesNotRewriteVp8Bitstream) {
  uint8_t buffer[] = {1, 2, 3};
  auto image_buffer = EncodedImageBuffer::Create(buffer, sizeof(buffer));
  EncodedImage image;
  image.SetEncodedData(image_buffer);
  CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = kVideoCodecVP8;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_metadata_writer(CreateTestEnvironment(),
                                                   &sink);
  encode_metadata_writer.UpdateBitstream(&codec_specific_info, &image);
  EXPECT_EQ(image.GetEncodedData(), image_buffer);
  EXPECT_EQ(image.size(), sizeof(buffer));
}

TEST(FrameEncodeMetadataWriterTest, RewritesH264BitstreamWithNonOptimalSps) {
  const uint8_t kOriginalSps[] = {0,    0,    0,    1,    H264::NaluType::kSps,
                                  0x00, 0x00, 0x03, 0x03, 0xF4,
                                  0x05, 0x03, 0xC7, 0xC0};
  const uint8_t kRewrittenSps[] = {0,    0,    0,    1,    H264::NaluType::kSps,
                                   0x00, 0x00, 0x03, 0x03, 0xF4,
                                   0x05, 0x03, 0xC7, 0xE0, 0x1B,
                                   0x41, 0x10, 0x8D, 0x00};

  EncodedImage image;
  image.SetEncodedData(
      EncodedImageBuffer::Create(kOriginalSps, sizeof(kOriginalSps)));
  image.set_frame_type(VideoFrameType::kVideoFrameKey);

  CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = kVideoCodecH264;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_metadata_writer(CreateTestEnvironment(),
                                                   &sink);
  encode_metadata_writer.UpdateBitstream(&codec_specific_info, &image);

  EXPECT_THAT(std::vector<uint8_t>(image.data(), image.data() + image.size()),
              testing::ElementsAreArray(kRewrittenSps));
}

TEST(FrameEncodeMetadataWriterTest, Av1SvcSpatialLayersCalculation) {
  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);

  VideoCodec codec_settings;
  codec_settings.codecType = kVideoCodecAV1;
  codec_settings.numberOfSimulcastStreams = 1;
  codec_settings.SetScalabilityMode(ScalabilityMode::kL2T2);
  encode_timer.OnEncoderInit(codec_settings);

  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 100000);
  bitrate_allocation.SetBitrate(1, 0, 200000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  const int64_t kTimestampMs = 1000;
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(kTimestampMs)
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);

  for (int i = 0; i < 2; ++i) {
    EncodedImage image;
    image.SetEncodedData(EncodedImageBuffer::Create(1000));
    image.capture_time_ms_ = kTimestampMs;
    image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
    image.SetSpatialIndex(i);
    encode_timer.FillMetadataAndTimingInfo(i, &image);
    EXPECT_EQ(kTimestampMs, image.capture_time_ms_);
    EXPECT_TRUE(IsTimingFrame(image));
  }
}

TEST(FrameEncodeMetadataWriterTest, Av1SimulcastSpatialLayersCalculation) {
  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);

  VideoCodec codec_settings;
  codec_settings.codecType = kVideoCodecAV1;
  codec_settings.numberOfSimulcastStreams = 2;
  codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  encode_timer.OnEncoderInit(codec_settings);

  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 100000);
  bitrate_allocation.SetBitrate(1, 0, 200000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  const int64_t kTimestampMs = 1000;
  VideoFrame frame = VideoFrame::Builder()
                         .set_timestamp_ms(kTimestampMs)
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);

  for (int i = 0; i < 2; ++i) {
    EncodedImage image;
    image.SetEncodedData(EncodedImageBuffer::Create(1000));
    image.capture_time_ms_ = kTimestampMs;
    image.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs * 90));
    image.SetSimulcastIndex(i);
    encode_timer.FillMetadataAndTimingInfo(i, &image);
    EXPECT_EQ(kTimestampMs, image.capture_time_ms_);
    EXPECT_TRUE(IsTimingFrame(image));
  }
}
TEST(FrameEncodeMetadataWriterTest, NotifiesAboutExplicitlyDroppedFrames) {
  const int64_t kTimestampMs = 4772184;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame = VideoFrame::Builder()
                         .set_rtp_timestamp(kTimestampMs * 90)
                         .set_timestamp_ms(kTimestampMs)
                         .set_video_frame_buffer(kFrameBuffer)
                         .build();
  encode_timer.OnEncodeStarted(frame);
  encode_timer.OnFrameDropped(kTimestampMs * 90, 0, true);

  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray({AllOf(
          Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs * 90)),
          Field(&DroppedFrameInfo::spatial_id, Eq(0)),
          Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true)))}));
}

TEST(FrameEncodeMetadataWriterTest, NotifiesAboutImplicitDropsOnEncodedFrame) {
  const int64_t kTimestampMs1 = 100000;
  const int64_t kTimestampMs2 = 100010;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs1 * 90)
                          .set_timestamp_ms(kTimestampMs1)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame1);

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs2 * 90)
                          .set_timestamp_ms(kTimestampMs2)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame2);

  // Fill metadata for frame 2 without having been given any feedback signal for
  // frame 1 - implying it has been dropped.
  EncodedImage image2;
  image2.capture_time_ms_ = kTimestampMs2;
  image2.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs2 * 90));
  encode_timer.FillMetadataAndTimingInfo(0, &image2);

  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray({AllOf(
          Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
          Field(&DroppedFrameInfo::spatial_id, Eq(0)),
          Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true)))}));
}

TEST(FrameEncodeMetadataWriterTest, NotifiesAboutImplicitDropsOnExplicitSkip) {
  const int64_t kTimestampMs1 = 100000;
  const int64_t kTimestampMs2 = 100010;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs1 * 90)
                          .set_timestamp_ms(kTimestampMs1)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame1);

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs2 * 90)
                          .set_timestamp_ms(kTimestampMs2)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame2);

  // Drop signal for frame 2 without having been given a feedback signal for
  // frame 1 implying it has been dropped.
  encode_timer.OnFrameDropped(kTimestampMs2 * 90, 0, true);

  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray(
          {AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true)))}));
}

TEST(FrameEncodeMetadataWriterTest, DroppingAlreadyDroppedFrameDoesNothing) {
  const int64_t kTimestampMs1 = 100000;
  const int64_t kTimestampMs2 = 100010;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  encode_timer.OnEncoderInit(VideoCodec());
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs1 * 90)
                          .set_timestamp_ms(kTimestampMs1)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame1);

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs2 * 90)
                          .set_timestamp_ms(kTimestampMs2)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame2);

  // Drop frame 1.
  encode_timer.OnFrameDropped(kTimestampMs1 * 90, 0, true);

  // Dropping an old already-dropped frame does nothing.
  encode_timer.OnFrameDropped(kTimestampMs1 * 90, 0, true);

  // Encoding an old already-dropped frame shouldn't crash, but it won't drop
  // anything new.
  EncodedImage image1;
  image1.capture_time_ms_ = kTimestampMs1;
  image1.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs1 * 90));
  encode_timer.FillMetadataAndTimingInfo(0, &image1);

  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray({AllOf(
          Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
          Field(&DroppedFrameInfo::spatial_id, Eq(0)),
          Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true)))}));
}

TEST(FrameEncodeMetadataWriterTest, ExplicitDropsWithMultipleSpatialLayers) {
  const int64_t kTimestampMs1 = 100000;
  const int64_t kTimestampMs2 = 100010;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  VideoCodec codec_settings;
  codec_settings.codecType = kVideoCodecVP8;
  codec_settings.numberOfSimulcastStreams = 2;
  encode_timer.OnEncoderInit(codec_settings);
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  bitrate_allocation.SetBitrate(1, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs1 * 90)
                          .set_timestamp_ms(kTimestampMs1)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame1);

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs2 * 90)
                          .set_timestamp_ms(kTimestampMs2)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame2);

  // Drop frame 1 on spatial layer 0.
  encode_timer.OnFrameDropped(kTimestampMs1 * 90, /*spatial_id=*/0,
                              /*is_end_of_temporal_unit=*/false);
  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray({AllOf(
          Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
          Field(&DroppedFrameInfo::spatial_id, Eq(0)),
          Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false)))}));

  // Drop frame 2 on spatial layer 1.
  encode_timer.OnFrameDropped(kTimestampMs2 * 90, /*spatial_id=*/1,
                              /*is_end_of_temporal_unit=*/true);
  // This implicitly drops frame 1 on layer 1, THEN explicitly drops frame 2 on
  // layer 1.
  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray(
          {AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(1)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(1)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true)))}));
}

TEST(FrameEncodeMetadataWriterTest,
     EndOfTemporalUnitOnDroppedFrameDropsOtherLayers) {
  const int64_t kTimestampMs1 = 100000;
  const int64_t kTimestampMs2 = 100010;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  VideoCodec codec_settings;
  codec_settings.codecType = kVideoCodecVP8;
  codec_settings.numberOfSimulcastStreams = 2;
  encode_timer.OnEncoderInit(codec_settings);
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  bitrate_allocation.SetBitrate(1, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs1 * 90)
                          .set_timestamp_ms(kTimestampMs1)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame1);

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs2 * 90)
                          .set_timestamp_ms(kTimestampMs2)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame2);

  encode_timer.OnFrameDropped(kTimestampMs2 * 90, 0, true);

  // Only the explicitly dropped frame (layer 0) should get
  // is_end_of_temporal_unit=true. The implicitly dropped frame on layer 1
  // should be pending on layer 0 at evaluation time and evaluate to false.
  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray(
          {AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(1)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(1)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(true)))}));
}

TEST(FrameEncodeMetadataWriterTest, DropsDifferentTimingAcrossLayers) {
  const int64_t kTimestampMs1 = 100000;
  const int64_t kTimestampMs2 = 100010;
  const int64_t kTimestampMs3 = 100020;

  FakeEncodedImageCallback sink;
  FrameEncodeMetadataWriter encode_timer(CreateTestEnvironment(), &sink);
  VideoCodec codec_settings;
  codec_settings.codecType = kVideoCodecVP8;
  codec_settings.numberOfSimulcastStreams = 2;
  encode_timer.OnEncoderInit(codec_settings);
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  bitrate_allocation.SetBitrate(1, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs1 * 90)
                          .set_timestamp_ms(kTimestampMs1)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame1);

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs2 * 90)
                          .set_timestamp_ms(kTimestampMs2)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame2);

  VideoFrame frame3 = VideoFrame::Builder()
                          .set_rtp_timestamp(kTimestampMs3 * 90)
                          .set_timestamp_ms(kTimestampMs3)
                          .set_video_frame_buffer(kFrameBuffer)
                          .build();
  encode_timer.OnEncodeStarted(frame3);

  // Fill metadata for frame 3 on spatial layer 0 without having given any
  // feedback signal for frame 1 and 2 - implying they have been dropped for
  // spatial layer 0.
  EncodedImage image3_s0;
  image3_s0.capture_time_ms_ = kTimestampMs3;
  image3_s0.SetRtpTimestamp(static_cast<uint32_t>(kTimestampMs3 * 90));
  encode_timer.FillMetadataAndTimingInfo(0, &image3_s0);

  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray(
          {AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false)))}));

  // Now explicitly drop frame 1 on spatial layer 1.
  encode_timer.OnFrameDropped(kTimestampMs1 * 90, /*spatial_id=*/1,
                              /*is_end_of_temporal_unit=*/false);

  EXPECT_THAT(
      sink.GetDroppedFrames(),
      ElementsAreArray(
          {AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs2 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(0)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false))),
           AllOf(
               Field(&DroppedFrameInfo::rtp_timestamp, Eq(kTimestampMs1 * 90)),
               Field(&DroppedFrameInfo::spatial_id, Eq(1)),
               Field(&DroppedFrameInfo::is_end_of_temporal_unit, Eq(false)))}));
}
}  // namespace test
}  // namespace webrtc
