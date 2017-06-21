/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"

#include <string.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_initializer.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "webrtc/modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "webrtc/system_wrappers/include/cpu_info.h"

namespace webrtc {
namespace test {

namespace {

// TODO(brandtr): Update this to use the real frame rate.
const int k90khzTimestampFrameDiff = 3000;  // Assuming 30 fps.

// Use the frame number as the basis for timestamp to identify frames. Let the
// first timestamp be non-zero, to not make the IvfFileWriter believe that we
// want to use capture timestamps in the IVF files.
uint32_t FrameNumberToTimestamp(int frame_number) {
  RTC_DCHECK_GE(frame_number, 0);
  return (frame_number + 1) * k90khzTimestampFrameDiff;
}

int TimestampToFrameNumber(uint32_t timestamp) {
  RTC_DCHECK_GT(timestamp, 0);
  RTC_DCHECK_EQ(timestamp % k90khzTimestampFrameDiff, 0);
  return (timestamp / k90khzTimestampFrameDiff) - 1;
}

std::unique_ptr<VideoBitrateAllocator> CreateBitrateAllocator(
    const TestConfig& config) {
  std::unique_ptr<TemporalLayersFactory> tl_factory;
  if (config.codec_settings->codecType == VideoCodecType::kVideoCodecVP8) {
    tl_factory.reset(new TemporalLayersFactory());
    config.codec_settings->VP8()->tl_factory = tl_factory.get();
  }
  return std::unique_ptr<VideoBitrateAllocator>(
      VideoCodecInitializer::CreateBitrateAllocator(*config.codec_settings,
                                                    std::move(tl_factory)));
}

void PrintCodecSettings(const VideoCodec* config) {
  printf("    Start bitrate    : %d kbps\n", config->startBitrate);
  printf("    Width            : %d\n", config->width);
  printf("    Height           : %d\n", config->height);
  printf("    Codec type       : %s\n",
         CodecTypeToPayloadName(config->codecType).value_or("Unknown"));
  if (config->codecType == kVideoCodecVP8) {
    printf("    Denoising        : %d\n", config->VP8().denoisingOn);
    printf("    Error concealment: %d\n", config->VP8().errorConcealmentOn);
    printf("    Frame dropping   : %d\n", config->VP8().frameDroppingOn);
    printf("    Resilience       : %d\n", config->VP8().resilience);
  } else if (config->codecType == kVideoCodecVP9) {
    printf("    Denoising        : %d\n", config->VP9().denoisingOn);
    printf("    Frame dropping   : %d\n", config->VP9().frameDroppingOn);
    printf("    Resilience       : %d\n", config->VP9().resilienceOn);
  }
}

int GetElapsedTimeMicroseconds(int64_t start_ns, int64_t stop_ns) {
  int64_t diff_us = (stop_ns - start_ns) / rtc::kNumNanosecsPerMicrosec;
  RTC_DCHECK_GE(diff_us, std::numeric_limits<int>::min());
  RTC_DCHECK_LE(diff_us, std::numeric_limits<int>::max());
  return static_cast<int>(diff_us);
}

}  // namespace

const char* ExcludeFrameTypesToStr(ExcludeFrameTypes e) {
  switch (e) {
    case kExcludeOnlyFirstKeyFrame:
      return "ExcludeOnlyFirstKeyFrame";
    case kExcludeAllKeyFrames:
      return "ExcludeAllKeyFrames";
    default:
      RTC_NOTREACHED();
      return "Unknown";
  }
}

TestConfig::TestConfig()
    : name(""),
      description(""),
      test_number(0),
      input_filename(""),
      output_filename(""),
      output_dir("out"),
      networking_config(),
      exclude_frame_types(kExcludeOnlyFirstKeyFrame),
      frame_length_in_bytes(0),
      use_single_core(false),
      keyframe_interval(0),
      codec_settings(nullptr),
      verbose(true) {}

TestConfig::~TestConfig() {}

VideoProcessorImpl::VideoProcessorImpl(webrtc::VideoEncoder* encoder,
                                       webrtc::VideoDecoder* decoder,
                                       FrameReader* analysis_frame_reader,
                                       FrameWriter* analysis_frame_writer,
                                       PacketManipulator* packet_manipulator,
                                       const TestConfig& config,
                                       Stats* stats,
                                       FrameWriter* source_frame_writer,
                                       IvfFileWriter* encoded_frame_writer,
                                       FrameWriter* decoded_frame_writer)
    : encoder_(encoder),
      decoder_(decoder),
      bitrate_allocator_(CreateBitrateAllocator(config)),
      encode_callback_(new VideoProcessorEncodeCompleteCallback(this)),
      decode_callback_(new VideoProcessorDecodeCompleteCallback(this)),
      packet_manipulator_(packet_manipulator),
      config_(config),
      analysis_frame_reader_(analysis_frame_reader),
      analysis_frame_writer_(analysis_frame_writer),
      num_frames_(analysis_frame_reader->NumberOfFrames()),
      source_frame_writer_(source_frame_writer),
      encoded_frame_writer_(encoded_frame_writer),
      decoded_frame_writer_(decoded_frame_writer),
      bit_rate_factor_(config.codec_settings->maxFramerate * 0.001 * 8),
      initialized_(false),
      last_encoded_frame_num_(-1),
      last_decoded_frame_num_(-1),
      first_key_frame_has_been_excluded_(false),
      last_decoded_frame_buffer_(0, analysis_frame_reader->FrameLength()),
      stats_(stats),
      num_dropped_frames_(0),
      num_spatial_resizes_(0) {
  RTC_DCHECK(encoder);
  RTC_DCHECK(decoder);
  RTC_DCHECK(packet_manipulator);
  RTC_DCHECK(analysis_frame_reader);
  RTC_DCHECK(analysis_frame_writer);
  RTC_DCHECK(stats);

  frame_infos_.reserve(num_frames_);
}

bool VideoProcessorImpl::Init() {
  RTC_DCHECK(!initialized_)
      << "This VideoProcessor has already been initialized.";

  // Setup required callbacks for the encoder/decoder.
  RTC_CHECK_EQ(encoder_->RegisterEncodeCompleteCallback(encode_callback_.get()),
               WEBRTC_VIDEO_CODEC_OK)
      << "Failed to register encode complete callback";
  RTC_CHECK_EQ(decoder_->RegisterDecodeCompleteCallback(decode_callback_.get()),
               WEBRTC_VIDEO_CODEC_OK)
      << "Failed to register decode complete callback";

  // Initialize the encoder and decoder.
  uint32_t num_cores =
      config_.use_single_core ? 1 : CpuInfo::DetectNumberOfCores();
  RTC_CHECK_EQ(
      encoder_->InitEncode(config_.codec_settings, num_cores,
                           config_.networking_config.max_payload_size_in_bytes),
      WEBRTC_VIDEO_CODEC_OK)
      << "Failed to initialize VideoEncoder";

  RTC_CHECK_EQ(decoder_->InitDecode(config_.codec_settings, num_cores),
               WEBRTC_VIDEO_CODEC_OK)
      << "Failed to initialize VideoDecoder";

  if (config_.verbose) {
    printf("Video Processor:\n");
    printf("  #CPU cores used  : %d\n", num_cores);
    printf("  Total # of frames: %d\n", num_frames_);
    printf("  Codec settings:\n");
    printf("    Encoder implementation name: %s\n",
           encoder_->ImplementationName());
    printf("    Decoder implementation name: %s\n",
           decoder_->ImplementationName());
    if (strcmp(encoder_->ImplementationName(),
               decoder_->ImplementationName()) == 0) {
      printf("    Codec implementation name: %s_%s\n",
             CodecTypeToPayloadName(config_.codec_settings->codecType)
                 .value_or("Unknown"),
             encoder_->ImplementationName());
    }
    PrintCodecSettings(config_.codec_settings);
  }

  initialized_ = true;

  return true;
}

VideoProcessorImpl::~VideoProcessorImpl() {
  encoder_->RegisterEncodeCompleteCallback(nullptr);
  decoder_->RegisterDecodeCompleteCallback(nullptr);
}

void VideoProcessorImpl::SetRates(int bit_rate, int frame_rate) {
  int set_rates_result = encoder_->SetRateAllocation(
      bitrate_allocator_->GetAllocation(bit_rate * 1000, frame_rate),
      frame_rate);
  RTC_DCHECK_GE(set_rates_result, 0)
      << "Failed to update encoder with new rate " << bit_rate;
  num_dropped_frames_ = 0;
  num_spatial_resizes_ = 0;
}

size_t VideoProcessorImpl::EncodedFrameSize(int frame_number) {
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  return frame_infos_[frame_number].encoded_frame_size;
}

FrameType VideoProcessorImpl::EncodedFrameType(int frame_number) {
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  return frame_infos_[frame_number].encoded_frame_type;
}

int VideoProcessorImpl::GetQpFromEncoder(int frame_number) {
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  return frame_infos_[frame_number].qp_encoder;
}

int VideoProcessorImpl::GetQpFromBitstream(int frame_number) {
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  return frame_infos_[frame_number].qp_bitstream;
}

int VideoProcessorImpl::NumberDroppedFrames() {
  return num_dropped_frames_;
}

int VideoProcessorImpl::NumberSpatialResizes() {
  return num_spatial_resizes_;
}

bool VideoProcessorImpl::ProcessFrame(int frame_number) {
  RTC_DCHECK_GE(frame_number, 0);
  RTC_DCHECK_LE(frame_number, frame_infos_.size())
      << "Must process frames without gaps.";
  RTC_DCHECK(initialized_) << "Attempting to use uninitialized VideoProcessor";

  rtc::scoped_refptr<I420BufferInterface> buffer(
      analysis_frame_reader_->ReadFrame());

  if (!buffer) {
    // Last frame has been reached.
    return false;
  }

  if (source_frame_writer_) {
    size_t length =
        CalcBufferSize(VideoType::kI420, buffer->width(), buffer->height());
    rtc::Buffer extracted_buffer(length);
    int extracted_length =
        ExtractBuffer(buffer, length, extracted_buffer.data());
    RTC_DCHECK_EQ(extracted_length, source_frame_writer_->FrameLength());
    RTC_CHECK(source_frame_writer_->WriteFrame(extracted_buffer.data()));
  }

  uint32_t timestamp = FrameNumberToTimestamp(frame_number);
  VideoFrame source_frame(buffer, timestamp, 0, webrtc::kVideoRotation_0);

  // Store frame information during the different stages of encode and decode.
  frame_infos_.emplace_back();
  FrameInfo* frame_info = &frame_infos_.back();
  frame_info->timestamp = timestamp;

  // Decide if we are going to force a keyframe.
  std::vector<FrameType> frame_types(1, kVideoFrameDelta);
  if (config_.keyframe_interval > 0 &&
      frame_number % config_.keyframe_interval == 0) {
    frame_types[0] = kVideoFrameKey;
  }

  // Create frame statistics object used for aggregation at end of test run.
  FrameStatistic* frame_stat = &stats_->NewFrame(frame_number);

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  frame_info->encode_start_ns = rtc::TimeNanos();
  frame_stat->encode_return_code =
      encoder_->Encode(source_frame, nullptr, &frame_types);

  if (frame_stat->encode_return_code != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to encode frame %d, return code: %d\n",
            frame_number, frame_stat->encode_return_code);
  }

