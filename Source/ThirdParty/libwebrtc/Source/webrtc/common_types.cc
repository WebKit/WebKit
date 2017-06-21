/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"

#include <string.h>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "webrtc/base/checks.h"
#include "webrtc/base/stringutils.h"

namespace webrtc {

StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}

constexpr size_t StreamId::kMaxSize;

bool StreamId::IsLegalName(rtc::ArrayView<const char> name) {
  return (name.size() <= kMaxSize && name.size() > 0 &&
          std::all_of(name.data(), name.data() + name.size(), isalnum));
}

void StreamId::Set(const char* data, size_t size) {
  // If |data| contains \0, the stream id size might become less than |size|.
  RTC_CHECK_LE(size, kMaxSize);
  memcpy(value_, data, size);
  if (size < kMaxSize)
    value_[size] = 0;
}

// StreamId is used as member of RTPHeader that is sometimes copied with memcpy
// and thus assume trivial destructibility.
static_assert(std::is_trivially_destructible<StreamId>::value, "");

RTPHeaderExtension::RTPHeaderExtension()
    : hasTransmissionTimeOffset(false),
      transmissionTimeOffset(0),
      hasAbsoluteSendTime(false),
      absoluteSendTime(0),
      hasTransportSequenceNumber(false),
      transportSequenceNumber(0),
      hasAudioLevel(false),
      voiceActivity(false),
      audioLevel(0),
      hasVideoRotation(false),
      videoRotation(kVideoRotation_0),
      hasVideoContentType(false),
      videoContentType(VideoContentType::UNSPECIFIED),
      has_video_timing(false) {}

RTPHeader::RTPHeader()
    : markerBit(false),
      payloadType(0),
      sequenceNumber(0),
      timestamp(0),
      ssrc(0),
      numCSRCs(0),
      arrOfCSRCs(),
      paddingLength(0),
      headerLength(0),
      payload_type_frequency(0),
      extension() {}

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

static bool CodecNamesEq(const char* name1, const char* name2) {
  return _stricmp(name1, name2) == 0;
}

rtc::Optional<const char*> CodecTypeToPayloadName(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
      return rtc::Optional<const char*>(kPayloadNameVp8);
    case kVideoCodecVP9:
      return rtc::Optional<const char*>(kPayloadNameVp9);
    case kVideoCodecH264:
      return rtc::Optional<const char*>(kPayloadNameH264);
    case kVideoCodecI420:
      return rtc::Optional<const char*>(kPayloadNameI420);
    case kVideoCodecRED:
      return rtc::Optional<const char*>(kPayloadNameRED);
    case kVideoCodecULPFEC:
      return rtc::Optional<const char*>(kPayloadNameULPFEC);
    case kVideoCodecGeneric:
      return rtc::Optional<const char*>(kPayloadNameGeneric);
    default:
      return rtc::Optional<const char*>();
  }
}

rtc::Optional<VideoCodecType> PayloadNameToCodecType(const std::string& name) {
  if (CodecNamesEq(name.c_str(), kPayloadNameVp8))
    return rtc::Optional<VideoCodecType>(kVideoCodecVP8);
  if (CodecNamesEq(name.c_str(), kPayloadNameVp9))
    return rtc::Optional<VideoCodecType>(kVideoCodecVP9);
  if (CodecNamesEq(name.c_str(), kPayloadNameH264))
    return rtc::Optional<VideoCodecType>(kVideoCodecH264);
  if (CodecNamesEq(name.c_str(), kPayloadNameI420))
    return rtc::Optional<VideoCodecType>(kVideoCodecI420);
  if (CodecNamesEq(name.c_str(), kPayloadNameRED))
    return rtc::Optional<VideoCodecType>(kVideoCodecRED);
  if (CodecNamesEq(name.c_str(), kPayloadNameULPFEC))
    return rtc::Optional<VideoCodecType>(kVideoCodecULPFEC);
  if (CodecNamesEq(name.c_str(), kPayloadNameGeneric))
    return rtc::Optional<VideoCodecType>(kVideoCodecGeneric);
  return rtc::Optional<VideoCodecType>();
}

const uint32_t BitrateAllocation::kMaxBitrateBps =
    std::numeric_limits<uint32_t>::max();

BitrateAllocation::BitrateAllocation() : sum_(0), bitrates_{} {}

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
  sum_ = static_cast<uint32_t>(new_bitrate_sum_bps);
  return true;
}

uint32_t BitrateAllocation::GetBitrate(size_t spatial_index,
                                       size_t temporal_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  return bitrates_[spatial_index][temporal_index];
}

// Get the sum of all the temporal layer for a specific spatial layer.
uint32_t BitrateAllocation::GetSpatialLayerSum(size_t spatial_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  uint32_t sum = 0;
  for (int i = 0; i < kMaxTemporalStreams; ++i)
    sum += bitrates_[spatial_index][i];
  return sum;
}

}  // namespace webrtc
