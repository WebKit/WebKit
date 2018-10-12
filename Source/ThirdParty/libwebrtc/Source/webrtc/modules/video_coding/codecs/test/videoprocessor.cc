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
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_codec_initializer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/timeutils.h"
#include "test/gtest.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

using FrameStatistics = VideoCodecTestStats::FrameStatistics;

namespace {
const int kMsToRtpTimestamp = kVideoPayloadTypeFrequency / 1000;
const int kMaxBufferedInputFrames = 20;

size_t GetMaxNaluSizeBytes(const EncodedImage& encoded_frame,
                           const VideoCodecTestFixture::Config& config) {
  if (config.codec_settings.codecType != kVideoCodecH264)
    return 0;

  std::vector<webrtc::H264::NaluIndex> nalu_indices =
      webrtc::H264::FindNaluIndices(encoded_frame._buffer,
                                    encoded_frame._length);

  RTC_CHECK(!nalu_indices.empty());

  size_t max_size = 0;
  for (const webrtc::H264::NaluIndex& index : nalu_indices)
    max_size = std::max(max_size, index.payload_size);

  return max_size;
}

size_t GetTemporalLayerIndex(const CodecSpecificInfo& codec_specific) {
  size_t temporal_idx = 0;
  if (codec_specific.codecType == kVideoCodecVP8) {
    temporal_idx = codec_specific.codecSpecific.VP8.temporalIdx;
  } else if (codec_specific.codecType == kVideoCodecVP9) {
    temporal_idx = codec_specific.codecSpecific.VP9.temporal_idx;
  }
  if (temporal_idx == kNoTemporalIdx) {
    temporal_idx = 0;
  }
  return temporal_idx;
}

int GetElapsedTimeMicroseconds(int64_t start_ns, int64_t stop_ns) {
  int64_t diff_us = (stop_ns - start_ns) / rtc::kNumNanosecsPerMicrosec;
  RTC_DCHECK_GE(diff_us, std::numeric_limits<int>::min());
  RTC_DCHECK_LE(diff_us, std::numeric_limits<int>::max());
  return static_cast<int>(diff_us);
}

void ExtractI420BufferWithSize(const VideoFrame& image,
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

void CalculateFrameQuality(const I420BufferInterface& ref_buffer,
                           const I420BufferInterface& dec_buffer,
                           FrameStatistics* frame_stat) {
  if (ref_buffer.width() != dec_buffer.width() ||
      ref_buffer.height() != dec_buffer.height()) {
    RTC_CHECK_GE(ref_buffer.width(), dec_buffer.width());
    RTC_CHECK_GE(ref_buffer.height(), dec_buffer.height());
    // Downscale reference frame.
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(dec_buffer.width(), dec_buffer.height());
    I420Scale(ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
              ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
              ref_buffer.width(), ref_buffer.height(),
              scaled_buffer->MutableDataY(), scaled_buffer->StrideY(),
              scaled_buffer->MutableDataU(), scaled_buffer->StrideU(),
              scaled_buffer->MutableDataV(), scaled_buffer->StrideV(),
              scaled_buffer->width(), scaled_buffer->height(),
              libyuv::kFilterBox);

    CalculateFrameQuality(*scaled_buffer, dec_buffer, frame_stat);
  } else {
    const uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
        dec_buffer.DataY(), dec_buffer.StrideY(), ref_buffer.DataY(),
        ref_buffer.StrideY(), dec_buffer.width(), dec_buffer.height());

    const uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
        dec_buffer.DataU(), dec_buffer.StrideU(), ref_buffer.DataU(),
        ref_buffer.StrideU(), dec_buffer.width() / 2, dec_buffer.height() / 2);

    const uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
        dec_buffer.DataV(), dec_buffer.StrideV(), ref_buffer.DataV(),
        ref_buffer.StrideV(), dec_buffer.width() / 2, dec_buffer.height() / 2);

    const size_t num_y_samples = dec_buffer.width() * dec_buffer.height();
    const size_t num_u_samples =
        dec_buffer.width() / 2 * dec_buffer.height() / 2;

