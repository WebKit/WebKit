/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_

#include <stdint.h>
#include <string>

#include "api/array_view.h"
#include "api/video/video_content_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class AbsoluteSendTime {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionAbsoluteSendTime;
  static constexpr uint8_t kValueSizeBytes = 3;
  static constexpr const char kUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

  static bool Parse(rtc::ArrayView<const uint8_t> data, uint32_t* time_24bits);
  static size_t ValueSize(uint32_t time_24bits) { return kValueSizeBytes; }
  static bool Write(rtc::ArrayView<uint8_t> data, uint32_t time_24bits);

  static constexpr uint32_t MsTo24Bits(int64_t time_ms) {
    return static_cast<uint32_t>(((time_ms << 18) + 500) / 1000) & 0x00FFFFFF;
  }
};

class AudioLevel {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionAudioLevel;
  static constexpr uint8_t kValueSizeBytes = 1;
  static constexpr const char kUri[] =
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level";

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    bool* voice_activity,
                    uint8_t* audio_level);
  static size_t ValueSize(bool voice_activity, uint8_t audio_level) {
    return kValueSizeBytes;
  }
  static bool Write(rtc::ArrayView<uint8_t> data,
                    bool voice_activity,
                    uint8_t audio_level);
};

class TransmissionOffset {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionTransmissionTimeOffset;
  static constexpr uint8_t kValueSizeBytes = 3;
  static constexpr const char kUri[] = "urn:ietf:params:rtp-hdrext:toffset";

  static bool Parse(rtc::ArrayView<const uint8_t> data, int32_t* rtp_time);
  static size_t ValueSize(int32_t rtp_time) { return kValueSizeBytes; }
  static bool Write(rtc::ArrayView<uint8_t> data, int32_t rtp_time);
};

class TransportSequenceNumber {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionTransportSequenceNumber;
  static constexpr uint8_t kValueSizeBytes = 2;
  static constexpr const char kUri[] =
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01";
  static bool Parse(rtc::ArrayView<const uint8_t> data, uint16_t* value);
  static size_t ValueSize(uint16_t value) { return kValueSizeBytes; }
  static bool Write(rtc::ArrayView<uint8_t> data, uint16_t value);
};

class VideoOrientation {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionVideoRotation;
  static constexpr uint8_t kValueSizeBytes = 1;
  static constexpr const char kUri[] = "urn:3gpp:video-orientation";

  static bool Parse(rtc::ArrayView<const uint8_t> data, VideoRotation* value);
  static size_t ValueSize(VideoRotation) { return kValueSizeBytes; }
  static bool Write(rtc::ArrayView<uint8_t> data, VideoRotation value);
  static bool Parse(rtc::ArrayView<const uint8_t> data, uint8_t* value);
  static size_t ValueSize(uint8_t value) { return kValueSizeBytes; }
  static bool Write(rtc::ArrayView<uint8_t> data, uint8_t value);
};

class PlayoutDelayLimits {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionPlayoutDelay;
  static constexpr uint8_t kValueSizeBytes = 3;
  static constexpr const char kUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

  // Playout delay in milliseconds. A playout delay limit (min or max)
  // has 12 bits allocated. This allows a range of 0-4095 values which
  // translates to a range of 0-40950 in milliseconds.
  static constexpr int kGranularityMs = 10;
  // Maximum playout delay value in milliseconds.
  static constexpr int kMaxMs = 0xfff * kGranularityMs;  // 40950.

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    PlayoutDelay* playout_delay);
  static size_t ValueSize(const PlayoutDelay&) {
    return kValueSizeBytes;
  }
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const PlayoutDelay& playout_delay);
};

class VideoContentTypeExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionVideoContentType;
  static constexpr uint8_t kValueSizeBytes = 1;
  static constexpr const char kUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type";

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    VideoContentType* content_type);
  static size_t ValueSize(VideoContentType) {
    return kValueSizeBytes;
  }
  static bool Write(rtc::ArrayView<uint8_t> data,
                    VideoContentType content_type);
};

class VideoTimingExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionVideoTiming;
  static constexpr uint8_t kValueSizeBytes = 13;
  static constexpr const char kUri[] =
      "http://www.webrtc.org/experiments/rtp-hdrext/video-timing";

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    VideoSendTiming* timing);
  static size_t ValueSize(const VideoSendTiming&) { return kValueSizeBytes; }
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const VideoSendTiming& timing);

  static size_t ValueSize(uint16_t time_delta_ms, uint8_t idx) {
    return kValueSizeBytes;
  }
  // Writes only single time delta to position idx.
  static bool Write(rtc::ArrayView<uint8_t> data,
                    uint16_t time_delta_ms,
                    uint8_t idx);
};

class FrameMarkingExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionFrameMarking;
  static constexpr const char kUri[] =
      "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07";

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    FrameMarking* frame_marking);
  static size_t ValueSize(const FrameMarking& frame_marking);
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const FrameMarking& frame_marking);

 private:
  static bool IsScalable(uint8_t temporal_id, uint8_t layer_id);
};

// Base extension class for RTP header extensions which are strings.
// Subclasses must defined kId and kUri static constexpr members.
class BaseRtpStringExtension {
 public:
  // String RTP header extensions are limited to 16 bytes because it is the
  // maximum length that can be encoded with one-byte header extensions.
  static constexpr uint8_t kMaxValueSizeBytes = 16;

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    StringRtpHeaderExtension* str);
  static size_t ValueSize(const StringRtpHeaderExtension& str) {
    return str.size();
  }
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const StringRtpHeaderExtension& str);

  static bool Parse(rtc::ArrayView<const uint8_t> data, std::string* str);
  static size_t ValueSize(const std::string& str) { return str.size(); }
  static bool Write(rtc::ArrayView<uint8_t> data, const std::string& str);
};

class RtpStreamId : public BaseRtpStringExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionRtpStreamId;
  static constexpr const char kUri[] =
      "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
};

class RepairedRtpStreamId : public BaseRtpStringExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionRepairedRtpStreamId;
  static constexpr const char kUri[] =
      "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";
};

class RtpMid : public BaseRtpStringExtension {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionMid;
  static constexpr const char kUri[] = "urn:ietf:params:rtp-hdrext:sdes:mid";
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_
