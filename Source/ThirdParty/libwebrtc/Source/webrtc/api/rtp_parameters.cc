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
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtc_error.h"
#include "api/rtp_transceiver_direction.h"
#include "media/base/media_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {
constexpr char kSdpDelimiterSemicolon[] = ";";
constexpr char kSdpDelimiterEqualChar = '=';
constexpr char kSdpDelimiterEqual[] = "=";
constexpr char kSdpDelimiterSemicolonChar = ';';

void ParseFmtpParam(absl::string_view line,
                    std::string* parameter,
                    std::string* value) {
  if (!tokenize_first(line, kSdpDelimiterEqualChar, parameter, value)) {
    // Support for non-key-value lines like RFC 2198 or RFC 4733.
    *parameter = "";
    *value = std::string(line);
  }
  // a=fmtp:<payload_type> <param1>=<value1>; <param2>=<value2>; ...
}

bool IsFmtpParam(absl::string_view name) {
  // RFC 4855, section 3 specifies the mapping of media format parameters to SDP
  // parameters. Only ptime, maxptime, channels and rate are placed outside of
  // the fmtp line. In WebRTC, channels and rate are already handled separately
  // and thus not included in the CodecParameterMap.
  return name != kCodecParamPTime && name != kCodecParamMaxPTime;
}

void WriteFmtpParameter(absl::string_view parameter_name,
                        absl::string_view parameter_value,
                        StringBuilder& os) {
  if (parameter_name.empty()) {
    // RFC 2198 and RFC 4733 don't use key-value pairs.
    os << parameter_value;
  } else {
    // fmtp parameters: `parameter_name`=`parameter_value`
    os << parameter_name << kSdpDelimiterEqual << parameter_value;
  }
}

}  // namespace

const char* DegradationPreferenceToString(
    DegradationPreference degradation_preference) {
  switch (degradation_preference) {
    case DegradationPreference::MAINTAIN_FRAMERATE_AND_RESOLUTION:
      return "maintain-framerate-and-resolution";
    case DegradationPreference::MAINTAIN_FRAMERATE:
      return "maintain-framerate";
    case DegradationPreference::MAINTAIN_RESOLUTION:
      return "maintain-resolution";
    case DegradationPreference::BALANCED:
      return "balanced";
  }
  RTC_CHECK_NOTREACHED();
}

const double kDefaultBitratePriority = 1.0;

bool WriteFmtpParameters(const CodecParameterMap& parameters,
                         StringBuilder& os) {
  bool empty = true;
  const char* delimiter = "";  // No delimiter before first parameter.
  for (const auto& entry : parameters) {
    const std::string& key = entry.first;
    const std::string& value = entry.second;

    if (IsFmtpParam(key)) {
      os << delimiter;
      // A semicolon before each subsequent parameter.
      delimiter = kSdpDelimiterSemicolon;
      WriteFmtpParameter(key, value, os);
      empty = false;
    }
  }

  return !empty;
}

RTCError ParseFmtpParameterSet(absl::string_view line_params,
                               CodecParameterMap& codec_params) {
  // Parse out format specific parameters.
  for (absl::string_view param :
       split(line_params, kSdpDelimiterSemicolonChar)) {
    std::string name;
    std::string value;
    ParseFmtpParam(absl::StripAsciiWhitespace(param), &name, &value);
    if (codec_params.find(name) != codec_params.end()) {
      RTC_LOG(LS_INFO) << "Overwriting duplicate fmtp parameter with key \""
                       << name << "\".";
    }
    codec_params[name] = value;
  }
  return RTCError::OK();
}

RtcpFeedback::RtcpFeedback() = default;
RtcpFeedback::RtcpFeedback(RtcpFeedbackType type) : type(type) {}
RtcpFeedback::RtcpFeedback(RtcpFeedbackType type,
                           RtcpFeedbackMessageType message_type)
    : type(type), message_type(message_type) {}
