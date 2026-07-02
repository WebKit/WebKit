/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/payload_type_picker.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "call/payload_type.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/media_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// Due to interoperability issues with old Chrome/WebRTC versions that
// ignore the [35, 63] range prefer the lower range for new codecs.
const int kFirstDynamicPayloadTypeLowerRange = 35;
const int kLastDynamicPayloadTypeLowerRange = 63;

const int kFirstDynamicPayloadTypeUpperRange = 96;
const int kLastDynamicPayloadTypeUpperRange = 127;

// Note: The only fields we need from a Codec are the type (audio/video),
// the subtype (vp8/h264/....), the clock rate, the channel count, and the
// fmtp parameters. The use of Codec, which contains more fields,
// is only a temporary measure.

struct MapTableEntry {
  SdpAudioFormat format;
  int payload_type;
};

// Helper function to determine whether a codec should use the [35, 63] range.
// Should be used when adding new codecs (or variants).
bool CodecPrefersLowerRange(const Codec& codec) {
  // All audio codecs prefer upper range.
  if (codec.type == Codec::Type::kAudio) {
    return absl::EqualsIgnoreCase(codec.name, kRedCodecName);
  }
  if (absl::EqualsIgnoreCase(codec.name, kFlexfecCodecName) ||
      absl::EqualsIgnoreCase(codec.name, kAv1CodecName) ||
      absl::EqualsIgnoreCase(codec.name, kH265CodecName)) {
    return true;
  } else if (absl::EqualsIgnoreCase(codec.name, kH264CodecName)) {
    std::string profile_level_id;
    std::string packetization_mode;

    if (codec.GetParam(kH264FmtpProfileLevelId, &profile_level_id)) {
      if (absl::StartsWithIgnoreCase(profile_level_id, "4d00")) {
        if (codec.GetParam(kH264FmtpPacketizationMode, &packetization_mode)) {
          return packetization_mode == "0";
        }
      }
      // H264 with YUV444.
      return absl::StartsWithIgnoreCase(profile_level_id, "f400");
    }
  } else if (absl::EqualsIgnoreCase(codec.name, kVp9CodecName)) {
    std::string profile_id;

    if (codec.GetParam(kVP9ProfileId, &profile_id)) {
      if (profile_id == "1" || profile_id == "3") {
        return true;
      }
    }
  } else if (absl::EqualsIgnoreCase(codec.name, kRtxCodecName)) {
    // For RTX prefer lower range if the associated codec is in that range.
    std::string associated_pt_str;
    int associated_pt;
    return codec.GetParam(kCodecParamAssociatedPayloadType,
                          &associated_pt_str) &&
           FromString(associated_pt_str, &associated_pt) &&
           associated_pt >= kFirstDynamicPayloadTypeLowerRange &&
           associated_pt <= kLastDynamicPayloadTypeLowerRange;
  }
  return false;
}

RTCErrorOr<PayloadType> FindFreePayloadType(
    const Codec& codec,
    const flat_set<PayloadType>& seen_pt,
    bool pick_from_top_of_range) {
  // Prefer to use lower range for codecs that can handle it.
  bool prefer_lower_range = CodecPrefersLowerRange(codec);

  struct Range {
    int first;
    int last;
  };
  const Range lower = {kFirstDynamicPayloadTypeLowerRange,
                       kLastDynamicPayloadTypeLowerRange};
  const Range upper = {kFirstDynamicPayloadTypeUpperRange,
                       kLastDynamicPayloadTypeUpperRange};

  const auto search_order = (pick_from_top_of_range || !prefer_lower_range)
                                ? std::array{upper, lower}
                                : std::array{lower, upper};

  for (const auto& range : search_order) {
    if (pick_from_top_of_range) {
      for (auto i = range.last; i >= range.first; --i) {
        if (!seen_pt.contains(PayloadType(i))) {
          return PayloadType(i);
        }
      }
    } else {
      for (auto i = range.first; i <= range.last; ++i) {
        if (!seen_pt.contains(PayloadType(i))) {
          return PayloadType(i);
        }
      }
    }
  }

  if (prefer_lower_range) {
    return RTCError(RTCErrorType::RESOURCE_EXHAUSTED,
                    "All available dynamic PTs have been assigned");
  } else {
    return RTCError(
        RTCErrorType::RESOURCE_EXHAUSTED,
        "All available dynamic PTs have been assigned, codec preferred upper");
  }
}

}  // namespace