    frame_stat->psnr_y = libyuv::SumSquareErrorToPsnr(sse_y, num_y_samples);
    frame_stat->psnr_u = libyuv::SumSquareErrorToPsnr(sse_u, num_u_samples);
    frame_stat->psnr_v = libyuv::SumSquareErrorToPsnr(sse_v, num_u_samples);
    frame_stat->psnr = libyuv::SumSquareErrorToPsnr(
        sse_y + sse_u + sse_v, num_y_samples + 2 * num_u_samples);
    frame_stat->ssim = I420SSIM(ref_buffer, dec_buffer);
  }
}

std::vector<FrameType> FrameTypeForFrame(
    const VideoCodecTestFixture::Config& config,
    size_t frame_idx) {
  if (config.keyframe_interval > 0 &&
      (frame_idx % config.keyframe_interval == 0)) {
    return {kVideoFrameKey};
  }
  return {kVideoFrameDelta};
}

}  // namespace

VideoProcessor::VideoProcessor(webrtc::VideoEncoder* encoder,
                               VideoDecoderList* decoders,
                               FrameReader* input_frame_reader,
                               const VideoCodecTestFixture::Config& config,
                               VideoCodecTestStats* stats,
                               IvfFileWriterList* encoded_frame_writers,
                               FrameWriterList* decoded_frame_writers)
    : config_(config),
      num_simulcast_or_spatial_layers_(
          std::max(config_.NumberOfSimulcastStreams(),
                   config_.NumberOfSpatialLayers())),
      stats_(stats),
      encoder_(encoder),
      decoders_(decoders),
      bitrate_allocator_(VideoCodecInitializer::CreateBitrateAllocator(
          config_.codec_settings)),
      framerate_fps_(0),
      encode_callback_(this),
      input_frame_reader_(input_frame_reader),
      merged_encoded_frames_(num_simulcast_or_spatial_layers_),
      encoded_frame_writers_(encoded_frame_writers),
      decoded_frame_writers_(decoded_frame_writers),
      last_inputed_frame_num_(0),
      last_inputed_timestamp_(0),
      first_encoded_frame_(num_simulcast_or_spatial_layers_, true),
      last_encoded_frame_num_(num_simulcast_or_spatial_layers_),
      first_decoded_frame_(num_simulcast_or_spatial_layers_, true),
      last_decoded_frame_num_(num_simulcast_or_spatial_layers_),
      decoded_frame_buffer_(num_simulcast_or_spatial_layers_),
      post_encode_time_ns_(0) {
  // Sanity checks.
  RTC_CHECK(rtc::TaskQueue::Current())
      << "VideoProcessor must be run on a task queue.";
  RTC_CHECK(encoder);
  RTC_CHECK(decoders);
  RTC_CHECK_EQ(decoders->size(), num_simulcast_or_spatial_layers_);
  RTC_CHECK(input_frame_reader);
  RTC_CHECK(stats);
  RTC_CHECK(!encoded_frame_writers ||
            encoded_frame_writers->size() == num_simulcast_or_spatial_layers_);
  RTC_CHECK(!decoded_frame_writers ||
            decoded_frame_writers->size() == num_simulcast_or_spatial_layers_);

  // Setup required callbacks for the encoder and decoder and initialize them.
  RTC_CHECK_EQ(encoder_->RegisterEncodeCompleteCallback(&encode_callback_),
               WEBRTC_VIDEO_CODEC_OK);

  // Initialize codecs so that they are ready to receive frames.
  RTC_CHECK_EQ(encoder_->InitEncode(&config_.codec_settings,
                                    static_cast<int>(config_.NumberOfCores()),
                                    config_.max_payload_size_bytes),
               WEBRTC_VIDEO_CODEC_OK);

  for (size_t i = 0; i < num_simulcast_or_spatial_layers_; ++i) {
    decode_callback_.push_back(
        absl::make_unique<VideoProcessorDecodeCompleteCallback>(this, i));
    RTC_CHECK_EQ(
        decoders_->at(i)->InitDecode(&config_.codec_settings,
                                     static_cast<int>(config_.NumberOfCores())),
        WEBRTC_VIDEO_CODEC_OK);
    RTC_CHECK_EQ(decoders_->at(i)->RegisterDecodeCompleteCallback(
                     decode_callback_.at(i).get()),
                 WEBRTC_VIDEO_CODEC_OK);
  }
}

