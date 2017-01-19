/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"

#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {

RTPPayloadRegistry::RTPPayloadRegistry(RTPPayloadStrategy* rtp_payload_strategy)
    : rtp_payload_strategy_(rtp_payload_strategy),
      red_payload_type_(-1),
      ulpfec_payload_type_(-1),
      incoming_payload_type_(-1),
      last_received_payload_type_(-1),
      last_received_media_payload_type_(-1),
      rtx_(false),
      ssrc_rtx_(0) {}

RTPPayloadRegistry::~RTPPayloadRegistry() {
  while (!payload_type_map_.empty()) {
    RtpUtility::PayloadTypeMap::iterator it = payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
}

int32_t RTPPayloadRegistry::RegisterReceivePayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payload_type,
    const uint32_t frequency,
    const size_t channels,
    const uint32_t rate,
    bool* created_new_payload) {
  assert(payload_type >= 0);
  assert(payload_name);
  *created_new_payload = false;

  // Sanity check.
  switch (payload_type) {
    // Reserved payload types to avoid RTCP conflicts when marker bit is set.
    case 64:        //  192 Full INTRA-frame request.
    case 72:        //  200 Sender report.
    case 73:        //  201 Receiver report.
    case 74:        //  202 Source description.
    case 75:        //  203 Goodbye.
    case 76:        //  204 Application-defined.
    case 77:        //  205 Transport layer FB message.
    case 78:        //  206 Payload-specific FB message.
    case 79:        //  207 Extended report.
      LOG(LS_ERROR) << "Can't register invalid receiver payload type: "
                    << payload_type;
      return -1;
    default:
      break;
  }

  size_t payload_name_length = strlen(payload_name);

  rtc::CritScope cs(&crit_sect_);

  RtpUtility::PayloadTypeMap::iterator it =
      payload_type_map_.find(payload_type);

  if (it != payload_type_map_.end()) {
    // We already use this payload type.
    RtpUtility::Payload* payload = it->second;

    assert(payload);

    size_t name_length = strlen(payload->name);

    // Check if it's the same as we already have.
    // If same, ignore sending an error.
    if (payload_name_length == name_length &&
        RtpUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
      if (rtp_payload_strategy_->PayloadIsCompatible(*payload, frequency,
                                                     channels, rate)) {
        rtp_payload_strategy_->UpdatePayloadRate(payload, rate);
        return 0;
      }
    }
    LOG(LS_ERROR) << "Payload type already registered: "
                  << static_cast<int>(payload_type);
    return -1;
  }

  if (rtp_payload_strategy_->CodecsMustBeUnique()) {
    DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
        payload_name, payload_name_length, frequency, channels, rate);
  }

  RtpUtility::Payload* payload = rtp_payload_strategy_->CreatePayloadType(
      payload_name, payload_type, frequency, channels, rate);

  payload_type_map_[payload_type] = payload;
  *created_new_payload = true;

  if (RtpUtility::StringCompare(payload_name, "red", 3)) {
    red_payload_type_ = payload_type;
  } else if (RtpUtility::StringCompare(payload_name, "ulpfec", 6)) {
    ulpfec_payload_type_ = payload_type;
  }

  // Successful set of payload type, clear the value of last received payload
  // type since it might mean something else.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
  return 0;
}

int32_t RTPPayloadRegistry::DeRegisterReceivePayload(
    const int8_t payload_type) {
  rtc::CritScope cs(&crit_sect_);
  RtpUtility::PayloadTypeMap::iterator it =
      payload_type_map_.find(payload_type);
  assert(it != payload_type_map_.end());
  delete it->second;
  payload_type_map_.erase(it);
  return 0;
}

// There can't be several codecs with the same rate, frequency and channels
// for audio codecs, but there can for video.
// Always called from within a critical section.
void RTPPayloadRegistry::DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const size_t payload_name_length,
    const uint32_t frequency,
    const size_t channels,
    const uint32_t rate) {
  RtpUtility::PayloadTypeMap::iterator iterator = payload_type_map_.begin();
  for (; iterator != payload_type_map_.end(); ++iterator) {
    RtpUtility::Payload* payload = iterator->second;
    size_t name_length = strlen(payload->name);

    if (payload_name_length == name_length &&
        RtpUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
      // We found the payload name in the list.
      // If audio, check frequency and rate.
      if (payload->audio) {
        if (rtp_payload_strategy_->PayloadIsCompatible(*payload, frequency,
                                                       channels, rate)) {
          // Remove old setting.
          delete payload;
          payload_type_map_.erase(iterator);
          break;
        }
      } else if (RtpUtility::StringCompare(payload_name, "red", 3)) {
        delete payload;
        payload_type_map_.erase(iterator);
        break;
      }
    }
  }
}

