/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "api/video/i420_buffer.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_common.h"
#include "modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

const int kRtpClockRateHz = 90000;

std::unique_ptr<VideoBitrateAllocator> CreateBitrateAllocator(
    TestConfig* config) {
  std::unique_ptr<TemporalLayersFactory> tl_factory;
  if (config->codec_settings.codecType == VideoCodecType::kVideoCodecVP8) {
    tl_factory.reset(new TemporalLayersFactory());
    config->codec_settings.VP8()->tl_factory = tl_factory.get();
  }
  return std::unique_ptr<VideoBitrateAllocator>(
      VideoCodecInitializer::CreateBitrateAllocator(config->codec_settings,
                                                    std::move(tl_factory)));
}

rtc::Optional<size_t> GetMaxNaluLength(const EncodedImage& encoded_frame,
                                       const TestConfig& config) {
  if (config.codec_settings.codecType != kVideoCodecH264)
    return rtc::nullopt;

  std::vector<webrtc::H264::NaluIndex> nalu_indices =
      webrtc::H264::FindNaluIndices(encoded_frame._buffer,
                                    encoded_frame._length);

  RTC_CHECK(!nalu_indices.empty());

  size_t max_length = 0;
  for (const webrtc::H264::NaluIndex& index : nalu_indices)
    max_length = std::max(max_length, index.payload_size);

  return max_length;
}

int GetElapsedTimeMicroseconds(int64_t start_ns, int64_t stop_ns) {
  int64_t diff_us = (stop_ns - start_ns) / rtc::kNumNanosecsPerMicrosec;
  RTC_DCHECK_GE(diff_us, std::numeric_limits<int>::min());
  RTC_DCHECK_LE(diff_us, std::numeric_limits<int>::max());
  return static_cast<int>(diff_us);
}

void ExtractBufferWithSize(const VideoFrame& image,
                           int width,
                           int height,
                           rtc::Buffer* buffer) {
  if (image.width() != width || image.height() != height) {
    EXPECT_DOUBLE_EQ(static_cast<double>(width) / height,
                     static_cast<double>(image.width()) / image.height());
    // Same aspect ratio, no cropping needed.
    rtc::scoped_refptr<I420Buffer> scaled(I420Buffer::Create(width, height));
    scaled->ScaleFrom(*image.video_frame_buffer()->ToI420());

    size_t length =
        CalcBufferSize(VideoType::kI420, scaled->width(), scaled->height());
    buffer->SetSize(length);
    RTC_CHECK_NE(ExtractBuffer(scaled, length, buffer->data()), -1);
    return;
  }

  // No resize.
  size_t length =
      CalcBufferSize(VideoType::kI420, image.width(), image.height());
  buffer->SetSize(length);
  RTC_CHECK_NE(ExtractBuffer(image, length, buffer->data()), -1);
}

}  // namespace

