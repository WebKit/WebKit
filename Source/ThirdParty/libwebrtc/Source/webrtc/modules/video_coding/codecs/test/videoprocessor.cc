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

#include <assert.h>
#include <string.h>

#include <limits>
#include <memory>
#include <vector>

#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/cpu_info.h"

namespace webrtc {
namespace test {

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
      codec_settings(NULL),
      verbose(true) {}

TestConfig::~TestConfig() {}

VideoProcessorImpl::VideoProcessorImpl(webrtc::VideoEncoder* encoder,
                                       webrtc::VideoDecoder* decoder,
                                       FrameReader* frame_reader,
                                       FrameWriter* frame_writer,
                                       PacketManipulator* packet_manipulator,
                                       const TestConfig& config,
                                       Stats* stats)
    : encoder_(encoder),
      decoder_(decoder),
      frame_reader_(frame_reader),
      frame_writer_(frame_writer),
      packet_manipulator_(packet_manipulator),
      config_(config),
      stats_(stats),
      encode_callback_(NULL),
      decode_callback_(NULL),
      first_key_frame_has_been_excluded_(false),
      last_frame_missing_(false),
      initialized_(false),
      encoded_frame_size_(0),
      encoded_frame_type_(kVideoFrameKey),
      prev_time_stamp_(0),
      num_dropped_frames_(0),
      num_spatial_resizes_(0),
      last_encoder_frame_width_(0),
      last_encoder_frame_height_(0) {
  assert(encoder);
  assert(decoder);
  assert(frame_reader);
  assert(frame_writer);
  assert(packet_manipulator);
  assert(stats);
}

bool VideoProcessorImpl::Init() {
  // Calculate a factor used for bit rate calculations:
  bit_rate_factor_ = config_.codec_settings->maxFramerate * 0.001 * 8;  // bits

  // Initialize data structures used by the encoder/decoder APIs
  size_t frame_length_in_bytes = frame_reader_->FrameLength();
  last_successful_frame_buffer_ = new uint8_t[frame_length_in_bytes];
  // Set fixed properties common for all frames.
  // To keep track of spatial resize actions by encoder.
  last_encoder_frame_width_ = config_.codec_settings->width;
  last_encoder_frame_height_ = config_.codec_settings->height;

  // Setup required callbacks for the encoder/decoder:
  encode_callback_ = new VideoProcessorEncodeCompleteCallback(this);
  decode_callback_ = new VideoProcessorDecodeCompleteCallback(this);
  int32_t register_result =
      encoder_->RegisterEncodeCompleteCallback(encode_callback_);
  if (register_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr,
            "Failed to register encode complete callback, return code: "
            "%d\n",
            register_result);
    return false;
  }
  register_result = decoder_->RegisterDecodeCompleteCallback(decode_callback_);
  if (register_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr,
            "Failed to register decode complete callback, return code: "
            "%d\n",
            register_result);
    return false;
  }
  // Init the encoder and decoder
  uint32_t nbr_of_cores = 1;
  if (!config_.use_single_core) {
    nbr_of_cores = CpuInfo::DetectNumberOfCores();
  }
  int32_t init_result =
      encoder_->InitEncode(config_.codec_settings, nbr_of_cores,
                           config_.networking_config.max_payload_size_in_bytes);
  if (init_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to initialize VideoEncoder, return code: %d\n",
            init_result);
    return false;
  }
  init_result = decoder_->InitDecode(config_.codec_settings, nbr_of_cores);
  if (init_result != WEBRTC_VIDEO_CODEC_OK) {
    fprintf(stderr, "Failed to initialize VideoDecoder, return code: %d\n",
            init_result);
    return false;
  }

  if (config_.verbose) {
    printf("Video Processor:\n");
    printf("  #CPU cores used  : %d\n", nbr_of_cores);
    printf("  Total # of frames: %d\n", frame_reader_->NumberOfFrames());
    printf("  Codec settings:\n");
    printf("    Start bitrate  : %d kbps\n",
           config_.codec_settings->startBitrate);
    printf("    Width          : %d\n", config_.codec_settings->width);
    printf("    Height         : %d\n", config_.codec_settings->height);
  }
  initialized_ = true;
  return true;
}

VideoProcessorImpl::~VideoProcessorImpl() {
  delete[] last_successful_frame_buffer_;
  encoder_->RegisterEncodeCompleteCallback(NULL);
  delete encode_callback_;
  decoder_->RegisterDecodeCompleteCallback(NULL);
  delete decode_callback_;
}

