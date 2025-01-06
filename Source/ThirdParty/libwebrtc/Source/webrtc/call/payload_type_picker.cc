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
#include <set>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/audio_codecs/audio_format.h"
#include "api/rtc_error.h"
#include "call/payload_type.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/media_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// Due to interoperability issues with old Chrome/WebRTC versions that
// ignore the [35, 63] range prefer the lower range for new codecs.
static const int kFirstDynamicPayloadTypeLowerRange = 35;
static const int kLastDynamicPayloadTypeLowerRange = 63;

static const int kFirstDynamicPayloadTypeUpperRange = 96;
static const int kLastDynamicPayloadTypeUpperRange = 127;

// Note: The only fields we need from a Codec are the type (audio/video),
// the subtype (vp8/h264/....), the clock rate, the channel count, and the
// fmtp parameters. The use of cricket::Codec, which contains more fields,
// is only a temporary measure.

struct MapTableEntry {
  webrtc::SdpAudioFormat format;
  int payload_type;
};

RTCErrorOr<PayloadType> FindFreePayloadType(std::set<PayloadType> seen_pt) {
  for (auto i = kFirstDynamicPayloadTypeUpperRange;
       i < kLastDynamicPayloadTypeUpperRange; i++) {
    if (seen_pt.count(PayloadType(i)) == 0) {
      return PayloadType(i);
    }
  }
  for (auto i = kFirstDynamicPayloadTypeLowerRange;
       i < kLastDynamicPayloadTypeLowerRange; i++) {
    if (seen_pt.count(PayloadType(i)) == 0) {
      return PayloadType(i);
    }
  }
  return RTCError(RTCErrorType::RESOURCE_EXHAUSTED,
                  "All available dynamic PTs have been assigned");
}

}  // namespace

PayloadTypePicker::PayloadTypePicker() {
  // Default audio codecs. Duplicates media/engine/payload_type_mapper.cc
  const MapTableEntry default_audio_mappings[] = {
      // Static payload type assignments according to RFC 3551.
      {{cricket::kPcmuCodecName, 8000, 1}, 0},
      {{"GSM", 8000, 1}, 3},
      {{"G723", 8000, 1}, 4},
      {{"DVI4", 8000, 1}, 5},
      {{"DVI4", 16000, 1}, 6},
      {{"LPC", 8000, 1}, 7},
      {{cricket::kPcmaCodecName, 8000, 1}, 8},
      {{cricket::kG722CodecName, 8000, 1}, 9},
      {{cricket::kL16CodecName, 44100, 2}, 10},
      {{cricket::kL16CodecName, 44100, 1}, 11},
      {{"QCELP", 8000, 1}, 12},
      {{cricket::kCnCodecName, 8000, 1}, 13},
      // RFC 4566 is a bit ambiguous on the contents of the "encoding
      // parameters" field, which, for audio, encodes the number of
      // channels. It is "optional and may be omitted if the number of
      // channels is one". Does that necessarily imply that an omitted
      // encoding parameter means one channel?  Since RFC 3551 doesn't
      // specify a value for this parameter for MPA, I've included both 0
      // and 1 here, to increase the chances it will be correctly used if
      // someone implements an MPEG audio encoder/decoder.
      {{"MPA", 90000, 0}, 14},
      {{"MPA", 90000, 1}, 14},
      {{"G728", 8000, 1}, 15},
      {{"DVI4", 11025, 1}, 16},
      {{"DVI4", 22050, 1}, 17},
      {{"G729", 8000, 1}, 18},

      // Payload type assignments currently used by WebRTC.
      // Includes data to reduce collisions (and thus reassignments)
      {{cricket::kIlbcCodecName, 8000, 1}, 102},
      {{cricket::kCnCodecName, 16000, 1}, 105},
      {{cricket::kCnCodecName, 32000, 1}, 106},
      {{cricket::kOpusCodecName,
        48000,
        2,
        {{cricket::kCodecParamMinPTime, "10"},
         {cricket::kCodecParamUseInbandFec, cricket::kParamValueTrue}}},
       111},
      // RED for opus is assigned in the lower range, starting at the top.
      // Note that the FMTP refers to the opus payload type.
      {{cricket::kRedCodecName,
        48000,
        2,
        {{cricket::kCodecParamNotInNameValueFormat, "111/111"}}},
       63},
      // TODO(solenberg): Remove the hard coded 16k,32k,48k DTMF once we
      // assign payload types dynamically for send side as well.
      {{cricket::kDtmfCodecName, 48000, 1}, 110},
      {{cricket::kDtmfCodecName, 32000, 1}, 112},
      {{cricket::kDtmfCodecName, 16000, 1}, 113},
      {{cricket::kDtmfCodecName, 8000, 1}, 126}};
  for (auto entry : default_audio_mappings) {
    AddMapping(PayloadType(entry.payload_type),
               cricket::CreateAudioCodec(entry.format));
  }
}

