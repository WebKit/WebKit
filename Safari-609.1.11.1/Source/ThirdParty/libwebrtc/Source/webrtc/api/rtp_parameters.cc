/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/rtp_parameters.h"

#include <algorithm>
#include <string>

#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

const double kDefaultBitratePriority = 1.0;

RtcpFeedback::RtcpFeedback() = default;
RtcpFeedback::RtcpFeedback(RtcpFeedbackType type) : type(type) {}
RtcpFeedback::RtcpFeedback(RtcpFeedbackType type,
                           RtcpFeedbackMessageType message_type)
    : type(type), message_type(message_type) {}
RtcpFeedback::RtcpFeedback(const RtcpFeedback& rhs) = default;
RtcpFeedback::~RtcpFeedback() = default;

RtpCodecCapability::RtpCodecCapability() = default;
RtpCodecCapability::~RtpCodecCapability() = default;

RtpHeaderExtensionCapability::RtpHeaderExtensionCapability() = default;
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    const std::string& uri)
    : uri(uri) {}
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    const std::string& uri,
    int preferred_id)
    : uri(uri), preferred_id(preferred_id) {}
RtpHeaderExtensionCapability::~RtpHeaderExtensionCapability() = default;

RtpExtension::RtpExtension() = default;
RtpExtension::RtpExtension(const std::string& uri, int id) : uri(uri), id(id) {}
RtpExtension::RtpExtension(const std::string& uri, int id, bool encrypt)
    : uri(uri), id(id), encrypt(encrypt) {}
RtpExtension::~RtpExtension() = default;

RtpFecParameters::RtpFecParameters() = default;
RtpFecParameters::RtpFecParameters(FecMechanism mechanism)
    : mechanism(mechanism) {}
RtpFecParameters::RtpFecParameters(FecMechanism mechanism, uint32_t ssrc)
    : ssrc(ssrc), mechanism(mechanism) {}
RtpFecParameters::RtpFecParameters(const RtpFecParameters& rhs) = default;
RtpFecParameters::~RtpFecParameters() = default;

RtpRtxParameters::RtpRtxParameters() = default;
RtpRtxParameters::RtpRtxParameters(uint32_t ssrc) : ssrc(ssrc) {}
RtpRtxParameters::RtpRtxParameters(const RtpRtxParameters& rhs) = default;
RtpRtxParameters::~RtpRtxParameters() = default;

RtpEncodingParameters::RtpEncodingParameters() = default;
RtpEncodingParameters::RtpEncodingParameters(const RtpEncodingParameters& rhs) =
    default;
RtpEncodingParameters::~RtpEncodingParameters() = default;

RtpCodecParameters::RtpCodecParameters() = default;
RtpCodecParameters::RtpCodecParameters(const RtpCodecParameters& rhs) = default;
RtpCodecParameters::~RtpCodecParameters() = default;

RtpCapabilities::RtpCapabilities() = default;
RtpCapabilities::~RtpCapabilities() = default;

RtcpParameters::RtcpParameters() = default;
RtcpParameters::RtcpParameters(const RtcpParameters& rhs) = default;
RtcpParameters::~RtcpParameters() = default;

RtpParameters::RtpParameters() = default;
RtpParameters::RtpParameters(const RtpParameters& rhs) = default;
RtpParameters::~RtpParameters() = default;

std::string RtpExtension::ToString() const {
  char buf[256];
  rtc::SimpleStringBuilder sb(buf);
  sb << "{uri: " << uri;
  sb << ", id: " << id;
  if (encrypt) {
    sb << ", encrypt";
  }
  sb << '}';
  return sb.str();
}

const char RtpExtension::kAudioLevelUri[] =
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level";

const char RtpExtension::kTimestampOffsetUri[] =
    "urn:ietf:params:rtp-hdrext:toffset";

const char RtpExtension::kAbsSendTimeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

const char RtpExtension::kAbsoluteCaptureTimeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time";

const char RtpExtension::kVideoRotationUri[] = "urn:3gpp:video-orientation";

const char RtpExtension::kTransportSequenceNumberUri[] =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
const char RtpExtension::kTransportSequenceNumberV2Uri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02";

// This extension allows applications to adaptively limit the playout delay
// on frames as per the current needs. For example, a gaming application
// has very different needs on end-to-end delay compared to a video-conference
// application.
const char RtpExtension::kPlayoutDelayUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

const char RtpExtension::kVideoContentTypeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type";

const char RtpExtension::kVideoTimingUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/video-timing";

const char RtpExtension::kMidUri[] = "urn:ietf:params:rtp-hdrext:sdes:mid";

const char RtpExtension::kFrameMarkingUri[] =
    "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07";