VideoProcessor::~VideoProcessor() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // Explicitly reset codecs, in case they don't do that themselves when they
  // go out of scope.
  RTC_CHECK_EQ(encoder_->Release(), WEBRTC_VIDEO_CODEC_OK);
  encoder_->RegisterEncodeCompleteCallback(nullptr);
  for (auto& decoder : *decoders_) {
    RTC_CHECK_EQ(decoder->Release(), WEBRTC_VIDEO_CODEC_OK);
    decoder->RegisterDecodeCompleteCallback(nullptr);
  }

  // Sanity check.
  RTC_CHECK_LE(input_frames_.size(), kMaxBufferedInputFrames);

  // Deal with manual memory management of EncodedImage's.
  for (size_t i = 0; i < num_simulcast_or_spatial_layers_; ++i) {
    uint8_t* buffer = merged_encoded_frames_.at(i)._buffer;
    if (buffer) {
      delete[] buffer;
    }
  }
}

void VideoProcessor::ProcessFrame() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  const size_t frame_number = last_inputed_frame_num_++;

  // Get input frame and store for future quality calculation.
  rtc::scoped_refptr<I420BufferInterface> buffer =
      input_frame_reader_->ReadFrame();
  RTC_CHECK(buffer) << "Tried to read too many frames from the file.";
  const size_t timestamp =
      last_inputed_timestamp_ + kVideoPayloadTypeFrequency / framerate_fps_;
  VideoFrame input_frame(buffer, static_cast<uint32_t>(timestamp),
                         static_cast<int64_t>(timestamp / kMsToRtpTimestamp),
                         webrtc::kVideoRotation_0);
  // Store input frame as a reference for quality calculations.
  if (config_.decode && !config_.measure_cpu) {
    if (input_frames_.size() == kMaxBufferedInputFrames) {
      input_frames_.erase(input_frames_.begin());
    }
    input_frames_.emplace(frame_number, input_frame);
  }
  last_inputed_timestamp_ = timestamp;

  post_encode_time_ns_ = 0;

  // Create frame statistics object for all simulcast/spatial layers.
  for (size_t i = 0; i < num_simulcast_or_spatial_layers_; ++i) {
    FrameStatistics frame_stat(frame_number, timestamp, i);
    stats_->AddFrame(frame_stat);
  }

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  const int64_t encode_start_ns = rtc::TimeNanos();
  for (size_t i = 0; i < num_simulcast_or_spatial_layers_; ++i) {
    FrameStatistics* frame_stat = stats_->GetFrame(frame_number, i);
    frame_stat->encode_start_ns = encode_start_ns;
  }

  // Encode.
  const std::vector<FrameType> frame_types =
      FrameTypeForFrame(config_, frame_number);
  const int encode_return_code =
      encoder_->Encode(input_frame, nullptr, &frame_types);
  for (size_t i = 0; i < num_simulcast_or_spatial_layers_; ++i) {
    FrameStatistics* frame_stat = stats_->GetFrame(frame_number, i);
    frame_stat->encode_return_code = encode_return_code;
  }
}

void VideoProcessor::SetRates(size_t bitrate_kbps, size_t framerate_fps) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  framerate_fps_ = static_cast<uint32_t>(framerate_fps);
  bitrate_allocation_ = bitrate_allocator_->GetAllocation(
      static_cast<uint32_t>(bitrate_kbps * 1000), framerate_fps_);
  const int set_rates_result =
      encoder_->SetRateAllocation(bitrate_allocation_, framerate_fps_);
  RTC_DCHECK_GE(set_rates_result, 0)
      << "Failed to update encoder with new rate " << bitrate_kbps << ".";
}