RTCErrorOr<PayloadType> PayloadTypePicker::SuggestMapping(
    cricket::Codec codec,
    const PayloadTypeRecorder* excluder) {
  // Test compatibility: If the codec contains a PT, and it is free, use it.
  // This saves having to rewrite tests that set the codec ID themselves.
  // Codecs with unassigned IDs should have -1 as their id.
  if (codec.id >= 0 && codec.id <= kLastDynamicPayloadTypeUpperRange &&
      seen_payload_types_.count(PayloadType(codec.id)) == 0) {
    AddMapping(PayloadType(codec.id), codec);
    return PayloadType(codec.id);
  }
  // The first matching entry is returned, unless excluder
  // maps it to something different.
  for (auto entry : entries_) {
    if (MatchesWithCodecRules(entry.codec(), codec)) {
      if (excluder) {
        auto result = excluder->LookupCodec(entry.payload_type());
        if (result.ok() && !MatchesWithCodecRules(result.value(), codec)) {
          continue;
        }
      }
      return entry.payload_type();
    }
  }
  // Assign the first free payload type.
  RTCErrorOr<PayloadType> found_pt = FindFreePayloadType(seen_payload_types_);
  if (found_pt.ok()) {
    AddMapping(found_pt.value(), codec);
  }
  return found_pt;
}

RTCError PayloadTypePicker::AddMapping(PayloadType payload_type,
                                       cricket::Codec codec) {
  // Completely duplicate mappings are ignored.
  // Multiple mappings for the same codec and the same PT are legal;
  for (auto entry : entries_) {
    if (payload_type == entry.payload_type() &&
        MatchesWithCodecRules(codec, entry.codec())) {
      return RTCError::OK();
    }
  }
  entries_.emplace_back(MapEntry(payload_type, codec));
  seen_payload_types_.emplace(payload_type);
  return RTCError::OK();
}

RTCError PayloadTypeRecorder::AddMapping(PayloadType payload_type,
                                         cricket::Codec codec) {
  auto existing_codec_it = payload_type_to_codec_.find(payload_type);
  if (existing_codec_it != payload_type_to_codec_.end() &&
      !MatchesWithCodecRules(codec, existing_codec_it->second)) {
    // Redefinition attempted.
    if (disallow_redefinition_level_ > 0) {
      if (accepted_definitions_.count(payload_type) > 0) {
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

std::vector<std::pair<PayloadType, cricket::Codec>>
PayloadTypeRecorder::GetMappings() const {
  return std::vector<std::pair<PayloadType, cricket::Codec>>{};
}

RTCErrorOr<PayloadType> PayloadTypeRecorder::LookupPayloadType(
    cricket::Codec codec) const {
  // Note that having multiple PTs mapping to the same codec is NOT an error.
  // In this case, we return the first found (not deterministic).
  auto result =
      std::find_if(payload_type_to_codec_.begin(), payload_type_to_codec_.end(),
                   [codec](const auto& iter) {
                     return MatchesWithCodecRules(iter.second, codec);
                   });
  if (result == payload_type_to_codec_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "No payload type found for codec");
  }
  return result->first;
}

RTCErrorOr<cricket::Codec> PayloadTypeRecorder::LookupCodec(
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

}  // namespace webrtc