const char RtpExtension::kGenericFrameDescriptorUri00[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00";
const char RtpExtension::kGenericFrameDescriptorUri01[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-01";
const char RtpExtension::kGenericFrameDescriptorUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00";

const char RtpExtension::kEncryptHeaderExtensionsUri[] =
    "urn:ietf:params:rtp-hdrext:encrypt";

const char RtpExtension::kColorSpaceUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/color-space";

const char RtpExtension::kRidUri[] =
    "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";

const char RtpExtension::kRepairedRidUri[] =
    "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";

constexpr int RtpExtension::kMinId;
constexpr int RtpExtension::kMaxId;
constexpr int RtpExtension::kMaxValueSize;
constexpr int RtpExtension::kOneByteHeaderExtensionMaxId;
constexpr int RtpExtension::kOneByteHeaderExtensionMaxValueSize;

bool RtpExtension::IsSupportedForAudio(const std::string& uri) {
  return uri == webrtc::RtpExtension::kAudioLevelUri ||
         uri == webrtc::RtpExtension::kAbsSendTimeUri ||
         // TODO(bugs.webrtc.org/10739): Uncomment once the audio impl is ready.
         // uri == webrtc::RtpExtension::kAbsoluteCaptureTimeUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberV2Uri ||
         uri == webrtc::RtpExtension::kMidUri ||
         uri == webrtc::RtpExtension::kRidUri ||
         uri == webrtc::RtpExtension::kRepairedRidUri;
}

bool RtpExtension::IsSupportedForVideo(const std::string& uri) {
  return uri == webrtc::RtpExtension::kTimestampOffsetUri ||
         uri == webrtc::RtpExtension::kAbsSendTimeUri ||
         // TODO(bugs.webrtc.org/10739): Uncomment once the video impl is ready.
         // uri == webrtc::RtpExtension::kAbsoluteCaptureTimeUri ||
         uri == webrtc::RtpExtension::kVideoRotationUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberV2Uri ||
         uri == webrtc::RtpExtension::kPlayoutDelayUri ||
         uri == webrtc::RtpExtension::kVideoContentTypeUri ||
         uri == webrtc::RtpExtension::kVideoTimingUri ||
         uri == webrtc::RtpExtension::kMidUri ||
         uri == webrtc::RtpExtension::kFrameMarkingUri ||
         uri == webrtc::RtpExtension::kGenericFrameDescriptorUri00 ||
         uri == webrtc::RtpExtension::kGenericFrameDescriptorUri01 ||
         uri == webrtc::RtpExtension::kColorSpaceUri ||
         uri == webrtc::RtpExtension::kRidUri ||
         uri == webrtc::RtpExtension::kRepairedRidUri;
}

bool RtpExtension::IsEncryptionSupported(const std::string& uri) {
  return uri == webrtc::RtpExtension::kAudioLevelUri ||
         uri == webrtc::RtpExtension::kTimestampOffsetUri ||
#if !defined(ENABLE_EXTERNAL_AUTH)
         // TODO(jbauch): Figure out a way to always allow "kAbsSendTimeUri"
         // here and filter out later if external auth is really used in
         // srtpfilter. External auth is used by Chromium and replaces the
         // extension header value of "kAbsSendTimeUri", so it must not be
         // encrypted (which can't be done by Chromium).
         uri == webrtc::RtpExtension::kAbsSendTimeUri ||
#endif
         uri == webrtc::RtpExtension::kAbsoluteCaptureTimeUri ||
         uri == webrtc::RtpExtension::kVideoRotationUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberV2Uri ||
         uri == webrtc::RtpExtension::kPlayoutDelayUri ||
         uri == webrtc::RtpExtension::kVideoContentTypeUri ||
         uri == webrtc::RtpExtension::kMidUri ||
         uri == webrtc::RtpExtension::kRidUri ||
         uri == webrtc::RtpExtension::kRepairedRidUri;
}

const RtpExtension* RtpExtension::FindHeaderExtensionByUri(
    const std::vector<RtpExtension>& extensions,
    const std::string& uri) {
  for (const auto& extension : extensions) {
    if (extension.uri == uri) {
      return &extension;
    }
  }
  return nullptr;
}

std::vector<RtpExtension> RtpExtension::FilterDuplicateNonEncrypted(
    const std::vector<RtpExtension>& extensions) {
  std::vector<RtpExtension> filtered;
  for (auto extension = extensions.begin(); extension != extensions.end();
       ++extension) {
    if (extension->encrypt) {
      filtered.push_back(*extension);
      continue;
    }

    // Only add non-encrypted extension if no encrypted with the same URI
    // is also present...
    if (std::any_of(extension + 1, extensions.end(),
                    [&](const RtpExtension& check) {
                      return extension->uri == check.uri;
                    })) {
      continue;
    }

    // ...and has not been added before.
    if (!FindHeaderExtensionByUri(filtered, extension->uri)) {
      filtered.push_back(*extension);
    }
  }
  return filtered;
}
}  // namespace webrtc