  return true;
}

void VideoProcessorImpl::FrameEncoded(
    webrtc::VideoCodecType codec,
    const EncodedImage& encoded_image,
    const webrtc::RTPFragmentationHeader* fragmentation) {
  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  int64_t encode_stop_ns = rtc::TimeNanos();

  if (encoded_frame_writer_) {
    RTC_CHECK(encoded_frame_writer_->WriteFrame(encoded_image, codec));
  }

  // Timestamp is proportional to frame number, so this gives us number of
  // dropped frames.
  int frame_number = TimestampToFrameNumber(encoded_image._timeStamp);
  bool last_frame_missing = false;
  if (frame_number > 0) {
    RTC_DCHECK_GE(last_encoded_frame_num_, 0);
    int num_dropped_from_last_encode =
        frame_number - last_encoded_frame_num_ - 1;
    RTC_DCHECK_GE(num_dropped_from_last_encode, 0);
    num_dropped_frames_ += num_dropped_from_last_encode;
    if (num_dropped_from_last_encode > 0) {
      // For dropped frames, we write out the last decoded frame to avoid
      // getting out of sync for the computation of PSNR and SSIM.
      for (int i = 0; i < num_dropped_from_last_encode; i++) {
        RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                      analysis_frame_writer_->FrameLength());
        RTC_CHECK(analysis_frame_writer_->WriteFrame(
            last_decoded_frame_buffer_.data()));
        if (decoded_frame_writer_) {
          RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                        decoded_frame_writer_->FrameLength());
          RTC_CHECK(decoded_frame_writer_->WriteFrame(
              last_decoded_frame_buffer_.data()));
        }
      }
    }

