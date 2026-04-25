/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_generator_impl.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/environment/environment.h"
#include "api/video/corruption_detection/corruption_detection_filter_settings.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_codec.h"
#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/utility/qp_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/corruption_detection_frame_selector_settings.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "video/corruption_detection/frame_selector.h"
#include "video/corruption_detection/generic_mapping_functions.h"
#include "video/corruption_detection/halton_frame_sampler.h"
#include "video/corruption_detection/utils.h"

namespace webrtc {
namespace {

// Avoid holding on to frames that might have been dropped by encoder, as that
// can lead to frame buffer pools draining.
// TODO: bugs.webrtc.org/358039777 - Once we have a reliable signal for dropped
// and completed frames, update this logic with a smarter culling logic.
constexpr size_t kMaxPendingFrames = 2;

std::optional<CorruptionDetectionFilterSettings> GetCorruptionFilterSettings(
    const EncodedImage& encoded_image,
    VideoCodecType video_codec_type,
    int layer_id) {
  std::optional<CorruptionDetectionFilterSettings> filter_settings =
      encoded_image.corruption_detection_filter_settings();

  if (!filter_settings.has_value()) {
    // No implementation specific filter settings available, using a generic
    // QP-based settings instead.
    int qp = encoded_image.qp_;
    if (qp == -1) {
      std::optional<uint32_t> parsed_qp =
          QpParser().Parse(video_codec_type, layer_id, encoded_image.data(),
                           encoded_image.size());
      if (!parsed_qp.has_value()) {
        RTC_LOG(LS_VERBOSE)
            << "Missing QP for " << CodecTypeToPayloadString(video_codec_type)
            << " layer " << layer_id << ".";
        return std::nullopt;
      }
      qp = *parsed_qp;
    }

    filter_settings = GetCorruptionFilterSettings(qp, video_codec_type);
  }
  return filter_settings;
}

std::unique_ptr<FrameSelector> CreateFrameSelector(
    const Environment* environment,
    VideoCodecType video_codec_type,
    std::optional<ScalabilityMode> scalability_mode) {
  if (!environment) {
    return nullptr;
  }

  CorruptionDetectionFrameSelectorSettings settings(
      environment->field_trials());
  if (!settings.is_enabled()) {
    return nullptr;
  }
  return std::make_unique<FrameSelector>(
      scalability_mode.value_or(ScalabilityMode::kL1T1),
      FrameSelector::Timespan{
          .lower_bound = settings.low_overhead_lower_bound(),
          .upper_bound = settings.low_overhead_upper_bound()},
      FrameSelector::Timespan{
          .lower_bound = settings.high_overhead_lower_bound(),
          .upper_bound = settings.high_overhead_upper_bound()});
}

}  // namespace

FrameInstrumentationGeneratorImpl::FrameInstrumentationGeneratorImpl(
    const Environment* environment,
    VideoCodecType video_codec_type,
    std::optional<ScalabilityMode> scalability_mode)
    : video_codec_type_(video_codec_type),
      frame_selector_(CreateFrameSelector(environment,
                                          video_codec_type,
                                          scalability_mode)) {}

void FrameInstrumentationGeneratorImpl::OnCapturedFrame(VideoFrame frame) {
  MutexLock lock(&mutex_);
  while (captured_frames_.size() >= kMaxPendingFrames) {
    captured_frames_.pop();
  }
  captured_frames_.push(frame);
}

std::optional<FrameInstrumentationData>
FrameInstrumentationGeneratorImpl::OnEncodedImage(
    const EncodedImage& encoded_image) {
  uint32_t rtp_timestamp_encoded_image = encoded_image.RtpTimestamp();
  std::optional<VideoFrame> captured_frame;
  int layer_id;
  FrameInstrumentationData data;
  std::vector<HaltonFrameSampler::Coordinates> sample_coordinates;
  {
    MutexLock lock(&mutex_);
    while (!captured_frames_.empty() &&
           IsNewerTimestamp(rtp_timestamp_encoded_image,
                            captured_frames_.front().rtp_timestamp())) {
      captured_frames_.pop();
    }
    if (captured_frames_.empty() || captured_frames_.front().rtp_timestamp() !=
                                        rtp_timestamp_encoded_image) {
      RTC_LOG(LS_VERBOSE) << "No captured frames for RTC timestamp "
                          << rtp_timestamp_encoded_image << ".";
      return std::nullopt;
    }
    captured_frame = captured_frames_.front();

    layer_id = GetSpatialLayerId(encoded_image);

    bool is_key_frame = encoded_image.IsKey();
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
      // TODO: bugs.webrtc.org/358039777 - Update this if statement such that
      // LxTy scalability modes work properly. It is not a problem for LxTy_KEY
      // scalability.
      //
      // For LxTy, it sometimes hinders calculating corruption score on the
      // higher spatial layers. Because e.g. in L3T1 the first frame might not
      // create 3 spatial layers but, only 2. Then, we end up not creating this
      // in the map and will therefore not get any corruption score until a new
      // key frame is sent.
      RTC_LOG(LS_INFO) << "The first frame of a spatial or simulcast layer is "
                          "not a key frame.";
      return std::nullopt;
    }

    int sequence_index = contexts_[layer_id].frame_sampler.GetCurrentIndex();
    if (is_key_frame && ((sequence_index & 0b0111'1111) != 0)) {
      // Increase until all the last 7 bits are zeroes.
      sequence_index >>= 7;
      sequence_index += 1;
      sequence_index <<= 7;
      contexts_[layer_id].frame_sampler.SetCurrentIndex(sequence_index);
    }

    if (sequence_index >= (1 << 14)) {
      // Overflow of 14 bit counter, reset to 0.
      sequence_index = 0;
      contexts_[layer_id].frame_sampler.SetCurrentIndex(sequence_index);
    }

    RTC_CHECK(data.SetSequenceIndex(sequence_index));

    // TODO: bugs.webrtc.org/358039777 - Maybe allow other sample sizes as well
    if (frame_selector_) {
      if (frame_selector_->ShouldInstrumentFrame(*captured_frame,
                                                 encoded_image)) {
        sample_coordinates =
            contexts_[layer_id].frame_sampler.GetSampleCoordinatesForFrame(
                /*num_samples=*/13);
      }
    } else {
      sample_coordinates =
          contexts_[layer_id]
              .frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
                  is_key_frame, captured_frame->rtp_timestamp(),
                  /*num_samples=*/13);
    }

    if (sample_coordinates.empty()) {
      if (!is_key_frame) {
        return std::nullopt;
      }
      // Sync message only.
      return data;
    }
  }
  RTC_DCHECK(captured_frame.has_value());
  RTC_DCHECK(!sample_coordinates.empty());

  std::optional<CorruptionDetectionFilterSettings> filter_settings =
      GetCorruptionFilterSettings(encoded_image, video_codec_type_, layer_id);
  if (!filter_settings.has_value()) {
    return std::nullopt;
  }

  RTC_CHECK(data.SetStdDev(filter_settings->std_dev));
  RTC_CHECK(data.SetLumaErrorThreshold(filter_settings->luma_error_threshold));
  RTC_CHECK(
      data.SetChromaErrorThreshold(filter_settings->chroma_error_threshold));

  std::vector<double> plain_values;
  std::vector<FilteredSample> samples = GetSampleValuesForFrame(
      *captured_frame, sample_coordinates, encoded_image._encodedWidth,
      encoded_image._encodedHeight, filter_settings->std_dev);
  plain_values.reserve(samples.size());
  absl::c_transform(samples, std::back_inserter(plain_values),
                    [](const FilteredSample& sample) { return sample.value; });

  RTC_CHECK(data.SetSampleValues(std::move(plain_values)));

  return data;
}

std::optional<int> FrameInstrumentationGeneratorImpl::GetHaltonSequenceIndex(
    int layer_id) const {
  MutexLock lock(&mutex_);
  auto it = contexts_.find(layer_id);
  if (it == contexts_.end()) {
    return std::nullopt;
  }
  return it->second.frame_sampler.GetCurrentIndex();
}

void FrameInstrumentationGeneratorImpl::SetHaltonSequenceIndex(int index,
                                                               int layer_id) {
  MutexLock lock(&mutex_);
  if (index <= 0x3FFF) {
    contexts_[layer_id].frame_sampler.SetCurrentIndex(index);
  }
  RTC_DCHECK_LE(index, 0x3FFF) << "Index must not be larger than 0x3FFF";
}

}  // namespace webrtc
