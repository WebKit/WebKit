/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_payload_registry.h"

#include <algorithm>

#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"

namespace webrtc {

namespace {

bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                         const SdpAudioFormat& audio_format) {
  return payload.typeSpecific.is_audio() &&
         audio_format.Matches(payload.typeSpecific.audio_payload().format);
}

bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                         const VideoCodec& video_codec) {
  if (!payload.typeSpecific.is_video() ||
      _stricmp(payload.name, CodecTypeToPayloadString(video_codec.codecType)) !=
          0)
    return false;
  // For H264, profiles must match as well.
  if (video_codec.codecType == kVideoCodecH264) {
    return video_codec.H264().profile ==
           payload.typeSpecific.video_payload().h264_profile;
  }
  return true;
}

RtpUtility::Payload CreatePayloadType(const SdpAudioFormat& audio_format) {
  RTC_DCHECK_GE(audio_format.clockrate_hz, 1000);
  return {audio_format.name.c_str(),
          PayloadUnion(AudioPayload{audio_format, 0})};
}

RtpVideoCodecTypes ConvertToRtpVideoCodecType(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
    case kVideoCodecVP9:
    case kVideoCodecH264:
      return type;
    default:
      return kVideoCodecGeneric;
  }
}

RtpUtility::Payload CreatePayloadType(const VideoCodec& video_codec) {
  VideoPayload p;
  p.videoCodecType = ConvertToRtpVideoCodecType(video_codec.codecType);
  if (video_codec.codecType == kVideoCodecH264)
    p.h264_profile = video_codec.H264().profile;
  return {CodecTypeToPayloadString(video_codec.codecType), PayloadUnion(p)};
}

bool IsPayloadTypeValid(int8_t payload_type) {
  assert(payload_type >= 0);

  // Sanity check.
  switch (payload_type) {
    // Reserved payload types to avoid RTCP conflicts when marker bit is set.
    case 64:  //  192 Full INTRA-frame request.
    case 72:  //  200 Sender report.
    case 73:  //  201 Receiver report.
    case 74:  //  202 Source description.
    case 75:  //  203 Goodbye.
    case 76:  //  204 Application-defined.
    case 77:  //  205 Transport layer FB message.
    case 78:  //  206 Payload-specific FB message.
    case 79:  //  207 Extended report.
      RTC_LOG(LS_ERROR) << "Can't register invalid receiver payload type: "
                        << payload_type;
      return false;
    default:
      return true;
  }
}

}  // namespace

RTPPayloadRegistry::RTPPayloadRegistry() = default;

RTPPayloadRegistry::~RTPPayloadRegistry() = default;

void RTPPayloadRegistry::SetAudioReceivePayloads(
    std::map<int, SdpAudioFormat> codecs) {
  rtc::CritScope cs(&crit_sect_);

#if RTC_DCHECK_IS_ON
  RTC_DCHECK(!used_for_video_);
  used_for_audio_ = true;
#endif

  payload_type_map_.clear();
  for (const auto& kv : codecs) {
    const int& rtp_payload_type = kv.first;
    const SdpAudioFormat& audio_format = kv.second;
    RTC_DCHECK(IsPayloadTypeValid(rtp_payload_type));
    payload_type_map_.emplace(rtp_payload_type,
                              CreatePayloadType(audio_format));
  }
}

int32_t RTPPayloadRegistry::RegisterReceivePayload(
    int payload_type,
    const SdpAudioFormat& audio_format,
    bool* created_new_payload) {
  rtc::CritScope cs(&crit_sect_);

#if RTC_DCHECK_IS_ON
  RTC_DCHECK(!used_for_video_);
  used_for_audio_ = true;
#endif

  *created_new_payload = false;
  if (!IsPayloadTypeValid(payload_type))
    return -1;

  const auto it = payload_type_map_.find(payload_type);
  if (it != payload_type_map_.end()) {
    // We already use this payload type. Check if it's the same as we already
    // have. If same, ignore sending an error.
    if (PayloadIsCompatible(it->second, audio_format)) {
      it->second.typeSpecific.audio_payload().rate = 0;
      return 0;
    }
    RTC_LOG(LS_ERROR) << "Payload type already registered: " << payload_type;
    return -1;
  }

  // Audio codecs must be unique.
  DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(audio_format);

  const auto insert_status =
      payload_type_map_.emplace(payload_type, CreatePayloadType(audio_format));
  RTC_DCHECK(insert_status.second);  // Insertion succeeded.
  *created_new_payload = true;

  // Successful set of payload type.
  return 0;
}

int32_t RTPPayloadRegistry::RegisterReceivePayload(
    const VideoCodec& video_codec) {
  rtc::CritScope cs(&crit_sect_);

#if RTC_DCHECK_IS_ON
  RTC_DCHECK(!used_for_audio_);
  used_for_video_ = true;
#endif

  if (!IsPayloadTypeValid(video_codec.plType))
    return -1;

  auto it = payload_type_map_.find(video_codec.plType);
  if (it != payload_type_map_.end()) {
    // We already use this payload type. Check if it's the same as we already
    // have. If same, ignore sending an error.
    if (PayloadIsCompatible(it->second, video_codec))
      return 0;
    RTC_LOG(LS_ERROR) << "Payload type already registered: "
                      << static_cast<int>(video_codec.plType);
    return -1;
  }

  const auto insert_status = payload_type_map_.emplace(
      video_codec.plType, CreatePayloadType(video_codec));
  RTC_DCHECK(insert_status.second);  // Insertion succeeded.

  // Successful set of payload type.
  return 0;
}

int32_t RTPPayloadRegistry::DeRegisterReceivePayload(
    const int8_t payload_type) {
  rtc::CritScope cs(&crit_sect_);
  payload_type_map_.erase(payload_type);
  return 0;
}

// There can't be several codecs with the same rate, frequency and channels
// for audio codecs, but there can for video.
// Always called from within a critical section.
void RTPPayloadRegistry::DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
    const SdpAudioFormat& audio_format) {
  for (auto iterator = payload_type_map_.begin();
       iterator != payload_type_map_.end(); ++iterator) {
    if (PayloadIsCompatible(iterator->second, audio_format)) {
      // Remove old setting.
      payload_type_map_.erase(iterator);
      break;
    }
  }
}

int RTPPayloadRegistry::GetPayloadTypeFrequency(uint8_t payload_type) const {
  const auto payload = PayloadTypeToPayload(payload_type);
  if (!payload) {
    return -1;
  }
  rtc::CritScope cs(&crit_sect_);
  return payload->typeSpecific.is_audio()
             ? payload->typeSpecific.audio_payload().format.clockrate_hz
             : kVideoPayloadTypeFrequency;
}

absl::optional<RtpUtility::Payload> RTPPayloadRegistry::PayloadTypeToPayload(
    uint8_t payload_type) const {
  rtc::CritScope cs(&crit_sect_);
  const auto it = payload_type_map_.find(payload_type);
  return it == payload_type_map_.end()
             ? absl::nullopt
             : absl::optional<RtpUtility::Payload>(it->second);
}

}  // namespace webrtc
