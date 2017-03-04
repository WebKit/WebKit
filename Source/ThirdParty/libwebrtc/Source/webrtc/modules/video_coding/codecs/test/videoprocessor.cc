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
#include "webrtc/modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "webrtc/modules/video_coding/utility/simulcast_rate_allocator.h"
#include "webrtc/system_wrappers/include/cpu_info.h"

namespace webrtc {
namespace test {

namespace {
const int k90khzTimestampFrameDiff = 3000;  // Assuming 30 fps.

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
      source_frame_writer_(source_frame_writer),
      encoded_frame_writer_(encoded_frame_writer),
      decoded_frame_writer_(decoded_frame_writer),
      first_key_frame_has_been_excluded_(false),
      last_frame_missing_(false),
      initialized_(false),
      encoded_frame_size_(0),
      encoded_frame_type_(kVideoFrameKey),
      prev_time_stamp_(0),
      last_encoder_frame_width_(0),
      last_encoder_frame_height_(0),
      stats_(stats),
      num_dropped_frames_(0),
      num_spatial_resizes_(0),
      bit_rate_factor_(0.0),
      encode_start_ns_(0),
      decode_start_ns_(0) {
  RTC_DCHECK(encoder);
  RTC_DCHECK(decoder);
  RTC_DCHECK(packet_manipulator);
  RTC_DCHECK(analysis_frame_reader);
  RTC_DCHECK(analysis_frame_writer);
  RTC_DCHECK(stats);
}

bool VideoProcessorImpl::Init() {
  // Calculate a factor used for bit rate calculations.
  bit_rate_factor_ = config_.codec_settings->maxFramerate * 0.001 * 8;  // bits

  // Initialize data structures used by the encoder/decoder APIs.
  size_t frame_length_in_bytes = analysis_frame_reader_->FrameLength();
  last_successful_frame_buffer_.reset(new uint8_t[frame_length_in_bytes]);

  // Set fixed properties common for all frames.
  // To keep track of spatial resize actions by encoder.
  last_encoder_frame_width_ = config_.codec_settings->width;
  last_encoder_frame_height_ = config_.codec_settings->height;

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
    printf("  Total # of frames: %d\n",
           analysis_frame_reader_->NumberOfFrames());
    printf("  Codec settings:\n");
    printf("    Start bitrate    : %d kbps\n",
           config_.codec_settings->startBitrate);
    printf("    Width            : %d\n", config_.codec_settings->width);
    printf("    Height           : %d\n", config_.codec_settings->height);
    printf("    Codec type       : %s\n",
           CodecTypeToPayloadName(config_.codec_settings->codecType)
               .value_or("Unknown"));
    printf("    Encoder implementation name: %s\n",
           encoder_->ImplementationName());
    printf("    Decoder implementation name: %s\n",
           decoder_->ImplementationName());
    if (config_.codec_settings->codecType == kVideoCodecVP8) {
      printf("    Denoising        : %d\n",
             config_.codec_settings->VP8()->denoisingOn);
      printf("    Error concealment: %d\n",
             config_.codec_settings->VP8()->errorConcealmentOn);
      printf("    Frame dropping   : %d\n",
             config_.codec_settings->VP8()->frameDroppingOn);
      printf("    Resilience       : %d\n",
             config_.codec_settings->VP8()->resilience);
    } else if (config_.codec_settings->codecType == kVideoCodecVP9) {
      printf("    Resilience       : %d\n",
             config_.codec_settings->VP9()->resilience);
    }
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
  RTC_CHECK_GE(set_rates_result, 0) << "Failed to update encoder with new rate "
                                    << bit_rate;
  num_dropped_frames_ = 0;
  num_spatial_resizes_ = 0;
}

size_t VideoProcessorImpl::EncodedFrameSize() {
  return encoded_frame_size_;
}

FrameType VideoProcessorImpl::EncodedFrameType() {
  return encoded_frame_type_;
}

int VideoProcessorImpl::NumberDroppedFrames() {
  return num_dropped_frames_;
}

int VideoProcessorImpl::NumberSpatialResizes() {
  return num_spatial_resizes_;
}

bool VideoProcessorImpl::ProcessFrame(int frame_number) {
  RTC_CHECK_GE(frame_number, 0);
  RTC_CHECK(initialized_) << "Attempting to use uninitialized VideoProcessor";

  rtc::scoped_refptr<VideoFrameBuffer> buffer(
      analysis_frame_reader_->ReadFrame());
  if (buffer) {
    if (source_frame_writer_) {
      // TODO(brandtr): Introduce temp buffer as data member, to avoid
      // allocating for every frame.
      size_t length = CalcBufferSize(kI420, buffer->width(), buffer->height());
      std::unique_ptr<uint8_t[]> extracted_buffer(new uint8_t[length]);
      int extracted_length =
          ExtractBuffer(buffer, length, extracted_buffer.get());
      RTC_CHECK_EQ(extracted_length, source_frame_writer_->FrameLength());
      source_frame_writer_->WriteFrame(extracted_buffer.get());
    }

    // Use the frame number as basis for timestamp to identify frames. Let the
    // first timestamp be non-zero, to not make the IvfFileWriter believe that
    // we want to use capture timestamps in the IVF files.
    VideoFrame source_frame(buffer,
                            (frame_number + 1) * k90khzTimestampFrameDiff, 0,
                            webrtc::kVideoRotation_0);

    // Ensure we have a new statistics data object we can fill.
    FrameStatistic& stat = stats_->NewFrame(frame_number);

    // Decide if we are going to force a keyframe.
    std::vector<FrameType> frame_types(1, kVideoFrameDelta);
    if (config_.keyframe_interval > 0 &&
        frame_number % config_.keyframe_interval == 0) {
      frame_types[0] = kVideoFrameKey;
    }

    // For dropped frames, we regard them as zero size encoded frames.
    encoded_frame_size_ = 0;
    encoded_frame_type_ = kVideoFrameDelta;

    // For the highest measurement accuracy of the encode time, the start/stop
    // time recordings should wrap the Encode call as tightly as possible.
    encode_start_ns_ = rtc::TimeNanos();
    int32_t encode_result =
        encoder_->Encode(source_frame, nullptr, &frame_types);

    if (encode_result != WEBRTC_VIDEO_CODEC_OK) {
      fprintf(stderr, "Failed to encode frame %d, return code: %d\n",
              frame_number, encode_result);
    }
    stat.encode_return_code = encode_result;

    return true;
  } else {
    // Last frame has been reached.
    return false;
  }
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
  int num_dropped_from_prev_encode =
      (encoded_image._timeStamp - prev_time_stamp_) / k90khzTimestampFrameDiff -
      1;
  num_dropped_frames_ += num_dropped_from_prev_encode;
  prev_time_stamp_ = encoded_image._timeStamp;
  if (num_dropped_from_prev_encode > 0) {
    // For dropped frames, we write out the last decoded frame to avoid getting
    // out of sync for the computation of PSNR and SSIM.
    for (int i = 0; i < num_dropped_from_prev_encode; i++) {
      RTC_CHECK(analysis_frame_writer_->WriteFrame(
          last_successful_frame_buffer_.get()));
      if (decoded_frame_writer_) {
        RTC_CHECK(decoded_frame_writer_->WriteFrame(
            last_successful_frame_buffer_.get()));
      }
    }
  }

  // Frame is not dropped, so update the encoded frame size
  // (encoder callback is only called for non-zero length frames).
  encoded_frame_size_ = encoded_image._length;
  encoded_frame_type_ = encoded_image._frameType;
  int frame_number = encoded_image._timeStamp / k90khzTimestampFrameDiff - 1;
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.encode_time_in_us =
      GetElapsedTimeMicroseconds(encode_start_ns_, encode_stop_ns);
  stat.encoding_successful = true;
  stat.encoded_frame_length_in_bytes = encoded_image._length;
  stat.frame_number = frame_number;
  stat.frame_type = encoded_image._frameType;
  stat.qp = encoded_image.qp_;
  stat.bit_rate_in_kbps = encoded_image._length * bit_rate_factor_;
  stat.total_packets =
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
    stat.packets_dropped =
        packet_manipulator_->ManipulatePackets(&copied_image);
  }