PayloadTypePicker::PayloadTypePicker() {
  // Default audio codecs. Duplicates media/engine/payload_type_mapper.cc
  const MapTableEntry default_audio_mappings[] = {
      // Static payload type assignments according to RFC 3551.
      {.format = {kPcmuCodecName, 8000, 1}, .payload_type = 0},
      {.format = {"GSM", 8000, 1}, .payload_type = 3},
      {.format = {"G723", 8000, 1}, .payload_type = 4},
      {.format = {"DVI4", 8000, 1}, .payload_type = 5},
      {.format = {"DVI4", 16000, 1}, .payload_type = 6},
      {.format = {"LPC", 8000, 1}, .payload_type = 7},
      {.format = {kPcmaCodecName, 8000, 1}, .payload_type = 8},
      {.format = {kG722CodecName, 8000, 1}, .payload_type = 9},
      {.format = {kL16CodecName, 44100, 2}, .payload_type = 10},
      {.format = {kL16CodecName, 44100, 1}, .payload_type = 11},
      {.format = {"QCELP", 8000, 1}, .payload_type = 12},
      {.format = {kCnCodecName, 8000, 1}, .payload_type = 13},
      // RFC 4566 is a bit ambiguous on the contents of the "encoding
      // parameters" field, which, for audio, encodes the number of
      // channels. It is "optional and may be omitted if the number of
      // channels is one". Does that necessarily imply that an omitted
      // encoding parameter means one channel?  Since RFC 3551 doesn't
      // specify a value for this parameter for MPA, I've included both 0
      // and 1 here, to increase the chances it will be correctly used if
      // someone implements an MPEG audio encoder/decoder.
      {.format = {"MPA", 90000, 0}, .payload_type = 14},
      {.format = {"MPA", 90000, 1}, .payload_type = 14},
      {.format = {"G728", 8000, 1}, .payload_type = 15},
      {.format = {"DVI4", 11025, 1}, .payload_type = 16},
      {.format = {"DVI4", 22050, 1}, .payload_type = 17},
      {.format = {"G729", 8000, 1}, .payload_type = 18},

      // Payload type assignments currently used by WebRTC.
      // Includes data to reduce collisions (and thus reassignments)
      // TODO(bugs.webrtc.org/400630582): Delete this, it's only for test
      // stability.
      {.format = {"reserved-do-not-use", 1, 0}, .payload_type = 102},
      {.format = {kCnCodecName, 16000, 1}, .payload_type = 105},
      {.format = {kCnCodecName, 32000, 1}, .payload_type = 106},
      {.format = {kOpusCodecName,
                  48000,
                  2,
                  {{kCodecParamMinPTime, "10"},
                   {kCodecParamUseInbandFec, kParamValueTrue}}},
       .payload_type = 111},
      // RED for opus is assigned in the lower range, starting at the top.
      // Note that the FMTP refers to the opus payload type.
      {.format = {kRedCodecName,
                  48000,
                  2,
                  {{kCodecParamNotInNameValueFormat, "111/111"}}},
       .payload_type = 63},
      // TODO(solenberg): Remove the hard coded 16k,32k,48k DTMF once we
      // assign payload types dynamically for send side as well.
      {.format = {kDtmfCodecName, 48000, 1}, .payload_type = 110},
      {.format = {kDtmfCodecName, 32000, 1}, .payload_type = 112},
      {.format = {kDtmfCodecName, 16000, 1}, .payload_type = 113},
      {.format = {kDtmfCodecName, 8000, 1}, .payload_type = 126}};
  for (const MapTableEntry& entry : default_audio_mappings) {
    AddMapping(PayloadType(entry.payload_type), CreateAudioCodec(entry.format));
  }
}