int32_t RTPPayloadRegistry::ReceivePayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const uint32_t frequency,
    const size_t channels,
    const uint32_t rate,
    int8_t* payload_type) const {
  assert(payload_type);
  size_t payload_name_length = strlen(payload_name);

  rtc::CritScope cs(&crit_sect_);

  RtpUtility::PayloadTypeMap::const_iterator it = payload_type_map_.begin();

  for (; it != payload_type_map_.end(); ++it) {
    RtpUtility::Payload* payload = it->second;
    assert(payload);

    size_t name_length = strlen(payload->name);
    if (payload_name_length == name_length &&
        RtpUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
      // Name matches.
      if (payload->audio) {
        if (rate == 0) {
          // [default] audio, check freq and channels.
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels) {
            *payload_type = it->first;
            return 0;
          }
        } else {
          // Non-default audio, check freq, channels and rate.
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels &&
              payload->typeSpecific.Audio.rate == rate) {
            // extra rate condition added
            *payload_type = it->first;
            return 0;
          }
        }
      } else {
        // Video.
        *payload_type = it->first;
        return 0;
      }
    }
  }
  return -1;
}

bool RTPPayloadRegistry::RtxEnabled() const {
  rtc::CritScope cs(&crit_sect_);
  return rtx_;
}

bool RTPPayloadRegistry::IsRtx(const RTPHeader& header) const {
  rtc::CritScope cs(&crit_sect_);
  return IsRtxInternal(header);
}

bool RTPPayloadRegistry::IsRtxInternal(const RTPHeader& header) const {
  return rtx_ && ssrc_rtx_ == header.ssrc;
}

bool RTPPayloadRegistry::RestoreOriginalPacket(uint8_t* restored_packet,
                                               const uint8_t* packet,
                                               size_t* packet_length,
                                               uint32_t original_ssrc,
                                               const RTPHeader& header) {
  if (kRtxHeaderSize + header.headerLength + header.paddingLength >
      *packet_length) {
    return false;
  }
  const uint8_t* rtx_header = packet + header.headerLength;
  uint16_t original_sequence_number = (rtx_header[0] << 8) + rtx_header[1];

  // Copy the packet into the restored packet, except for the RTX header.
  memcpy(restored_packet, packet, header.headerLength);
  memcpy(restored_packet + header.headerLength,
         packet + header.headerLength + kRtxHeaderSize,
         *packet_length - header.headerLength - kRtxHeaderSize);
  *packet_length -= kRtxHeaderSize;

  // Replace the SSRC and the sequence number with the originals.
  ByteWriter<uint16_t>::WriteBigEndian(restored_packet + 2,
                                       original_sequence_number);
  ByteWriter<uint32_t>::WriteBigEndian(restored_packet + 8, original_ssrc);

  rtc::CritScope cs(&crit_sect_);
  if (!rtx_)
    return true;

  auto apt_mapping = rtx_payload_type_map_.find(header.payloadType);
  if (apt_mapping == rtx_payload_type_map_.end()) {
    // No associated payload type found. Warn, unless we have already done so.
    if (payload_types_with_suppressed_warnings_.find(header.payloadType) ==
        payload_types_with_suppressed_warnings_.end()) {
      LOG(LS_WARNING)
          << "No RTX associated payload type mapping was available; "
             "not able to restore original packet from RTX packet "
             "with payload type: "
          << static_cast<int>(header.payloadType) << ". "
          << "Suppressing further warnings for this payload type.";
      payload_types_with_suppressed_warnings_.insert(header.payloadType);
    }
    return false;
  }
  restored_packet[1] = static_cast<uint8_t>(apt_mapping->second);
  if (header.markerBit) {
    restored_packet[1] |= kRtpMarkerBitMask;  // Marker bit is set.
  }
  return true;
}

void RTPPayloadRegistry::SetRtxSsrc(uint32_t ssrc) {
  rtc::CritScope cs(&crit_sect_);
  ssrc_rtx_ = ssrc;
  rtx_ = true;
}

bool RTPPayloadRegistry::GetRtxSsrc(uint32_t* ssrc) const {
  rtc::CritScope cs(&crit_sect_);
  *ssrc = ssrc_rtx_;
  return rtx_;
}

void RTPPayloadRegistry::SetRtxPayloadType(int payload_type,
                                           int associated_payload_type) {
  rtc::CritScope cs(&crit_sect_);
  if (payload_type < 0) {
    LOG(LS_ERROR) << "Invalid RTX payload type: " << payload_type;
    return;
  }

  rtx_payload_type_map_[payload_type] = associated_payload_type;
  rtx_ = true;
}

bool RTPPayloadRegistry::IsRed(const RTPHeader& header) const {
  rtc::CritScope cs(&crit_sect_);
  return red_payload_type_ == header.payloadType;
}

bool RTPPayloadRegistry::IsEncapsulated(const RTPHeader& header) const {
  return IsRed(header) || IsRtx(header);
}