    last_frame_missing =
        (frame_infos_[last_encoded_frame_num_].manipulated_length == 0);
  }
  // Ensure strict monotonicity.
  RTC_CHECK_GT(frame_number, last_encoded_frame_num_);
  last_encoded_frame_num_ = frame_number;

  // Frame is not dropped, so update frame information and statistics.
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  FrameInfo* frame_info = &frame_infos_[frame_number];
  frame_info->encoded_frame_size = encoded_image._length;
  frame_info->encoded_frame_type = encoded_image._frameType;
  frame_info->qp_encoder = encoded_image.qp_;
  if (codec == kVideoCodecVP8) {
    vp8::GetQp(encoded_image._buffer, encoded_image._length,
      &frame_info->qp_bitstream);
  } else if (codec == kVideoCodecVP9) {
    vp9::GetQp(encoded_image._buffer, encoded_image._length,
      &frame_info->qp_bitstream);
  }
  FrameStatistic* frame_stat = &stats_->stats_[frame_number];
  frame_stat->encode_time_in_us =
      GetElapsedTimeMicroseconds(frame_info->encode_start_ns, encode_stop_ns);
  frame_stat->encoding_successful = true;
  frame_stat->encoded_frame_length_in_bytes = encoded_image._length;
  frame_stat->frame_number = frame_number;
  frame_stat->frame_type = encoded_image._frameType;
  frame_stat->qp = encoded_image.qp_;
  frame_stat->bit_rate_in_kbps = encoded_image._length * bit_rate_factor_;
  frame_stat->total_packets =
      encoded_image._length / config_.networking_config.packet_size_in_bytes +
      1;

  // Simulate packet loss.
  bool exclude_this_frame = false;
  if (encoded_image._frameType == kVideoFrameKey) {
    // Only keyframes can be excluded.
    switch (config_.exclude_frame_types) {
      case kExcludeOnlyFirstKeyFrame:
        if (!first_key_frame_has_been_excluded_) {
          first_key_frame_has_been_excluded_ = true;
          exclude_this_frame = true;
        }
        break;
      case kExcludeAllKeyFrames:
        exclude_this_frame = true;
        break;
      default:
        RTC_NOTREACHED();
    }
  }

  // Make a raw copy of the |encoded_image| buffer.
  size_t copied_buffer_size = encoded_image._length +
                              EncodedImage::GetBufferPaddingBytes(codec);
  std::unique_ptr<uint8_t[]> copied_buffer(new uint8_t[copied_buffer_size]);
  memcpy(copied_buffer.get(), encoded_image._buffer, encoded_image._length);
  // The image to feed to the decoder.
  EncodedImage copied_image;
  memcpy(&copied_image, &encoded_image, sizeof(copied_image));
  copied_image._size = copied_buffer_size;
  copied_image._buffer = copied_buffer.get();

  if (!exclude_this_frame) {
    frame_stat->packets_dropped =
        packet_manipulator_->ManipulatePackets(&copied_image);
  }
  frame_info->manipulated_length = copied_image._length;

  // Keep track of if frames are lost due to packet loss so we can tell
  // this to the encoder (this is handled by the RTP logic in the full stack).
  // TODO(kjellander): Pass fragmentation header to the decoder when
  // CL 172001 has been submitted and PacketManipulator supports this.

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  frame_info->decode_start_ns = rtc::TimeNanos();
  frame_stat->decode_return_code =
      decoder_->Decode(copied_image, last_frame_missing, nullptr);

  if (frame_stat->decode_return_code != WEBRTC_VIDEO_CODEC_OK) {
    // Write the last successful frame the output file to avoid getting it out
    // of sync with the source file for SSIM and PSNR comparisons.
    RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                  analysis_frame_writer_->FrameLength());
    RTC_CHECK(
        analysis_frame_writer_->WriteFrame(last_decoded_frame_buffer_.data()));
    if (decoded_frame_writer_) {
      RTC_DCHECK_EQ(last_decoded_frame_buffer_.size(),
                    decoded_frame_writer_->FrameLength());
      RTC_CHECK(
          decoded_frame_writer_->WriteFrame(last_decoded_frame_buffer_.data()));
    }
  }
}