void VideoProcessorImpl::SetRates(int bit_rate, int frame_rate) {
  int set_rates_result = encoder_->SetRates(bit_rate, frame_rate);
  assert(set_rates_result >= 0);
  if (set_rates_result < 0) {
    fprintf(stderr,
            "Failed to update encoder with new rate %d, "
            "return code: %d\n",
            bit_rate, set_rates_result);
  }
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
  assert(frame_number >= 0);
  if (!initialized_) {
    fprintf(stderr, "Attempting to use uninitialized VideoProcessor!\n");
    return false;
  }
  // |prev_time_stamp_| is used for getting number of dropped frames.
  if (frame_number == 0) {
    prev_time_stamp_ = -1;
  }
  rtc::scoped_refptr<VideoFrameBuffer> buffer(frame_reader_->ReadFrame());
  if (buffer) {
    // Use the frame number as "timestamp" to identify frames
    VideoFrame source_frame(buffer, frame_number, 0, webrtc::kVideoRotation_0);

    // Ensure we have a new statistics data object we can fill:
    FrameStatistic& stat = stats_->NewFrame(frame_number);

    encode_start_ns_ = rtc::TimeNanos();

    // Decide if we're going to force a keyframe:
    std::vector<FrameType> frame_types(1, kVideoFrameDelta);
    if (config_.keyframe_interval > 0 &&
        frame_number % config_.keyframe_interval == 0) {
      frame_types[0] = kVideoFrameKey;
    }

    // For dropped frames, we regard them as zero size encoded frames.
    encoded_frame_size_ = 0;
    encoded_frame_type_ = kVideoFrameDelta;

    int32_t encode_result = encoder_->Encode(source_frame, NULL, &frame_types);

    if (encode_result != WEBRTC_VIDEO_CODEC_OK) {
      fprintf(stderr, "Failed to encode frame %d, return code: %d\n",
              frame_number, encode_result);
    }
    stat.encode_return_code = encode_result;
    return true;
  } else {
    return false;  // we've reached the last frame
  }
}

