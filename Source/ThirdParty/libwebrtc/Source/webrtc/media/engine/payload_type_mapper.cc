/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/payload_type_mapper.h"

#include "webrtc/common_types.h"
#include "webrtc/media/base/mediaconstants.h"

namespace cricket {

PayloadTypeMapper::PayloadTypeMapper()
    // RFC 3551 reserves payload type numbers in the range 96-127 exclusively
    // for dynamic assignment. Once those are used up, it is recommended that
    // payload types unassigned by the RFC are used for dynamic payload type
    // mapping, before any static payload ids. At this point, we only support
    // mapping within the exclusive range.
    : next_unused_payload_type_(96),
      max_payload_type_(127),
      mappings_({
          // Static payload type assignments according to RFC 3551.
          {{"PCMU",   8000, 1}, 0},
          {{"GSM",    8000, 1}, 3},
          {{"G723",   8000, 1}, 4},
          {{"DVI4",   8000, 1}, 5},
          {{"DVI4",  16000, 1}, 6},
          {{"LPC",    8000, 1}, 7},
          {{"PCMA",   8000, 1}, 8},
          {{"G722",   8000, 1}, 9},
          {{"L16",   44100, 2}, 10},
          {{"L16",   44100, 1}, 11},
          {{"QCELP",  8000, 1}, 12},
          {{"CN",     8000, 1}, 13},
          // RFC 4566 is a bit ambiguous on the contents of the "encoding
          // parameters" field, which, for audio, encodes the number of
          // channels. It is "optional and may be omitted if the number of
          // channels is one". Does that necessarily imply that an omitted
          // encoding parameter means one channel?  Since RFC 3551 doesn't
          // specify a value for this parameter for MPA, I've included both 0
          // and 1 here, to increase the chances it will be correctly used if
          // someone implements an MPEG audio encoder/decoder.
          {{"MPA",   90000, 0}, 14},
          {{"MPA",   90000, 1}, 14},
          {{"G728",   8000, 1}, 15},
          {{"DVI4",  11025, 1}, 16},
          {{"DVI4",  22050, 1}, 17},
          {{"G729",   8000, 1}, 18},

          // Payload type assignments currently used by WebRTC.
          // Includes video, to reduce collisions (and thus reassignments)
          // RTX codecs mapping to specific video payload types
          {{kRtxCodecName,    90000, 0,
              {{kCodecParamAssociatedPayloadType,
                      std::to_string(kDefaultVp8PlType)}}},
                kDefaultRtxVp8PlType},
          {{kRtxCodecName,    90000, 0,
              {{kCodecParamAssociatedPayloadType,
                      std::to_string(kDefaultVp9PlType)}}},
                kDefaultRtxVp9PlType},
          {{kRtxCodecName,    90000, 0,
              {{kCodecParamAssociatedPayloadType,
                      std::to_string(kDefaultRedPlType)}}},
                kDefaultRtxRedPlType},
          {{kRtxCodecName,    90000, 0,
              {{kCodecParamAssociatedPayloadType,
                      std::to_string(kDefaultH264PlType)}}},
                kDefaultRtxH264ConstrainedBaselinePlType},
          // Other codecs
          {{kVp8CodecName,    90000, 0}, kDefaultVp8PlType},
          {{kVp9CodecName,    90000, 0}, kDefaultVp9PlType},
          {{kIlbcCodecName,    8000, 1}, 102},
          {{kIsacCodecName,   16000, 1}, 103},
          {{kIsacCodecName,   32000, 1}, 104},
          {{kCnCodecName,     16000, 1}, 105},
          {{kCnCodecName,     32000, 1}, 106},
          {{kH264CodecName,   90000, 0}, kDefaultH264PlType},
          {{kOpusCodecName,   48000, 2,
              {{"minptime", "10"}, {"useinbandfec", "1"}}}, 111},
          {{kRedCodecName,    90000, 0}, kDefaultRedPlType},
          {{kUlpfecCodecName, 90000, 0}, kDefaultUlpfecType},
          {{kFlexfecCodecName, 90000, 0}, kDefaultFlexfecPlType},
          {{kDtmfCodecName,    8000, 1}, 126}}) {
  // TODO(ossu): Try to keep this as change-proof as possible until we're able
  // to remove the payload type constants from everywhere in the code.
  for (const auto& mapping : mappings_) {
    used_payload_types_.insert(mapping.second);
  }
}

PayloadTypeMapper::~PayloadTypeMapper() = default;

rtc::Optional<int> PayloadTypeMapper::GetMappingFor(
    const webrtc::SdpAudioFormat& format) {
  auto iter = mappings_.find(format);
  if (iter != mappings_.end())
    return rtc::Optional<int>(iter->second);

  for (; next_unused_payload_type_ <= max_payload_type_;
       ++next_unused_payload_type_) {
    int payload_type = next_unused_payload_type_;
    if (used_payload_types_.find(payload_type) == used_payload_types_.end()) {
      used_payload_types_.insert(payload_type);
      mappings_[format] = payload_type;
      ++next_unused_payload_type_;
      return rtc::Optional<int>(payload_type);
    }
  }

  return rtc::Optional<int>();
}

rtc::Optional<int> PayloadTypeMapper::FindMappingFor(
    const webrtc::SdpAudioFormat& format) const {
  auto iter = mappings_.find(format);
  if (iter != mappings_.end())
    return rtc::Optional<int>(iter->second);

  return rtc::Optional<int>();
}

rtc::Optional<AudioCodec> PayloadTypeMapper::ToAudioCodec(
    const webrtc::SdpAudioFormat& format) {
  // TODO(ossu): We can safely set bitrate to zero here, since that field is
  // not presented in the SDP. It is used to ferry around some target bitrate
  // values for certain codecs (ISAC and Opus) and in ways it really
  // shouldn't. It should be removed once we no longer use CodecInsts in the
  // ACM or NetEq.
  auto opt_payload_type = GetMappingFor(format);
  if (opt_payload_type) {
    AudioCodec codec(*opt_payload_type, format.name, format.clockrate_hz, 0,
                     format.num_channels);
    codec.params = format.parameters;
    return rtc::Optional<AudioCodec>(std::move(codec));
  }

  return rtc::Optional<AudioCodec>();
}

bool PayloadTypeMapper::SdpAudioFormatOrdering::operator()(
    const webrtc::SdpAudioFormat& a,
    const webrtc::SdpAudioFormat& b) const {
  if (a.clockrate_hz == b.clockrate_hz) {
    if (a.num_channels == b.num_channels) {
      int name_cmp = STR_CASE_CMP(a.name.c_str(), b.name.c_str());
      if (name_cmp == 0)
        return a.parameters < b.parameters;
      return name_cmp < 0;
    }
    return a.num_channels < b.num_channels;
  }
  return a.clockrate_hz < b.clockrate_hz;
}

}  // namespace cricket