void VideoProcessorImpl::FrameDecoded(const VideoFrame& image) {
  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  int64_t decode_stop_ns = rtc::TimeNanos();

  // Update frame information and statistics.
  int frame_number = TimestampToFrameNumber(image.timestamp());
  RTC_DCHECK_LT(frame_number, frame_infos_.size());
  FrameInfo* frame_info = &frame_infos_[frame_number];
  frame_info->decoded_width = image.width();
  frame_info->decoded_height = image.height();
  FrameStatistic* frame_stat = &stats_->stats_[frame_number];
  frame_stat->decode_time_in_us =
      GetElapsedTimeMicroseconds(frame_info->decode_start_ns, decode_stop_ns);
  frame_stat->decoding_successful = true;

  // Check if the codecs have resized the frame since previously decoded frame.
  if (frame_number > 0) {
    RTC_DCHECK_GE(last_decoded_frame_num_, 0);
    const FrameInfo& last_decoded_frame_info =
        frame_infos_[last_decoded_frame_num_];
    if (static_cast<int>(image.width()) !=
            last_decoded_frame_info.decoded_width ||
        static_cast<int>(image.height()) !=
            last_decoded_frame_info.decoded_height) {
      ++num_spatial_resizes_;
    }
  }
  // Ensure strict monotonicity.
  RTC_CHECK_GT(frame_number, last_decoded_frame_num_);
  last_decoded_frame_num_ = frame_number;

  // Check if codec size is different from the original size, and if so,
  // scale back to original size. This is needed for the PSNR and SSIM
  // calculations.
  size_t extracted_length;
  rtc::Buffer extracted_buffer;
  if (image.width() != config_.codec_settings->width ||
      image.height() != config_.codec_settings->height) {
    rtc::scoped_refptr<I420Buffer> scaled_buffer(I420Buffer::Create(
        config_.codec_settings->width, config_.codec_settings->height));
    // Should be the same aspect ratio, no cropping needed.
    scaled_buffer->ScaleFrom(*image.video_frame_buffer()->ToI420());

    size_t length = CalcBufferSize(VideoType::kI420, scaled_buffer->width(),
                                   scaled_buffer->height());
    extracted_buffer.SetSize(length);
    extracted_length =
        ExtractBuffer(scaled_buffer, length, extracted_buffer.data());
  } else {
    // No resize.
    size_t length =
        CalcBufferSize(VideoType::kI420, image.width(), image.height());
    extracted_buffer.SetSize(length);
    extracted_length = ExtractBuffer(image.video_frame_buffer()->ToI420(),
                                     length, extracted_buffer.data());
  }

  RTC_DCHECK_EQ(extracted_length, analysis_frame_writer_->FrameLength());
  RTC_CHECK(analysis_frame_writer_->WriteFrame(extracted_buffer.data()));
  if (decoded_frame_writer_) {
    RTC_DCHECK_EQ(extracted_length, decoded_frame_writer_->FrameLength());
    RTC_CHECK(decoded_frame_writer_->WriteFrame(extracted_buffer.data()));
  }

  last_decoded_frame_buffer_ = std::move(extracted_buffer);
}

}  // namespace test
}  // namespace webrtc