VideoProcessor::VideoProcessor(webrtc::VideoEncoder* encoder,
                               webrtc::VideoDecoder* decoder,
                               FrameReader* analysis_frame_reader,
                               PacketManipulator* packet_manipulator,
                               const TestConfig& config,
                               Stats* stats,
                               IvfFileWriter* encoded_frame_writer,
                               FrameWriter* decoded_frame_writer)
    : config_(config),
      encoder_(encoder),
      decoder_(decoder),
      bitrate_allocator_(CreateBitrateAllocator(&config_)),
      encode_callback_(this),
      decode_callback_(this),
      packet_manipulator_(packet_manipulator),
      analysis_frame_reader_(analysis_frame_reader),
      encoded_frame_writer_(encoded_frame_writer),
      decoded_frame_writer_(decoded_frame_writer),
      last_inputed_frame_num_(-1),
      last_encoded_frame_num_(-1),
      last_decoded_frame_num_(-1),
      first_key_frame_has_been_excluded_(false),
      last_decoded_frame_buffer_(analysis_frame_reader->FrameLength()),
      stats_(stats),
      rate_update_index_(-1) {
  RTC_DCHECK(encoder);
  RTC_DCHECK(decoder);
  RTC_DCHECK(packet_manipulator);
  RTC_DCHECK(analysis_frame_reader);
  RTC_DCHECK(stats);

  // Setup required callbacks for the encoder and decoder.
  RTC_CHECK_EQ(encoder_->RegisterEncodeCompleteCallback(&encode_callback_),
               WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK_EQ(decoder_->RegisterDecodeCompleteCallback(&decode_callback_),
               WEBRTC_VIDEO_CODEC_OK);

  // Initialize the encoder and decoder.
  RTC_CHECK_EQ(
      encoder_->InitEncode(&config_.codec_settings, config_.NumberOfCores(),
                           config_.networking_config.max_payload_size_in_bytes),
      WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK_EQ(
      decoder_->InitDecode(&config_.codec_settings, config_.NumberOfCores()),
      WEBRTC_VIDEO_CODEC_OK);
}

VideoProcessor::~VideoProcessor() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  RTC_CHECK_EQ(encoder_->Release(), WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK_EQ(decoder_->Release(), WEBRTC_VIDEO_CODEC_OK);

  encoder_->RegisterEncodeCompleteCallback(nullptr);
  decoder_->RegisterDecodeCompleteCallback(nullptr);
}

void VideoProcessor::ProcessFrame() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  const int frame_number = ++last_inputed_frame_num_;

  // Get frame from file.
  rtc::scoped_refptr<I420BufferInterface> buffer(
      analysis_frame_reader_->ReadFrame());
  RTC_CHECK(buffer) << "Tried to read too many frames from the file.";
  // Use the frame number as the basis for timestamp to identify frames. Let the
  // first timestamp be non-zero, to not make the IvfFileWriter believe that we
  // want to use capture timestamps in the IVF files.
  const uint32_t rtp_timestamp = (frame_number + 1) * kRtpClockRateHz /
                                 config_.codec_settings.maxFramerate;
  const int64_t render_time_ms = (frame_number + 1) * rtc::kNumMillisecsPerSec /
                                 config_.codec_settings.maxFramerate;
  rtp_timestamp_to_frame_num_[rtp_timestamp] = frame_number;
  input_frames_[frame_number] = rtc::MakeUnique<VideoFrame>(
      buffer, rtp_timestamp, render_time_ms, webrtc::kVideoRotation_0);

  std::vector<FrameType> frame_types = config_.FrameTypeForFrame(frame_number);

  // Create frame statistics object used for aggregation at end of test run.
  FrameStatistic* frame_stat = stats_->AddFrame();

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  frame_stat->encode_start_ns = rtc::TimeNanos();
  frame_stat->encode_return_code =
      encoder_->Encode(*input_frames_[frame_number], nullptr, &frame_types);
}

void VideoProcessor::SetRates(int bitrate_kbps, int framerate_fps) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  config_.codec_settings.maxFramerate = framerate_fps;
  int set_rates_result = encoder_->SetRateAllocation(
      bitrate_allocator_->GetAllocation(bitrate_kbps * 1000, framerate_fps),
      framerate_fps);
  RTC_DCHECK_GE(set_rates_result, 0)
      << "Failed to update encoder with new rate " << bitrate_kbps << ".";
  ++rate_update_index_;
  num_dropped_frames_.push_back(0);
  num_spatial_resizes_.push_back(0);
}

std::vector<int> VideoProcessor::NumberDroppedFramesPerRateUpdate() const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  return num_dropped_frames_;
}

std::vector<int> VideoProcessor::NumberSpatialResizesPerRateUpdate() const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  return num_spatial_resizes_;
}

void VideoProcessor::FrameEncoded(webrtc::VideoCodecType codec,
                                  const EncodedImage& encoded_image) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  int64_t encode_stop_ns = rtc::TimeNanos();

  if (config_.encoded_frame_checker) {
    config_.encoded_frame_checker->CheckEncodedFrame(codec, encoded_image);
  }

  const int frame_number =
      rtp_timestamp_to_frame_num_[encoded_image._timeStamp];

  // Ensure strict monotonicity.
  RTC_CHECK_GT(frame_number, last_encoded_frame_num_);

  // Check for dropped frames.
  bool last_frame_missing = false;
  if (frame_number > 0) {
    int num_dropped_from_last_encode =
        frame_number - last_encoded_frame_num_ - 1;
    RTC_DCHECK_GE(num_dropped_from_last_encode, 0);
    RTC_CHECK_GE(rate_update_index_, 0);
    num_dropped_frames_[rate_update_index_] += num_dropped_from_last_encode;
    const FrameStatistic* last_encoded_frame_stat =
        stats_->GetFrame(last_encoded_frame_num_);
    last_frame_missing = (last_encoded_frame_stat->manipulated_length == 0);
  }
  last_encoded_frame_num_ = frame_number;

  // Update frame statistics.
  FrameStatistic* frame_stat = stats_->GetFrame(frame_number);
  frame_stat->encode_time_us =
      GetElapsedTimeMicroseconds(frame_stat->encode_start_ns, encode_stop_ns);
  frame_stat->encoding_successful = true;
  frame_stat->encoded_frame_size_bytes = encoded_image._length;
  frame_stat->frame_type = encoded_image._frameType;
  frame_stat->qp = encoded_image.qp_;
  frame_stat->bitrate_kbps = static_cast<int>(
      encoded_image._length * config_.codec_settings.maxFramerate * 8 / 1000);
  frame_stat->total_packets =
      encoded_image._length / config_.networking_config.packet_size_in_bytes +
      1;
  frame_stat->max_nalu_length = GetMaxNaluLength(encoded_image, config_);

  // Make a raw copy of |encoded_image| to feed to the decoder.
  size_t copied_buffer_size = encoded_image._length +
                              EncodedImage::GetBufferPaddingBytes(codec);
  std::unique_ptr<uint8_t[]> copied_buffer(new uint8_t[copied_buffer_size]);
  memcpy(copied_buffer.get(), encoded_image._buffer, encoded_image._length);
  EncodedImage copied_image = encoded_image;
  copied_image._size = copied_buffer_size;
  copied_image._buffer = copied_buffer.get();

  // Simulate packet loss.
  if (!ExcludeFrame(copied_image)) {
    frame_stat->packets_dropped =
        packet_manipulator_->ManipulatePackets(&copied_image);
  }
  frame_stat->manipulated_length = copied_image._length;

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  frame_stat->decode_start_ns = rtc::TimeNanos();
  frame_stat->decode_return_code =
      decoder_->Decode(copied_image, last_frame_missing, nullptr);

  if (encoded_frame_writer_) {
    RTC_CHECK(encoded_frame_writer_->WriteFrame(encoded_image, codec));
  }
}