int32_t VideoProcessor::VideoProcessorDecodeCompleteCallback::Decoded(
    VideoFrame& image) {
  // Post the callback to the right task queue, if needed.
  if (!task_queue_->IsCurrent()) {
    // There might be a limited amount of output buffers, make a copy to make
    // sure we don't block the decoder.
    VideoFrame copy(I420Buffer::Copy(*image.video_frame_buffer()->ToI420()),
                    image.rotation(), image.timestamp_us());
    copy.set_timestamp(image.timestamp());

    task_queue_->PostTask([this, copy]() {
      video_processor_->FrameDecoded(copy, simulcast_svc_idx_);
    });
    return 0;
  }
  video_processor_->FrameDecoded(image, simulcast_svc_idx_);
  return 0;
}

void VideoProcessor::FrameEncoded(
    const webrtc::EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo& codec_specific) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the encode time, the start/stop
  // time recordings should wrap the Encode call as tightly as possible.
  const int64_t encode_stop_ns = rtc::TimeNanos();

  const VideoCodecType codec_type = codec_specific.codecType;
  if (config_.encoded_frame_checker) {
    config_.encoded_frame_checker->CheckEncodedFrame(codec_type, encoded_image);
  }

  // Layer metadata.
  size_t spatial_idx = encoded_image.SpatialIndex().value_or(0);
  size_t temporal_idx = GetTemporalLayerIndex(codec_specific);

  FrameStatistics* frame_stat =
      stats_->GetFrameWithTimestamp(encoded_image.Timestamp(), spatial_idx);
  const size_t frame_number = frame_stat->frame_number;

  // Ensure that the encode order is monotonically increasing, within this
  // simulcast/spatial layer.
  RTC_CHECK(first_encoded_frame_[spatial_idx] ||
            last_encoded_frame_num_[spatial_idx] < frame_number);

  // Ensure SVC spatial layers are delivered in ascending order.
  if (!first_encoded_frame_[spatial_idx] &&
      config_.NumberOfSpatialLayers() > 1) {
    for (size_t i = 0; i < spatial_idx; ++i) {
      RTC_CHECK_LE(last_encoded_frame_num_[i], frame_number);
    }
    for (size_t i = spatial_idx + 1; i < num_simulcast_or_spatial_layers_;
         ++i) {
      RTC_CHECK_GT(frame_number, last_encoded_frame_num_[i]);
    }
  }
  first_encoded_frame_[spatial_idx] = false;
  last_encoded_frame_num_[spatial_idx] = frame_number;

  // Update frame statistics.
  frame_stat->encoding_successful = true;
  frame_stat->encode_time_us = GetElapsedTimeMicroseconds(
      frame_stat->encode_start_ns, encode_stop_ns - post_encode_time_ns_);
  frame_stat->target_bitrate_kbps =
      bitrate_allocation_.GetTemporalLayerSum(spatial_idx, temporal_idx) / 1000;
  frame_stat->length_bytes = encoded_image._length;
  frame_stat->frame_type = encoded_image._frameType;
  frame_stat->temporal_idx = temporal_idx;
  frame_stat->max_nalu_size_bytes = GetMaxNaluSizeBytes(encoded_image, config_);
  frame_stat->qp = encoded_image.qp_;

  const size_t num_spatial_layers = config_.NumberOfSpatialLayers();
  bool end_of_picture = false;
  if (codec_type == kVideoCodecVP9) {
    const CodecSpecificInfoVP9& vp9_info = codec_specific.codecSpecific.VP9;
    frame_stat->inter_layer_predicted = vp9_info.inter_layer_predicted;
    frame_stat->non_ref_for_inter_layer_pred =
        vp9_info.non_ref_for_inter_layer_pred;
    end_of_picture = vp9_info.end_of_picture;
  } else {
    frame_stat->inter_layer_predicted = false;
    frame_stat->non_ref_for_inter_layer_pred = true;
  }

  const webrtc::EncodedImage* encoded_image_for_decode = &encoded_image;
  if (config_.decode || encoded_frame_writers_) {
    if (num_spatial_layers > 1) {
      encoded_image_for_decode = BuildAndStoreSuperframe(
          encoded_image, codec_type, frame_number, spatial_idx,
          frame_stat->inter_layer_predicted);
    }
  }

  if (config_.decode) {
    DecodeFrame(*encoded_image_for_decode, spatial_idx);

    if (end_of_picture && num_spatial_layers > 1) {
      // If inter-layer prediction is enabled and upper layer was dropped then
      // base layer should be passed to upper layer decoder. Otherwise decoder
      // won't be able to decode next superframe.
      const EncodedImage* base_image = nullptr;
      const FrameStatistics* base_stat = nullptr;
      for (size_t i = 0; i < num_spatial_layers; ++i) {
        const bool layer_dropped = (first_decoded_frame_[i] ||
                                    last_decoded_frame_num_[i] < frame_number);

        // Ensure current layer was decoded.
        RTC_CHECK(layer_dropped == false || i != spatial_idx);

        if (!layer_dropped) {
          base_image = &merged_encoded_frames_[i];
          base_stat =
              stats_->GetFrameWithTimestamp(encoded_image.Timestamp(), i);
        } else if (base_image && !base_stat->non_ref_for_inter_layer_pred) {
          DecodeFrame(*base_image, i);
        }
      }
    }
  } else {
    frame_stat->decode_return_code = WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }

  if (encoded_frame_writers_) {
    RTC_CHECK(encoded_frame_writers_->at(spatial_idx)
                  ->WriteFrame(*encoded_image_for_decode,
                               config_.codec_settings.codecType));
  }

  if (!config_.encode_in_real_time) {
    // To get pure encode time for next layers, measure time spent in encode
    // callback and subtract it from encode time of next layers.
    post_encode_time_ns_ += rtc::TimeNanos() - encode_stop_ns;
  }
}

