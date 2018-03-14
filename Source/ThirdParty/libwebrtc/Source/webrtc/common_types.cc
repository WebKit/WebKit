/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_types.h"  // NOLINT(build/include)

#include <string.h>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "rtc_base/checks.h"
#include "rtc_base/stringutils.h"

namespace webrtc {

VideoCodec::VideoCodec()
    : codecType(kVideoCodecUnknown),
      plName(),
      plType(0),
      width(0),
      height(0),
      startBitrate(0),
      maxBitrate(0),
      minBitrate(0),
      targetBitrate(0),
      maxFramerate(0),
      qpMax(0),
      numberOfSimulcastStreams(0),
      simulcastStream(),
      spatialLayers(),
      mode(kRealtimeVideo),
      expect_encode_from_texture(false),
      timing_frame_thresholds({0, 0}),
      codec_specific_() {}

VideoCodecVP8* VideoCodec::VP8() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return &codec_specific_.VP8;
}

const VideoCodecVP8& VideoCodec::VP8() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return codec_specific_.VP8;
}

VideoCodecVP9* VideoCodec::VP9() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return &codec_specific_.VP9;
}

const VideoCodecVP9& VideoCodec::VP9() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return codec_specific_.VP9;
}

VideoCodecH264* VideoCodec::H264() {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return &codec_specific_.H264;
}

const VideoCodecH264& VideoCodec::H264() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return codec_specific_.H264;
}

static const char* kPayloadNameVp8 = "VP8";
static const char* kPayloadNameVp9 = "VP9";
static const char* kPayloadNameH264 = "H264";
static const char* kPayloadNameI420 = "I420";
static const char* kPayloadNameRED = "RED";
static const char* kPayloadNameULPFEC = "ULPFEC";
static const char* kPayloadNameGeneric = "Generic";
static const char* kPayloadNameStereo = "Stereo";

static bool CodecNamesEq(const char* name1, const char* name2) {
  return _stricmp(name1, name2) == 0;
}

const char* CodecTypeToPayloadString(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
      return kPayloadNameVp8;
    case kVideoCodecVP9:
      return kPayloadNameVp9;
    case kVideoCodecH264:
      return kPayloadNameH264;
    case kVideoCodecI420:
      return kPayloadNameI420;
    case kVideoCodecRED:
      return kPayloadNameRED;
    case kVideoCodecULPFEC:
      return kPayloadNameULPFEC;
    // Other codecs default to generic.
    case kVideoCodecStereo:
    case kVideoCodecFlexfec:
    case kVideoCodecGeneric:
    case kVideoCodecUnknown:
      return kPayloadNameGeneric;
  }
  return kPayloadNameGeneric;
}

VideoCodecType PayloadStringToCodecType(const std::string& name) {
  if (CodecNamesEq(name.c_str(), kPayloadNameVp8))
    return kVideoCodecVP8;
  if (CodecNamesEq(name.c_str(), kPayloadNameVp9))
    return kVideoCodecVP9;
  if (CodecNamesEq(name.c_str(), kPayloadNameH264))
    return kVideoCodecH264;
  if (CodecNamesEq(name.c_str(), kPayloadNameI420))
    return kVideoCodecI420;
  if (CodecNamesEq(name.c_str(), kPayloadNameRED))
    return kVideoCodecRED;
  if (CodecNamesEq(name.c_str(), kPayloadNameULPFEC))
    return kVideoCodecULPFEC;
  if (CodecNamesEq(name.c_str(), kPayloadNameStereo))
    return kVideoCodecStereo;
  return kVideoCodecGeneric;
}

const uint32_t BitrateAllocation::kMaxBitrateBps =
    std::numeric_limits<uint32_t>::max();

BitrateAllocation::BitrateAllocation() : sum_(0), bitrates_{}, has_bitrate_{} {}

bool BitrateAllocation::SetBitrate(size_t spatial_index,
                                   size_t temporal_index,
                                   uint32_t bitrate_bps) {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  RTC_CHECK_LE(bitrates_[spatial_index][temporal_index], sum_);
  uint64_t new_bitrate_sum_bps = sum_;
  new_bitrate_sum_bps -= bitrates_[spatial_index][temporal_index];
  new_bitrate_sum_bps += bitrate_bps;
  if (new_bitrate_sum_bps > kMaxBitrateBps)
    return false;

  bitrates_[spatial_index][temporal_index] = bitrate_bps;
  has_bitrate_[spatial_index][temporal_index] = true;
  sum_ = static_cast<uint32_t>(new_bitrate_sum_bps);
  return true;
}

bool BitrateAllocation::HasBitrate(size_t spatial_index,
                                   size_t temporal_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  return has_bitrate_[spatial_index][temporal_index];
}

uint32_t BitrateAllocation::GetBitrate(size_t spatial_index,
                                       size_t temporal_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  return bitrates_[spatial_index][temporal_index];
}

// Whether the specific spatial layers has the bitrate set in any of its
// temporal layers.
bool BitrateAllocation::IsSpatialLayerUsed(size_t spatial_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  for (int i = 0; i < kMaxTemporalStreams; ++i) {
    if (has_bitrate_[spatial_index][i])
      return true;
  }
  return false;
}

// Get the sum of all the temporal layer for a specific spatial layer.
uint32_t BitrateAllocation::GetSpatialLayerSum(size_t spatial_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  uint32_t sum = 0;
  for (int i = 0; i < kMaxTemporalStreams; ++i)
    sum += bitrates_[spatial_index][i];
  return sum;
}

std::string BitrateAllocation::ToString() const {
  if (sum_ == 0)
    return "BitrateAllocation [ [] ]";

  // TODO(sprang): Replace this stringstream with something cheaper.
  std::ostringstream oss;
  oss << "BitrateAllocation [";
  uint32_t spatial_cumulator = 0;
  for (int si = 0; si < kMaxSpatialLayers; ++si) {
    RTC_DCHECK_LE(spatial_cumulator, sum_);
    if (spatial_cumulator == sum_)
      break;

    const uint32_t layer_sum = GetSpatialLayerSum(si);
    if (layer_sum == sum_) {
      oss << " [";
    } else {
      if (si > 0)
        oss << ",";
      oss << std::endl << "  [";
    }
    spatial_cumulator += layer_sum;

    uint32_t temporal_cumulator = 0;
    for (int ti = 0; ti < kMaxTemporalStreams; ++ti) {
      RTC_DCHECK_LE(temporal_cumulator, layer_sum);
      if (temporal_cumulator == layer_sum)
        break;

      if (ti > 0)
        oss << ", ";

      uint32_t bitrate = bitrates_[si][ti];
      oss << bitrate;
      temporal_cumulator += bitrate;
    }
    oss << "]";
  }

  RTC_DCHECK_EQ(spatial_cumulator, sum_);
  oss << " ]";
  return oss.str();
}

std::ostream& BitrateAllocation::operator<<(std::ostream& os) const {
  os << ToString();
  return os;
}

}  // namespace webrtc
