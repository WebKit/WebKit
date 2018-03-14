/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/rtpparameters.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "rtc_base/checks.h"

namespace webrtc {

const double kDefaultBitratePriority = 1.0;

RtcpFeedback::RtcpFeedback() {}
RtcpFeedback::RtcpFeedback(RtcpFeedbackType type) : type(type) {}
RtcpFeedback::RtcpFeedback(RtcpFeedbackType type,
                           RtcpFeedbackMessageType message_type)
    : type(type), message_type(message_type) {}
RtcpFeedback::~RtcpFeedback() {}

RtpCodecCapability::RtpCodecCapability() {}
RtpCodecCapability::~RtpCodecCapability() {}

RtpHeaderExtensionCapability::RtpHeaderExtensionCapability() {}
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    const std::string& uri)
    : uri(uri) {}
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    const std::string& uri,
    int preferred_id)
    : uri(uri), preferred_id(preferred_id) {}
RtpHeaderExtensionCapability::~RtpHeaderExtensionCapability() {}

RtpExtension::RtpExtension() {}
RtpExtension::RtpExtension(const std::string& uri, int id) : uri(uri), id(id) {}
RtpExtension::RtpExtension(const std::string& uri, int id, bool encrypt)
    : uri(uri), id(id), encrypt(encrypt) {}
RtpExtension::~RtpExtension() {}

RtpFecParameters::RtpFecParameters() {}
RtpFecParameters::RtpFecParameters(FecMechanism mechanism)
    : mechanism(mechanism) {}
RtpFecParameters::RtpFecParameters(FecMechanism mechanism, uint32_t ssrc)
    : ssrc(ssrc), mechanism(mechanism) {}
RtpFecParameters::~RtpFecParameters() {}

RtpRtxParameters::RtpRtxParameters() {}
RtpRtxParameters::RtpRtxParameters(uint32_t ssrc) : ssrc(ssrc) {}
RtpRtxParameters::~RtpRtxParameters() {}

RtpEncodingParameters::RtpEncodingParameters() {}
RtpEncodingParameters::~RtpEncodingParameters() {}

RtpCodecParameters::RtpCodecParameters() {}
RtpCodecParameters::~RtpCodecParameters() {}

RtpCapabilities::RtpCapabilities() {}
RtpCapabilities::~RtpCapabilities() {}

RtpParameters::RtpParameters() {}
RtpParameters::~RtpParameters() {}

std::string RtpExtension::ToString() const {
  std::stringstream ss;
  ss << "{uri: " << uri;
  ss << ", id: " << id;
  if (encrypt) {
    ss << ", encrypt";
  }
  ss << '}';
  return ss.str();
}

const char RtpExtension::kAudioLevelUri[] =
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
const int RtpExtension::kAudioLevelDefaultId = 1;

const char RtpExtension::kTimestampOffsetUri[] =
    "urn:ietf:params:rtp-hdrext:toffset";
const int RtpExtension::kTimestampOffsetDefaultId = 2;

const char RtpExtension::kAbsSendTimeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
const int RtpExtension::kAbsSendTimeDefaultId = 3;

const char RtpExtension::kVideoRotationUri[] = "urn:3gpp:video-orientation";
const int RtpExtension::kVideoRotationDefaultId = 4;

const char RtpExtension::kTransportSequenceNumberUri[] =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
const int RtpExtension::kTransportSequenceNumberDefaultId = 5;

// This extension allows applications to adaptively limit the playout delay
// on frames as per the current needs. For example, a gaming application
// has very different needs on end-to-end delay compared to a video-conference
// application.
const char RtpExtension::kPlayoutDelayUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";
const int RtpExtension::kPlayoutDelayDefaultId = 6;

const char RtpExtension::kVideoContentTypeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type";
const int RtpExtension::kVideoContentTypeDefaultId = 7;

const char RtpExtension::kVideoTimingUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/video-timing";
const int RtpExtension::kVideoTimingDefaultId = 8;

const char RtpExtension::kEncryptHeaderExtensionsUri[] =
    "urn:ietf:params:rtp-hdrext:encrypt";

const int RtpExtension::kMinId = 1;
const int RtpExtension::kMaxId = 14;

bool RtpExtension::IsSupportedForAudio(const std::string& uri) {
  return uri == webrtc::RtpExtension::kAudioLevelUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri;
}

bool RtpExtension::IsSupportedForVideo(const std::string& uri) {
  return uri == webrtc::RtpExtension::kTimestampOffsetUri ||
         uri == webrtc::RtpExtension::kAbsSendTimeUri ||
         uri == webrtc::RtpExtension::kVideoRotationUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kPlayoutDelayUri ||
         uri == webrtc::RtpExtension::kVideoContentTypeUri ||
         uri == webrtc::RtpExtension::kVideoTimingUri;
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
         uri == webrtc::RtpExtension::kVideoRotationUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kPlayoutDelayUri ||
         uri == webrtc::RtpExtension::kVideoContentTypeUri;
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
    if (std::find_if(extension + 1, extensions.end(),
                     [extension](const RtpExtension& check) {
                       return extension->uri == check.uri;
                     }) != extensions.end()) {
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