bool RTPPayloadRegistry::GetPayloadSpecifics(uint8_t payload_type,
                                             PayloadUnion* payload) const {
  rtc::CritScope cs(&crit_sect_);
  RtpUtility::PayloadTypeMap::const_iterator it =
      payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return false;
  }
  *payload = it->second->typeSpecific;
  return true;
}

int RTPPayloadRegistry::GetPayloadTypeFrequency(
    uint8_t payload_type) const {
  const RtpUtility::Payload* payload = PayloadTypeToPayload(payload_type);
  if (!payload) {
    return -1;
  }
  rtc::CritScope cs(&crit_sect_);
  return rtp_payload_strategy_->GetPayloadTypeFrequency(*payload);
}

const RtpUtility::Payload* RTPPayloadRegistry::PayloadTypeToPayload(
    uint8_t payload_type) const {
  rtc::CritScope cs(&crit_sect_);

  RtpUtility::PayloadTypeMap::const_iterator it =
      payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return nullptr;
  }

  return it->second;
}

void RTPPayloadRegistry::SetIncomingPayloadType(const RTPHeader& header) {
  rtc::CritScope cs(&crit_sect_);
  if (!IsRtxInternal(header))
    incoming_payload_type_ = header.payloadType;
}

bool RTPPayloadRegistry::ReportMediaPayloadType(uint8_t media_payload_type) {
  rtc::CritScope cs(&crit_sect_);
  if (last_received_media_payload_type_ == media_payload_type) {
    // Media type unchanged.
    return true;
  }
  last_received_media_payload_type_ = media_payload_type;
  return false;
}

class RTPPayloadAudioStrategy : public RTPPayloadStrategy {
 public:
  bool CodecsMustBeUnique() const override { return true; }

  bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                           const uint32_t frequency,
                           const size_t channels,
                           const uint32_t rate) const override {
    return
        payload.audio &&
        payload.typeSpecific.Audio.frequency == frequency &&
        payload.typeSpecific.Audio.channels == channels &&
        (payload.typeSpecific.Audio.rate == rate ||
            payload.typeSpecific.Audio.rate == 0 || rate == 0);
  }

  void UpdatePayloadRate(RtpUtility::Payload* payload,
                         const uint32_t rate) const override {
    payload->typeSpecific.Audio.rate = rate;
  }

  RtpUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const size_t channels,
      const uint32_t rate) const override {
    RtpUtility::Payload* payload = new RtpUtility::Payload;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    assert(frequency >= 1000);
    payload->typeSpecific.Audio.frequency = frequency;
    payload->typeSpecific.Audio.channels = channels;
    payload->typeSpecific.Audio.rate = rate;
    payload->audio = true;
    return payload;
  }

  int GetPayloadTypeFrequency(
      const RtpUtility::Payload& payload) const override {
    return payload.typeSpecific.Audio.frequency;
  }
};

class RTPPayloadVideoStrategy : public RTPPayloadStrategy {
 public:
  bool CodecsMustBeUnique() const override { return false; }

  bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                           const uint32_t frequency,
                           const size_t channels,
                           const uint32_t rate) const override {
    return !payload.audio;
  }

  void UpdatePayloadRate(RtpUtility::Payload* payload,
                         const uint32_t rate) const override {}

  RtpUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const size_t channels,
      const uint32_t rate) const override {
    RtpVideoCodecTypes videoType = kRtpVideoGeneric;

    if (RtpUtility::StringCompare(payloadName, "VP8", 3)) {
      videoType = kRtpVideoVp8;
    } else if (RtpUtility::StringCompare(payloadName, "VP9", 3)) {
      videoType = kRtpVideoVp9;
    } else if (RtpUtility::StringCompare(payloadName, "H264", 4)) {
      videoType = kRtpVideoH264;
    } else if (RtpUtility::StringCompare(payloadName, "I420", 4)) {
      videoType = kRtpVideoGeneric;
    } else if (RtpUtility::StringCompare(payloadName, "ULPFEC", 6) ||
        RtpUtility::StringCompare(payloadName, "RED", 3)) {
      videoType = kRtpVideoNone;
    } else {
      videoType = kRtpVideoGeneric;
    }
    RtpUtility::Payload* payload = new RtpUtility::Payload;

    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    payload->typeSpecific.Video.videoCodecType = videoType;
    payload->audio = false;
    return payload;
  }

  int GetPayloadTypeFrequency(
      const RtpUtility::Payload& payload) const override {
    return kVideoPayloadTypeFrequency;
  }
};

RTPPayloadStrategy* RTPPayloadStrategy::CreateStrategy(
    const bool handling_audio) {
  if (handling_audio) {
    return new RTPPayloadAudioStrategy();
  } else {
    return new RTPPayloadVideoStrategy();
  }
}

}  // namespace webrtc
