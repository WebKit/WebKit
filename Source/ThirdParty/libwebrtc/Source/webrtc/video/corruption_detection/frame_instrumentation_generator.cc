/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_generator.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/types/variant.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_codec.h"
#include "common_video/frame_instrumentation_data.h"
#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/utility/qp_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/corruption_detection/generic_mapping_functions.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {
namespace {

// Avoid holding on to frames that might have been dropped by encoder, as that
// can lead to frame buffer pools draining.
constexpr size_t kMaxPendingFrames = 3;

std::optional<FilterSettings> GetCorruptionFilterSettings(
    const EncodedImage& encoded_image,
    VideoCodecType video_codec_type,
    int layer_id) {
  /* TODO: bugs.webrtc.org/358039777 - Uncomment when parameters are available
     in EncodedImage.
  if (encoded_image.CorruptionDetectionParameters()) {
    return FilterSettings{
        .std_dev = encoded_image.CorruptionDetectionParameters()->std_dev,
        .luma_error_threshold =
            encoded_image.CorruptionDetectionParameters()->luma_error_threshold,
        .chroma_error_threshold = encoded_image.CorruptionDetectionParameters()
                                      ->chroma_error_threshold};
  }
  */

  int qp = encoded_image.qp_;
  if (qp == -1) {
    std::optional<uint32_t> parsed_qp = QpParser().Parse(
        video_codec_type, layer_id, encoded_image.data(), encoded_image.size());
    if (!parsed_qp.has_value()) {
      RTC_LOG(LS_VERBOSE) << "Missing QP for "
                          << CodecTypeToPayloadString(video_codec_type)
                          << " layer " << layer_id << ".";
      return std::nullopt;
    }
    qp = *parsed_qp;
  }

  return GetCorruptionFilterSettings(qp, video_codec_type);
}

}  // namespace

FrameInstrumentationGenerator::FrameInstrumentationGenerator(
    VideoCodecType video_codec_type)
    : video_codec_type_(video_codec_type) {}

void FrameInstrumentationGenerator::OnCapturedFrame(VideoFrame frame) {
  while (captured_frames_.size() >= kMaxPendingFrames) {
    captured_frames_.pop();
  }
  captured_frames_.push(frame);
}

std::optional<
    absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
FrameInstrumentationGenerator::OnEncodedImage(
    const EncodedImage& encoded_image) {
  uint32_t rtp_timestamp_encoded_image = encoded_image.RtpTimestamp();
  while (!captured_frames_.empty() &&
         IsNewerTimestamp(rtp_timestamp_encoded_image,
                          captured_frames_.front().rtp_timestamp())) {
    captured_frames_.pop();
  }
  if (captured_frames_.empty() ||
      captured_frames_.front().rtp_timestamp() != rtp_timestamp_encoded_image) {
    RTC_LOG(LS_VERBOSE) << "No captured frames for RTC timestamp "
                        << rtp_timestamp_encoded_image << ".";
    return std::nullopt;
  }
  VideoFrame captured_frame = captured_frames_.front();

  int layer_id = GetLayerId(encoded_image);

  bool is_key_frame =
      encoded_image.FrameType() == VideoFrameType::kVideoFrameKey;
  if (!is_key_frame) {
    for (const auto& [unused, context] : contexts_) {
      if (context.rtp_timestamp_of_last_key_frame ==
          rtp_timestamp_encoded_image) {
        // Upper layer of an SVC key frame.
        is_key_frame = true;
        break;
      }
    }
  }
  if (is_key_frame) {
    contexts_[layer_id].rtp_timestamp_of_last_key_frame =
        encoded_image.RtpTimestamp();
  } else if (contexts_.find(layer_id) == contexts_.end()) {
    RTC_LOG(LS_INFO) << "The first frame of a spatial or simulcast layer is "
                        "not a key frame.";
    return std::nullopt;
  }

  int sequence_index = contexts_[layer_id].frame_sampler.GetCurrentIndex();
  bool communicate_upper_bits = false;
  if (is_key_frame) {
    communicate_upper_bits = true;
    // Increase until all the last 7 bits are zeroes.

    // If this would overflow to 15 bits, reset to 0.
    if (sequence_index > 0b0011'1111'1000'0000) {
      sequence_index = 0;
    } else if ((sequence_index & 0b0111'1111) != 0) {
      // Last 7 bits are not all zeroes.
      sequence_index >>= 7;
      sequence_index += 1;
      sequence_index <<= 7;
    }
    contexts_[layer_id].frame_sampler.SetCurrentIndex(sequence_index);
  }

  // TODO: bugs.webrtc.org/358039777 - Maybe allow other sample sizes as well
  std::vector<HaltonFrameSampler::Coordinates> sample_coordinates =
      contexts_[layer_id]
          .frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
              is_key_frame, captured_frame.rtp_timestamp(),
              /*num_samples=*/13);
  if (sample_coordinates.empty()) {
    if (!is_key_frame) {
      return std::nullopt;
    }
    return FrameInstrumentationSyncData{.sequence_index = sequence_index,
                                        .communicate_upper_bits = true};
  }

  std::optional<FilterSettings> filter_settings =
      GetCorruptionFilterSettings(encoded_image, video_codec_type_, layer_id);
  if (!filter_settings.has_value()) {
    return std::nullopt;
  }

  scoped_refptr<I420BufferInterface> captured_frame_buffer_as_i420 =
      captured_frame.video_frame_buffer()->ToI420();
  if (!captured_frame_buffer_as_i420) {
    RTC_LOG(LS_ERROR) << "Failed to convert "
                      << VideoFrameBufferTypeToString(
                             captured_frame.video_frame_buffer()->type())
                      << " image to I420.";
    return std::nullopt;
  }

  FrameInstrumentationData data = {
      .sequence_index = sequence_index,
      .communicate_upper_bits = communicate_upper_bits,
      .std_dev = filter_settings->std_dev,
      .luma_error_threshold = filter_settings->luma_error_threshold,
      .chroma_error_threshold = filter_settings->chroma_error_threshold};
  std::vector<FilteredSample> samples = GetSampleValuesForFrame(
      captured_frame_buffer_as_i420, sample_coordinates,
      encoded_image._encodedWidth, encoded_image._encodedHeight,
      filter_settings->std_dev);
  data.sample_values.reserve(samples.size());
  absl::c_transform(samples, std::back_inserter(data.sample_values),
                    [](const FilteredSample& sample) { return sample.value; });
  return data;
}

std::optional<int> FrameInstrumentationGenerator::GetHaltonSequenceIndex(
    int layer_id) const {
  auto it = contexts_.find(layer_id);
  if (it == contexts_.end()) {
    return std::nullopt;
  }
  return it->second.frame_sampler.GetCurrentIndex();
}

void FrameInstrumentationGenerator::SetHaltonSequenceIndex(int index,
                                                           int layer_id) {
  if (index <= 0x3FFF) {
    contexts_[layer_id].frame_sampler.SetCurrentIndex(index);
  }
  RTC_DCHECK_LE(index, 0x3FFF) << "Index must not be larger than 0x3FFF";
}

int FrameInstrumentationGenerator::GetLayerId(
    const EncodedImage& encoded_image) const {
  return std::max(encoded_image.SpatialIndex().value_or(0),
                  encoded_image.SimulcastIndex().value_or(0));
}
}  // namespace webrtc
