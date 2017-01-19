/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class AbsoluteSendTime {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionAbsoluteSendTime;
  static constexpr uint8_t kValueSizeBytes = 3;
  static constexpr const char* kUri =
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

  static bool Parse(const uint8_t* data, uint32_t* time_24bits);
  static bool Write(uint8_t* data, int64_t time_ms);

  static constexpr uint32_t MsTo24Bits(int64_t time_ms) {
    return static_cast<uint32_t>(((time_ms << 18) + 500) / 1000) & 0x00FFFFFF;
  }
};

class AudioLevel {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionAudioLevel;
  static constexpr uint8_t kValueSizeBytes = 1;
  static constexpr const char* kUri =
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level";

  static bool Parse(const uint8_t* data,
                    bool* voice_activity,
                    uint8_t* audio_level);
  static bool Write(uint8_t* data, bool voice_activity, uint8_t audio_level);
};

class TransmissionOffset {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionTransmissionTimeOffset;
  static constexpr uint8_t kValueSizeBytes = 3;
  static constexpr const char* kUri = "urn:ietf:params:rtp-hdrext:toffset";

  static bool Parse(const uint8_t* data, int32_t* rtp_time);
  static bool Write(uint8_t* data, int32_t rtp_time);
};

class TransportSequenceNumber {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionTransportSequenceNumber;
  static constexpr uint8_t kValueSizeBytes = 2;
  static constexpr const char* kUri =
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01";
  static bool Parse(const uint8_t* data, uint16_t* value);
  static bool Write(uint8_t* data, uint16_t value);
};

class VideoOrientation {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionVideoRotation;
  static constexpr uint8_t kValueSizeBytes = 1;
  static constexpr const char* kUri = "urn:3gpp:video-orientation";

  static bool Parse(const uint8_t* data, VideoRotation* value);
  static bool Write(uint8_t* data, VideoRotation value);
  static bool Parse(const uint8_t* data, uint8_t* value);
  static bool Write(uint8_t* data, uint8_t value);
};

class PlayoutDelayLimits {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionPlayoutDelay;
  static constexpr uint8_t kValueSizeBytes = 3;
  static constexpr const char* kUri =
      "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

  // Playout delay in milliseconds. A playout delay limit (min or max)
  // has 12 bits allocated. This allows a range of 0-4095 values which
  // translates to a range of 0-40950 in milliseconds.
  static constexpr int kGranularityMs = 10;
  // Maximum playout delay value in milliseconds.
  static constexpr int kMaxMs = 0xfff * kGranularityMs;  // 40950.

  static bool Parse(const uint8_t* data, PlayoutDelay* playout_delay);
  static bool Write(uint8_t* data, const PlayoutDelay& playout_delay);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_