  // Keep track of if frames are lost due to packet loss so we can tell
  // this to the encoder (this is handled by the RTP logic in the full stack).
  // TODO(kjellander): Pass fragmentation header to the decoder when
  // CL 172001 has been submitted and PacketManipulator supports this.

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  decode_start_ns_ = rtc::TimeNanos();
  int32_t decode_result =
      decoder_->Decode(copied_image, last_frame_missing_, nullptr);
  stat.decode_return_code = decode_result;

  if (decode_result != WEBRTC_VIDEO_CODEC_OK) {
    // Write the last successful frame the output file to avoid getting it out
    // of sync with the source file for SSIM and PSNR comparisons.
    RTC_CHECK(analysis_frame_writer_->WriteFrame(
        last_successful_frame_buffer_.get()));
    if (decoded_frame_writer_) {
      RTC_CHECK(decoded_frame_writer_->WriteFrame(
          last_successful_frame_buffer_.get()));
    }
  }

  // Save status for losses so we can inform the decoder for the next frame.
  last_frame_missing_ = copied_image._length == 0;
}

void VideoProcessorImpl::FrameDecoded(const VideoFrame& image) {
  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  int64_t decode_stop_ns = rtc::TimeNanos();

  // Report stats.
  int frame_number = image.timestamp() / k90khzTimestampFrameDiff - 1;
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.decode_time_in_us =
      GetElapsedTimeMicroseconds(decode_start_ns_, decode_stop_ns);
  stat.decoding_successful = true;

  // Check for resize action (either down or up).
  if (static_cast<int>(image.width()) != last_encoder_frame_width_ ||
      static_cast<int>(image.height()) != last_encoder_frame_height_) {
    ++num_spatial_resizes_;
    last_encoder_frame_width_ = image.width();
    last_encoder_frame_height_ = image.height();
  }
  // Check if codec size is different from native/original size, and if so,
  // upsample back to original size. This is needed for PSNR and SSIM
  // calculations.
  if (image.width() != config_.codec_settings->width ||
      image.height() != config_.codec_settings->height) {
    rtc::scoped_refptr<I420Buffer> up_image(
        I420Buffer::Create(config_.codec_settings->width,
                           config_.codec_settings->height));

    // Should be the same aspect ratio, no cropping needed.
    if (image.video_frame_buffer()->native_handle()) {
      up_image->ScaleFrom(*image.video_frame_buffer()->NativeToI420Buffer());
    } else {
      up_image->ScaleFrom(*image.video_frame_buffer());
    }

    // TODO(mikhal): Extracting the buffer for now - need to update test.
    size_t length =
        CalcBufferSize(kI420, up_image->width(), up_image->height());
    std::unique_ptr<uint8_t[]> image_buffer(new uint8_t[length]);
    int extracted_length = ExtractBuffer(up_image, length, image_buffer.get());
    RTC_CHECK_GT(extracted_length, 0);
    // Update our copy of the last successful frame.
    memcpy(last_successful_frame_buffer_.get(), image_buffer.get(),
           extracted_length);

    RTC_CHECK(analysis_frame_writer_->WriteFrame(image_buffer.get()));
    if (decoded_frame_writer_) {
      RTC_CHECK(decoded_frame_writer_->WriteFrame(image_buffer.get()));
    }
  } else {  // No resize.
    // Update our copy of the last successful frame.
    // TODO(mikhal): Add as a member function, so won't be allocated per frame.
    size_t length = CalcBufferSize(kI420, image.width(), image.height());
    std::unique_ptr<uint8_t[]> image_buffer(new uint8_t[length]);
    int extracted_length;
    if (image.video_frame_buffer()->native_handle()) {
      extracted_length =
          ExtractBuffer(image.video_frame_buffer()->NativeToI420Buffer(),
                        length, image_buffer.get());
    } else {
      extracted_length =
          ExtractBuffer(image.video_frame_buffer(), length, image_buffer.get());
    }
    RTC_CHECK_GT(extracted_length, 0);
    memcpy(last_successful_frame_buffer_.get(), image_buffer.get(),
           extracted_length);

    RTC_CHECK(analysis_frame_writer_->WriteFrame(image_buffer.get()));
    if (decoded_frame_writer_) {
      RTC_CHECK(decoded_frame_writer_->WriteFrame(image_buffer.get()));
    }
  }
}

int VideoProcessorImpl::GetElapsedTimeMicroseconds(int64_t start,
                                                   int64_t stop) {
  int64_t encode_time = (stop - start) / rtc::kNumNanosecsPerMicrosec;
  RTC_DCHECK_GE(encode_time, std::numeric_limits<int>::min());
  RTC_DCHECK_LE(encode_time, std::numeric_limits<int>::max());
  return static_cast<int>(encode_time);
}

}  // namespace test
}  // namespace webrtc