RTCErrorOr<PayloadType> PayloadTypePicker::SuggestMapping(
    Codec codec,
    const PayloadTypeRecorder* excluder,
    bool pick_from_top_of_range) {
  // Test compatibility: If the codec contains a PT, and it is free and valid,
  // use it. This saves having to rewrite tests that set the codec ID
  // themselves. Unassigned IDs will have id.IsSet() = false.
  if (codec.id.IsSet() && codec.id.IsDynamic() &&
      !seen_payload_types_.contains(codec.id)) {
    AddMapping(PayloadType(codec.id), codec);
    return PayloadType(codec.id);
  }
  // The first matching entry is returned, unless excluder
  // maps it to something different.
  auto relaxed_comparator = [](PayloadType a, PayloadType b) {
    return a == b || a == PayloadType::NotSet() || b == PayloadType::NotSet();
  };
  for (const MapEntry& entry : entries_) {
    if (MatchesWithReferenceAttributesAndComparator(entry.codec(), codec,
                                                    relaxed_comparator)) {
      if (excluder) {
        auto result = excluder->LookupCodec(entry.payload_type());
        if (result.ok() && !MatchesWithReferenceAttributesAndComparator(
                               result.value(), codec, relaxed_comparator)) {
          continue;
        }
      }
      AddMapping(entry.payload_type(), codec);
      return entry.payload_type();
    }
  }
  // Assign the first free payload type.
  RTCErrorOr<PayloadType> found_pt =
      FindFreePayloadType(codec, seen_payload_types_, pick_from_top_of_range);
  if (found_pt.ok()) {
    AddMapping(found_pt.value(), codec);
  }
  return found_pt;
}

RTCError PayloadTypePicker::AddMapping(PayloadType payload_type, Codec codec) {
  // Completely duplicate mappings are ignored.
  // Multiple mappings for the same codec and the same PT are legal;
  for (const MapEntry& entry : entries_) {
    if (payload_type == entry.payload_type() &&
        MatchesWithReferenceAttributes(codec, entry.codec())) {
      return RTCError::OK();
    }
  }
  entries_.emplace_back(MapEntry(payload_type, codec));
  // Add the mapping to "seen" if it is not already present.
  seen_payload_types_.emplace(payload_type);
  return RTCError::OK();
}

std::optional<Codec> PayloadTypePicker::LookupCodec(
    PayloadType payload_type) const {
  std::optional<Codec> result;
  for (const auto& entry : entries_) {
    if (entry.payload_type() == payload_type) {
      // If there are multiple matches, the last one wins.
      result = entry.codec();
    }
  }
  return result;
}