void VideoProcessor::FrameDecoded(const VideoFrame& decoded_frame,
                                  size_t spatial_idx) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  // For the highest measurement accuracy of the decode time, the start/stop
  // time recordings should wrap the Decode call as tightly as possible.
  const int64_t decode_stop_ns = rtc::TimeNanos();

  FrameStatistics* frame_stat =
      stats_->GetFrameWithTimestamp(decoded_frame.timestamp(), spatial_idx);
  const size_t frame_number = frame_stat->frame_number;

  if (decoded_frame_writers_ && !first_decoded_frame_[spatial_idx]) {
    // Fill drops with last decoded frame to make them look like freeze at
    // playback and to keep decoded layers in sync.
    for (size_t i = last_decoded_frame_num_[spatial_idx] + 1; i < frame_number;
         ++i) {
      RTC_CHECK(decoded_frame_writers_->at(spatial_idx)
                    ->WriteFrame(decoded_frame_buffer_[spatial_idx].data()));
    }
  }

  // Ensure that the decode order is monotonically increasing, within this
  // simulcast/spatial layer.
  RTC_CHECK(first_decoded_frame_[spatial_idx] ||
            last_decoded_frame_num_[spatial_idx] < frame_number);
  first_decoded_frame_[spatial_idx] = false;
  last_decoded_frame_num_[spatial_idx] = frame_number;

  // Update frame statistics.
  frame_stat->decoding_successful = true;
  frame_stat->decode_time_us =
      GetElapsedTimeMicroseconds(frame_stat->decode_start_ns, decode_stop_ns);
  frame_stat->decoded_width = decoded_frame.width();
  frame_stat->decoded_height = decoded_frame.height();

  // Skip quality metrics calculation to not affect CPU usage.
  if (!config_.measure_cpu) {
    const auto reference_frame = input_frames_.find(frame_number);
    RTC_CHECK(reference_frame != input_frames_.cend())
        << "The codecs are either buffering too much, dropping too much, or "
           "being too slow relative the input frame rate.";
    CalculateFrameQuality(
        *reference_frame->second.video_frame_buffer()->ToI420(),
        *decoded_frame.video_frame_buffer()->ToI420(), frame_stat);

    // Erase all buffered input frames that we have moved past for all
    // simulcast/spatial layers. Never buffer more than
    // |kMaxBufferedInputFrames| frames, to protect against long runs of
    // consecutive frame drops for a particular layer.
    const auto min_last_decoded_frame_num = std::min_element(
        last_decoded_frame_num_.cbegin(), last_decoded_frame_num_.cend());
    const size_t min_buffered_frame_num = std::max(
        0, static_cast<int>(frame_number) - kMaxBufferedInputFrames + 1);
    RTC_CHECK(min_last_decoded_frame_num != last_decoded_frame_num_.cend());
    const auto input_frames_erase_before = input_frames_.lower_bound(
        std::max(*min_last_decoded_frame_num, min_buffered_frame_num));
    input_frames_.erase(input_frames_.cbegin(), input_frames_erase_before);
  }

  if (decoded_frame_writers_) {
    ExtractI420BufferWithSize(decoded_frame, config_.codec_settings.width,
                              config_.codec_settings.height,
                              &decoded_frame_buffer_[spatial_idx]);
    RTC_CHECK_EQ(decoded_frame_buffer_[spatial_idx].size(),
                 decoded_frame_writers_->at(spatial_idx)->FrameLength());
    RTC_CHECK(decoded_frame_writers_->at(spatial_idx)
                  ->WriteFrame(decoded_frame_buffer_[spatial_idx].data()));
  }
}