RtcpFeedback::RtcpFeedback(const RtcpFeedback& rhs) = default;
RtcpFeedback::~RtcpFeedback() = default;

RtpCodec::RtpCodec() = default;
RtpCodec::RtpCodec(const RtpCodec&) = default;
RtpCodec::~RtpCodec() = default;
bool RtpCodec::IsResiliencyCodec() const {
  return name == kRtxCodecName || name == kRedCodecName ||
         name == kUlpfecCodecName || name == kFlexfecCodecName;
}
bool RtpCodec::IsMediaCodec() const {
  return !IsResiliencyCodec() && name != kComfortNoiseCodecName;
}
RtpCodecCapability::RtpCodecCapability() = default;
RtpCodecCapability::~RtpCodecCapability() = default;

RtpHeaderExtensionCapability::RtpHeaderExtensionCapability() = default;
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    absl::string_view uri)
    : uri(uri) {}
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    absl::string_view uri,
    int preferred_id)
    : uri(uri), preferred_id(preferred_id) {}
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    absl::string_view uri,
    int preferred_id,
    RtpTransceiverDirection direction)
    : uri(uri), preferred_id(preferred_id), direction(direction) {}
RtpHeaderExtensionCapability::RtpHeaderExtensionCapability(
    absl::string_view uri,
    int preferred_id,
    bool preferred_encrypt,
    RtpTransceiverDirection direction)
    : uri(uri),
      preferred_id(preferred_id),
      preferred_encrypt(preferred_encrypt),
      direction(direction) {}
RtpHeaderExtensionCapability::~RtpHeaderExtensionCapability() = default;

RtpExtension::RtpExtension() = default;
RtpExtension::RtpExtension(absl::string_view uri, int id) : uri(uri), id(id) {}
RtpExtension::RtpExtension(absl::string_view uri, int id, bool encrypt)
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
  SimpleStringBuilder sb(buf);
  sb << "{uri: " << uri;
  sb << ", id: " << id;
  if (encrypt) {
    sb << ", encrypt";
  }
  sb << '}';
  return sb.str();
}

bool RtpExtension::IsSupportedForAudio(absl::string_view uri) {
  return uri == RtpExtension::kAudioLevelUri ||
         uri == RtpExtension::kAbsSendTimeUri ||
         uri == RtpExtension::kAbsoluteCaptureTimeUri ||
         uri == RtpExtension::kTransportSequenceNumberUri ||
         uri == RtpExtension::kTransportSequenceNumberV2Uri ||
         uri == RtpExtension::kMidUri || uri == RtpExtension::kRidUri ||
         uri == RtpExtension::kRepairedRidUri;
}

bool RtpExtension::IsSupportedForVideo(absl::string_view uri) {
  return uri == RtpExtension::kTimestampOffsetUri ||
         uri == RtpExtension::kAbsSendTimeUri ||
         uri == RtpExtension::kAbsoluteCaptureTimeUri ||
         uri == RtpExtension::kVideoRotationUri ||
         uri == RtpExtension::kTransportSequenceNumberUri ||
         uri == RtpExtension::kTransportSequenceNumberV2Uri ||
         uri == RtpExtension::kPlayoutDelayUri ||
         uri == RtpExtension::kVideoContentTypeUri ||
         uri == RtpExtension::kVideoTimingUri || uri == RtpExtension::kMidUri ||
         uri == RtpExtension::kGenericFrameDescriptorUri00 ||
         uri == RtpExtension::kDependencyDescriptorUri ||
         uri == RtpExtension::kColorSpaceUri || uri == RtpExtension::kRidUri ||
         uri == RtpExtension::kRepairedRidUri ||
         uri == RtpExtension::kVideoLayersAllocationUri ||
         uri == RtpExtension::kVideoFrameTrackingIdUri ||
         uri == RtpExtension::kCorruptionDetectionUri;
}