RTCError PayloadTypeRecorder::AddMapping(PayloadType payload_type,
                                         Codec codec) {
  auto existing_codec_it = payload_type_to_codec_.find(payload_type);
  if (existing_codec_it != payload_type_to_codec_.end() &&
      !MatchesWithReferenceAttributes(codec, existing_codec_it->second)) {
    // Redefinition attempted.
    if (disallow_redefinition_level_ > 0) {
      if (accepted_definitions_.contains(payload_type)) {
        // We have already defined this PT in this scope.
        RTC_LOG(LS_WARNING)
            << "Rejected attempt to redefine mapping for PT " << payload_type
            << " from " << existing_codec_it->second << " to " << codec;
        return RTCError(RTCErrorType::INVALID_MODIFICATION,
                        "Attempt to redefine a codec mapping");
      }
    }
    if (absl::EqualsIgnoreCase(codec.name, existing_codec_it->second.name)) {
      // The difference is in clock rate, channels or FMTP parameters.
      RTC_LOG(LS_INFO) << "Warning: Attempt to change a codec's parameters";
      // Some FMTP value changes are harmless, others are harmful.
      // This is done in production today, so we can't return an error.
    } else {
      RTC_LOG(LS_WARNING) << "Warning: You attempted to redefine a codec from "
                          << existing_codec_it->second << " to "
                          << " new codec " << codec;
      // This is a spec violation.
      // TODO: https://issues.webrtc.org/41480892 - return an error.
    }
    // Accept redefinition.
    accepted_definitions_.emplace(payload_type);
    payload_type_to_codec_.insert_or_assign(payload_type, codec);
    return RTCError::OK();
  }
  accepted_definitions_.emplace(payload_type);
  payload_type_to_codec_.emplace(payload_type, codec);
  suggester_.AddMapping(payload_type, codec);
  return RTCError::OK();
}

std::vector<std::pair<PayloadType, Codec>> PayloadTypeRecorder::GetMappings()
    const {
  return std::vector<std::pair<PayloadType, Codec>>(
      payload_type_to_codec_.begin(), payload_type_to_codec_.end());
}

RTCErrorOr<PayloadType> PayloadTypeRecorder::LookupPayloadType(
    Codec codec) const {
  // Note that having multiple PTs mapping to the same codec is NOT an error.
  // In this case, we return the first found (not deterministic).
  auto result =
      std::find_if(payload_type_to_codec_.begin(), payload_type_to_codec_.end(),
                   [codec](const auto& iter) {
                     return MatchesWithReferenceAttributes(iter.second, codec);
                   });
  if (result == payload_type_to_codec_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "No payload type found for codec");
  }
  return result->first;
}

RTCErrorOr<Codec> PayloadTypeRecorder::LookupCodec(
    PayloadType payload_type) const {
  auto result = payload_type_to_codec_.find(payload_type);
  if (result == payload_type_to_codec_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER, "No such payload type");
  }
  return result->second;
}

void PayloadTypeRecorder::DisallowRedefinition() {
  if (disallow_redefinition_level_ == 0) {
    accepted_definitions_.clear();
  }
  ++disallow_redefinition_level_;
}

void PayloadTypeRecorder::ReallowRedefinition() {
  RTC_CHECK(disallow_redefinition_level_ > 0);
  --disallow_redefinition_level_;
}

void PayloadTypeRecorder::Commit() {
  checkpoint_payload_type_to_codec_ = payload_type_to_codec_;
}
void PayloadTypeRecorder::Rollback() {
  payload_type_to_codec_ = checkpoint_payload_type_to_codec_;
}

RTCError RtpHeaderExtensionRecorder::AddMapping(RtpHeaderExtensionId id,
                                                absl::string_view uri,
                                                bool encrypt) {
  auto it = uri_to_id_.find(std::pair{std::string(uri), encrypt});
  if (it != uri_to_id_.end()) {
    if (it->second != id) {
      RTC_HISTOGRAM_BOOLEAN(
          "WebRTC.PeerConnection.RtpHeaderExtensionRedefinition", true);
      // TODO: bugs.webrtc.org/504685269 - Enable error return by default.
      if (env_.field_trials().IsEnabled(
              "WebRTC-ErrorOnRtpExtensionRedefinition")) {
        return RTCError(RTCErrorType::INVALID_PARAMETER,
                        "Redefining mapping for RTP header extension");
      }
      RTC_LOG(LS_ERROR) << "RtpHeaderExtensionRecorder: Redefining mapping for "
                        << uri << " (encrypt=" << encrypt << ") from "
                        << it->second << " to " << id;
    }
  }
  uri_to_id_[{std::string(uri), encrypt}] = id;
  return RTCError::OK();
}