void VideoProcessor::DecodeFrame(const EncodedImage& encoded_image,
                                 size_t spatial_idx) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  FrameStatistics* frame_stat =
      stats_->GetFrameWithTimestamp(encoded_image.Timestamp(), spatial_idx);

  frame_stat->decode_start_ns = rtc::TimeNanos();
  frame_stat->decode_return_code =
      decoders_->at(spatial_idx)->Decode(encoded_image, false, nullptr, 0);
}

const webrtc::EncodedImage* VideoProcessor::BuildAndStoreSuperframe(
    const EncodedImage& encoded_image,
    const VideoCodecType codec,
    size_t frame_number,
    size_t spatial_idx,
    bool inter_layer_predicted) {
  // Should only be called for SVC.
  RTC_CHECK_GT(config_.NumberOfSpatialLayers(), 1);

  EncodedImage base_image;
  RTC_CHECK_EQ(base_image._length, 0);

  // Each SVC layer is decoded with dedicated decoder. Find the nearest
  // non-dropped base frame and merge it and current frame into superframe.
  if (inter_layer_predicted) {
    for (int base_idx = static_cast<int>(spatial_idx) - 1; base_idx >= 0;
         --base_idx) {
      EncodedImage lower_layer = merged_encoded_frames_.at(base_idx);
      if (lower_layer.Timestamp() == encoded_image.Timestamp()) {
        base_image = lower_layer;
        break;
      }
    }
  }
  const size_t payload_size_bytes = base_image._length + encoded_image._length;
  const size_t buffer_size_bytes =
      payload_size_bytes + EncodedImage::GetBufferPaddingBytes(codec);

  uint8_t* copied_buffer = new uint8_t[buffer_size_bytes];
  RTC_CHECK(copied_buffer);

  if (base_image._length) {
    RTC_CHECK(base_image._buffer);
    memcpy(copied_buffer, base_image._buffer, base_image._length);
  }
  memcpy(copied_buffer + base_image._length, encoded_image._buffer,
         encoded_image._length);

  EncodedImage copied_image = encoded_image;
  copied_image = encoded_image;
  copied_image._buffer = copied_buffer;
  copied_image._length = payload_size_bytes;
  copied_image._size = buffer_size_bytes;

  // Replace previous EncodedImage for this spatial layer.
  uint8_t* old_buffer = merged_encoded_frames_.at(spatial_idx)._buffer;
  if (old_buffer) {
    delete[] old_buffer;
  }
  merged_encoded_frames_.at(spatial_idx) = copied_image;

  return &merged_encoded_frames_.at(spatial_idx);
}

}  // namespace test
}  // namespace webrtc