void VideoProcessor::FrameDecoded(const VideoFrame& decoded_frame) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  int64_t decode_stop_ns = rtc::TimeNanos();

  // Update frame statistics.
  const int frame_number =
      rtp_timestamp_to_frame_num_[decoded_frame.timestamp()];
  FrameStatistic* frame_stat = stats_->GetFrame(frame_number);
  frame_stat->decoded_width = decoded_frame.width();
  frame_stat->decoded_height = decoded_frame.height();
  frame_stat->decode_time_us =
      GetElapsedTimeMicroseconds(frame_stat->decode_start_ns, decode_stop_ns);
  frame_stat->decoding_successful = true;

  // Ensure strict monotonicity.
  RTC_CHECK_GT(frame_number, last_decoded_frame_num_);

  // Check if the codecs have resized the frame since previously decoded frame.
  if (frame_number > 0) {
    if (decoded_frame_writer_ && last_decoded_frame_num_ >= 0) {
      // For dropped/lost frames, write out the last decoded frame to make it
      // look like a freeze at playback.
      const int num_dropped_frames = frame_number - last_decoded_frame_num_;
      for (int i = 0; i < num_dropped_frames; i++) {
        WriteDecodedFrameToFile(&last_decoded_frame_buffer_);
      }
    }
    // TODO(ssilkin): move to FrameEncoded when webm:1474 is implemented.
    const FrameStatistic* last_decoded_frame_stat =
        stats_->GetFrame(last_decoded_frame_num_);
    if (decoded_frame.width() != last_decoded_frame_stat->decoded_width ||
        decoded_frame.height() != last_decoded_frame_stat->decoded_height) {
      RTC_CHECK_GE(rate_update_index_, 0);
      ++num_spatial_resizes_[rate_update_index_];
    }
  }
  last_decoded_frame_num_ = frame_number;

  // Skip quality metrics calculation to not affect CPU usage.
  if (!config_.measure_cpu) {
    frame_stat->psnr =
        I420PSNR(input_frames_[frame_number].get(), &decoded_frame);
    frame_stat->ssim =
        I420SSIM(input_frames_[frame_number].get(), &decoded_frame);
  }

  // Delay erasing of input frames by one frame. The current frame might
  // still be needed for other simulcast stream or spatial layer.
  const int frame_number_to_erase = frame_number - 1;
  if (frame_number_to_erase >= 0) {
    auto input_frame_erase_to =
        input_frames_.lower_bound(frame_number_to_erase);
    input_frames_.erase(input_frames_.begin(), input_frame_erase_to);
  }

  if (decoded_frame_writer_) {
    ExtractBufferWithSize(decoded_frame, config_.codec_settings.width,
                          config_.codec_settings.height,
                          &last_decoded_frame_buffer_);
    WriteDecodedFrameToFile(&last_decoded_frame_buffer_);
  }
}

void VideoProcessor::WriteDecodedFrameToFile(rtc::Buffer* buffer) {
  RTC_DCHECK_EQ(buffer->size(), decoded_frame_writer_->FrameLength());
  RTC_CHECK(decoded_frame_writer_->WriteFrame(buffer->data()));
}

bool VideoProcessor::ExcludeFrame(const EncodedImage& encoded_image) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  if (encoded_image._frameType != kVideoFrameKey) {
    return false;
  }
  bool exclude_frame = false;
  switch (config_.exclude_frame_types) {
    case kExcludeOnlyFirstKeyFrame:
      if (!first_key_frame_has_been_excluded_) {
        first_key_frame_has_been_excluded_ = true;
        exclude_frame = true;
      }
      break;
    case kExcludeAllKeyFrames:
      exclude_frame = true;
      break;
    default:
      RTC_NOTREACHED();
  }
  return exclude_frame;
}

}  // namespace test
}  // namespace webrtc