void VideoProcessorImpl::FrameEncoded(
    webrtc::VideoCodecType codec,
    const EncodedImage& encoded_image,
    const webrtc::RTPFragmentationHeader* fragmentation) {
  // Timestamp is frame number, so this gives us #dropped frames.
  int num_dropped_from_prev_encode =
      encoded_image._timeStamp - prev_time_stamp_ - 1;
  num_dropped_frames_ += num_dropped_from_prev_encode;
  prev_time_stamp_ = encoded_image._timeStamp;
  if (num_dropped_from_prev_encode > 0) {
    // For dropped frames, we write out the last decoded frame to avoid getting
    // out of sync for the computation of PSNR and SSIM.
    for (int i = 0; i < num_dropped_from_prev_encode; i++) {
      frame_writer_->WriteFrame(last_successful_frame_buffer_);
    }
  }
  // Frame is not dropped, so update the encoded frame size
  // (encoder callback is only called for non-zero length frames).
  encoded_frame_size_ = encoded_image._length;

  encoded_frame_type_ = encoded_image._frameType;

  int64_t encode_stop_ns = rtc::TimeNanos();
  int frame_number = encoded_image._timeStamp;
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.encode_time_in_us =
      GetElapsedTimeMicroseconds(encode_start_ns_, encode_stop_ns);
  stat.encoding_successful = true;
  stat.encoded_frame_length_in_bytes = encoded_image._length;
  stat.frame_number = encoded_image._timeStamp;
  stat.frame_type = encoded_image._frameType;
  stat.bit_rate_in_kbps = encoded_image._length * bit_rate_factor_;
  stat.total_packets =
      encoded_image._length / config_.networking_config.packet_size_in_bytes +
      1;

  // Perform packet loss if criteria is fullfilled:
  bool exclude_this_frame = false;
  // Only keyframes can be excluded
  if (encoded_image._frameType == kVideoFrameKey) {
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
        assert(false);
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
  // this to the encoder (this is handled by the RTP logic in the full stack)
  decode_start_ns_ = rtc::TimeNanos();
  // TODO(kjellander): Pass fragmentation header to the decoder when
  // CL 172001 has been submitted and PacketManipulator supports this.
  int32_t decode_result =
      decoder_->Decode(copied_image, last_frame_missing_, NULL);
  stat.decode_return_code = decode_result;
  if (decode_result != WEBRTC_VIDEO_CODEC_OK) {
    // Write the last successful frame the output file to avoid getting it out
    // of sync with the source file for SSIM and PSNR comparisons:
    frame_writer_->WriteFrame(last_successful_frame_buffer_);
  }
  // save status for losses so we can inform the decoder for the next frame:
  last_frame_missing_ = copied_image._length == 0;
}

void VideoProcessorImpl::FrameDecoded(const VideoFrame& image) {
  int64_t decode_stop_ns = rtc::TimeNanos();
  int frame_number = image.timestamp();
  // Report stats
  FrameStatistic& stat = stats_->stats_[frame_number];
  stat.decode_time_in_us =
      GetElapsedTimeMicroseconds(decode_start_ns_, decode_stop_ns);
  stat.decoding_successful = true;

  // Check for resize action (either down or up):
  if (static_cast<int>(image.width()) != last_encoder_frame_width_ ||
      static_cast<int>(image.height()) != last_encoder_frame_height_) {
    ++num_spatial_resizes_;
    last_encoder_frame_width_ = image.width();
    last_encoder_frame_height_ = image.height();
  }
  // Check if codec size is different from native/original size, and if so,
  // upsample back to original size: needed for PSNR and SSIM computations.
  if (image.width() != config_.codec_settings->width ||
      image.height() != config_.codec_settings->height) {
    rtc::scoped_refptr<I420Buffer> up_image(
        I420Buffer::Create(config_.codec_settings->width,
                           config_.codec_settings->height));

    // Should be the same aspect ratio, no cropping needed.
    up_image->ScaleFrom(*image.video_frame_buffer());

    // TODO(mikhal): Extracting the buffer for now - need to update test.
    size_t length =
        CalcBufferSize(kI420, up_image->width(), up_image->height());
    std::unique_ptr<uint8_t[]> image_buffer(new uint8_t[length]);
    int extracted_length = ExtractBuffer(up_image, length, image_buffer.get());
    assert(extracted_length > 0);
    // Update our copy of the last successful frame:
    memcpy(last_successful_frame_buffer_, image_buffer.get(), extracted_length);
    bool write_success = frame_writer_->WriteFrame(image_buffer.get());
    assert(write_success);
    if (!write_success) {
      fprintf(stderr, "Failed to write frame %d to disk!", frame_number);
    }
  } else {  // No resize.
    // Update our copy of the last successful frame:
    // TODO(mikhal): Add as a member function, so won't be allocated per frame.
    size_t length = CalcBufferSize(kI420, image.width(), image.height());
    std::unique_ptr<uint8_t[]> image_buffer(new uint8_t[length]);
    int extracted_length = ExtractBuffer(image, length, image_buffer.get());
    assert(extracted_length > 0);
    memcpy(last_successful_frame_buffer_, image_buffer.get(), extracted_length);

    bool write_success = frame_writer_->WriteFrame(image_buffer.get());
    assert(write_success);
    if (!write_success) {
      fprintf(stderr, "Failed to write frame %d to disk!", frame_number);
    }
  }
}

int VideoProcessorImpl::GetElapsedTimeMicroseconds(int64_t start,
                                                   int64_t stop) {
  uint64_t encode_time = (stop - start) / rtc::kNumNanosecsPerMicrosec;
  assert(encode_time <
         static_cast<unsigned int>(std::numeric_limits<int>::max()));
  return static_cast<int>(encode_time);
}

const char* ExcludeFrameTypesToStr(ExcludeFrameTypes e) {
  switch (e) {
    case kExcludeOnlyFirstKeyFrame:
      return "ExcludeOnlyFirstKeyFrame";
    case kExcludeAllKeyFrames:
      return "ExcludeAllKeyFrames";
    default:
      assert(false);
      return "Unknown";
  }
}

const char* VideoCodecTypeToStr(webrtc::VideoCodecType e) {
  switch (e) {
    case kVideoCodecVP8:
      return "VP8";
    case kVideoCodecI420:
      return "I420";
    case kVideoCodecRED:
      return "RED";
    case kVideoCodecULPFEC:
      return "ULPFEC";
    case kVideoCodecUnknown:
      return "Unknown";
    default:
      assert(false);
      return "Unknown";
  }
}

// Callbacks
EncodedImageCallback::Result
VideoProcessorImpl::VideoProcessorEncodeCompleteCallback::OnEncodedImage(
    const EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const webrtc::RTPFragmentationHeader* fragmentation) {
  // Forward to parent class.
  RTC_CHECK(codec_specific_info);
  video_processor_->FrameEncoded(codec_specific_info->codecType,
                                 encoded_image,
                                 fragmentation);
  return Result(Result::OK, 0);
}
int32_t VideoProcessorImpl::VideoProcessorDecodeCompleteCallback::Decoded(
    VideoFrame& image) {
  // Forward to parent class.
  video_processor_->FrameDecoded(image);
  return 0;
}

}  // namespace test
}  // namespace webrtc
