/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_PAYLOAD_REGISTRY_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_PAYLOAD_REGISTRY_H_

#include <map>
#include <set>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_format.h"
#include "api/video_codecs/video_codec.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class RTPPayloadRegistry {
 public:
  RTPPayloadRegistry();
  ~RTPPayloadRegistry();

  // TODO(magjed): Split RTPPayloadRegistry into separate Audio and Video class
  // and simplify the code. http://crbug/webrtc/6743.

  // Replace all audio receive payload types with the given map.
  void SetAudioReceivePayloads(std::map<int, SdpAudioFormat> codecs);

  int32_t RegisterReceivePayload(int payload_type,
                                 const SdpAudioFormat& audio_format,
                                 bool* created_new_payload_type);
  int32_t RegisterReceivePayload(const VideoCodec& video_codec);

  int32_t DeRegisterReceivePayload(int8_t payload_type);

  int GetPayloadTypeFrequency(uint8_t payload_type) const;

  absl::optional<RtpUtility::Payload> PayloadTypeToPayload(
      uint8_t payload_type) const;

 private:
  // Prunes the payload type map of the specific payload type, if it exists.
  void DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
      const SdpAudioFormat& audio_format);

  rtc::CriticalSection crit_sect_;
  std::map<int, RtpUtility::Payload> payload_type_map_;

// As a first step in splitting this class up in separate cases for audio and
// video, DCHECK that no instance is used for both audio and video.
#if RTC_DCHECK_IS_ON
  bool used_for_audio_ RTC_GUARDED_BY(crit_sect_) = false;
  bool used_for_video_ RTC_GUARDED_BY(crit_sect_) = false;
#endif
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_PAYLOAD_REGISTRY_H_