bool RtpExtension::IsEncryptionSupported(absl::string_view uri) {
  return
#if defined(ENABLE_EXTERNAL_AUTH)
      // TODO(jbauch): Figure out a way to always allow "kAbsSendTimeUri"
      // here and filter out later if external auth is really used in
      // srtpfilter. External auth is used by Chromium and replaces the
      // extension header value of "kAbsSendTimeUri", so it must not be
      // encrypted (which can't be done by Chromium).
      uri != RtpExtension::kAbsSendTimeUri &&
#endif
      uri != RtpExtension::kEncryptHeaderExtensionsUri;
}

// Returns whether a header extension with the given URI exists.
// Note: This does not differentiate between encrypted and non-encrypted
// extensions, so use with care!
static bool HeaderExtensionWithUriExists(
    const std::vector<RtpExtension>& extensions,
    absl::string_view uri) {
  for (const auto& extension : extensions) {
    if (extension.uri == uri) {
      return true;
    }
  }
  return false;
}

const RtpExtension* RtpExtension::FindHeaderExtensionByUri(
    const std::vector<RtpExtension>& extensions,
    absl::string_view uri,
    Filter filter) {
  const RtpExtension* fallback_extension = nullptr;
  for (const auto& extension : extensions) {
    if (extension.uri != uri) {
      continue;
    }

    switch (filter) {
      case kDiscardEncryptedExtension:
        // We only accept an unencrypted extension.
        if (!extension.encrypt) {
          return &extension;
        }
        break;

      case kPreferEncryptedExtension:
        // We prefer an encrypted extension but we can fall back to an
        // unencrypted extension.
        if (extension.encrypt) {
          return &extension;
        } else {
          fallback_extension = &extension;
        }
        break;

      case kRequireEncryptedExtension:
        // We only accept an encrypted extension.
        if (extension.encrypt) {
          return &extension;
        }
        break;
    }
  }

  // Returning fallback extension (if any)
  return fallback_extension;
}

const RtpExtension* RtpExtension::FindHeaderExtensionByUriAndEncryption(
    const std::vector<RtpExtension>& extensions,
    absl::string_view uri,
    bool encrypt) {
  for (const auto& extension : extensions) {
    if (extension.uri == uri && extension.encrypt == encrypt) {
      return &extension;
    }
  }
  return nullptr;
}

const std::vector<RtpExtension> RtpExtension::DeduplicateHeaderExtensions(
    const std::vector<RtpExtension>& extensions,
    Filter filter) {
  std::vector<RtpExtension> filtered;

  // If we do not discard encrypted extensions, add them first
  if (filter != kDiscardEncryptedExtension) {
    for (const auto& extension : extensions) {
      if (!extension.encrypt) {
        continue;
      }
      if (!HeaderExtensionWithUriExists(filtered, extension.uri)) {
        filtered.push_back(extension);
      }
    }
  }

  // If we do not require encrypted extensions, add missing, non-encrypted
  // extensions.
  if (filter != kRequireEncryptedExtension) {
    for (const auto& extension : extensions) {
      if (extension.encrypt) {
        continue;
      }
      if (!HeaderExtensionWithUriExists(filtered, extension.uri)) {
        filtered.push_back(extension);
      }
    }
  }

  // Sort the returned vector to make comparisons of header extensions reliable.
  // In order of priority, we sort by uri first, then encrypt and id last.
  std::sort(filtered.begin(), filtered.end(),
            [](const RtpExtension& a, const RtpExtension& b) {
              return std::tie(a.uri, a.encrypt, a.id) <
                     std::tie(b.uri, b.encrypt, b.id);
            });

  return filtered;
}

bool RtpParameters::IsMixedCodec() const {
  std::optional<std::optional<RtpCodec>> first_codec;
  for (const RtpEncodingParameters& encoding : encodings) {
    if (!encoding.active) {
      continue;
    }
    if (!first_codec) {
      first_codec = encoding.codec;
      continue;
    }
    if (*first_codec != encoding.codec) {
      return true;
    }
  }
  return false;
}

}  // namespace webrtc