RTCErrorOr<RtpHeaderExtensionId> RtpHeaderExtensionRecorder::LookupId(
    absl::string_view uri,
    bool encrypt) const {
  auto it = uri_to_id_.find(std::pair{std::string(uri), encrypt});
  if (it == uri_to_id_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "No ID found for extension");
  }
  return it->second;
}

void RtpHeaderExtensionRecorder::Commit() {
  checkpoint_uri_to_id_ = uri_to_id_;
}

void RtpHeaderExtensionRecorder::Rollback() {
  uri_to_id_ = checkpoint_uri_to_id_;
}

RTCErrorOr<RtpHeaderExtensionId> RtpHeaderExtensionPicker::SuggestMapping(
    absl::string_view uri,
    bool encrypt,
    RtpHeaderExtensionId preferred_id,
    RtpTransceiverIdDomain id_domain,
    const RtpHeaderExtensionRecorder* excluder) {
  // If we already have a mapping for this (uri, encrypt), use it.
  for (const auto& entry : entries_) {
    if (entry.uri == uri && entry.encrypt == encrypt) {
      if (excluder) {
        auto result = excluder->LookupId(entry.uri, entry.encrypt);
        if (result.ok() && result.value() != entry.id) {
          continue;
        }
      }
      return entry.id;
    }
  }

  // Test compatibility: If preferred_id is provided and free, use it.
  if (preferred_id.Valid() && !seen_ids_.contains(preferred_id)) {
    if (preferred_id <= RtpHeaderExtensionId::kOneByteHeaderExtensionMaxId) {
      AddMapping(preferred_id, uri, encrypt);
      return preferred_id;
    }
    // We allow preferred_id >= 15 even if id_domain is kOneByteOnly because
    // it might be a re-negotiation or a test where the ID was explicitly
    // assigned. Automatic allocation below will still respect id_domain.
    if (preferred_id > RtpHeaderExtensionId::kOneByteHeaderExtensionMaxId) {
      AddMapping(preferred_id, uri, encrypt);
      return preferred_id;
    }
  }

  // Find a free ID.
  // One-byte range: 1-14.
  // We prefer to allocate from the top of the range (14 down to 1).
  for (RtpHeaderExtensionId id =
           RtpHeaderExtensionId::kOneByteHeaderExtensionMaxId;
       id >= RtpHeaderExtensionId::kMinId; id = id.value() - 1) {
    if (!seen_ids_.contains(id)) {
      AddMapping(id, uri, encrypt);
      return id;
    }
  }

  if (id_domain == RtpTransceiverIdDomain::kTwoByteAllowed) {
    // TODO: issues.webrtc.org/334925828 - add unit tests for this case.
    // Two-byte range: 16-255. (Avoid 15, which is special in RFC 8285)
    for (int id = 16; id <= 255; ++id) {
      if (!seen_ids_.contains(RtpHeaderExtensionId(id))) {
        AddMapping(RtpHeaderExtensionId(id), uri, encrypt);
        return RtpHeaderExtensionId(id);
      }
    }
  }

  return RTCError(RTCErrorType::RESOURCE_EXHAUSTED,
                  "No free RTP extension IDs");
}

RTCError RtpHeaderExtensionPicker::AddMapping(RtpHeaderExtensionId id,
                                              absl::string_view uri,
                                              bool encrypt) {
  RTC_DCHECK(id.Valid());
  // 15 is special and should be avoided, but allowed in the two-byte form
  // according to RFC 8285. But still, it's unexpected to see it used.
  if (id == RtpHeaderExtensionId(15)) {
    RTC_LOG(LS_WARNING) << "Use of special URI extension id 15 encountered.";
  }
  for (const auto& entry : entries_) {
    if (entry.id == id && entry.uri == uri && entry.encrypt == encrypt) {
      return RTCError::OK();
    }
  }
  entries_.push_back({std::string(uri), encrypt, id});
  seen_ids_.insert(id);
  return RTCError::OK();
}

}  // namespace webrtc
