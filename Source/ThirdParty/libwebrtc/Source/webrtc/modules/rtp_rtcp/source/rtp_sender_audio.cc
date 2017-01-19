/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender_audio.h"

#include <string.h>

#include <memory>
#include <utility>

#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

static const int kDtmfFrequencyHz = 8000;

RTPSenderAudio::RTPSenderAudio(Clock* clock, RTPSender* rtp_sender)
    : clock_(clock),
      rtp_sender_(rtp_sender),
      packet_size_samples_(160),
      dtmf_event_is_on_(false),
      dtmf_event_first_packet_sent_(false),
      dtmf_payload_type_(-1),
      dtmf_timestamp_(0),
      dtmf_key_(0),
      dtmf_length_samples_(0),
      dtmf_level_(0),
      dtmf_time_last_sent_(0),
      dtmf_timestamp_last_sent_(0),
      inband_vad_active_(false),
      cngnb_payload_type_(-1),
      cngwb_payload_type_(-1),
      cngswb_payload_type_(-1),
      cngfb_payload_type_(-1),
      last_payload_type_(-1),
      audio_level_dbov_(0) {}

RTPSenderAudio::~RTPSenderAudio() {}

// set audio packet size, used to determine when it's time to send a DTMF packet
// in silence (CNG)
int32_t RTPSenderAudio::SetAudioPacketSize(uint16_t packet_size_samples) {
  rtc::CritScope cs(&send_audio_critsect_);
  packet_size_samples_ = packet_size_samples;
  return 0;
}

int32_t RTPSenderAudio::RegisterAudioPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payload_type,
    const uint32_t frequency,
    const size_t channels,
    const uint32_t rate,
    RtpUtility::Payload** payload) {
  if (RtpUtility::StringCompare(payloadName, "cn", 2)) {
    rtc::CritScope cs(&send_audio_critsect_);
    //  we can have multiple CNG payload types
    switch (frequency) {
      case 8000:
        cngnb_payload_type_ = payload_type;
        break;
      case 16000:
        cngwb_payload_type_ = payload_type;
        break;
      case 32000:
        cngswb_payload_type_ = payload_type;
        break;
      case 48000:
        cngfb_payload_type_ = payload_type;
        break;
      default:
        return -1;
    }
  } else if (RtpUtility::StringCompare(payloadName, "telephone-event", 15)) {
    rtc::CritScope cs(&send_audio_critsect_);
    // Don't add it to the list
    // we dont want to allow send with a DTMF payloadtype
    dtmf_payload_type_ = payload_type;
    return 0;
    // The default timestamp rate is 8000 Hz, but other rates may be defined.
  }
  *payload = new RtpUtility::Payload;
  (*payload)->typeSpecific.Audio.frequency = frequency;
  (*payload)->typeSpecific.Audio.channels = channels;
  (*payload)->typeSpecific.Audio.rate = rate;
  (*payload)->audio = true;
  (*payload)->name[RTP_PAYLOAD_NAME_SIZE - 1] = '\0';
  strncpy((*payload)->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  return 0;
}

bool RTPSenderAudio::MarkerBit(FrameType frame_type, int8_t payload_type) {
  rtc::CritScope cs(&send_audio_critsect_);
  // for audio true for first packet in a speech burst
  bool marker_bit = false;
  if (last_payload_type_ != payload_type) {
    if (payload_type != -1 && (cngnb_payload_type_ == payload_type ||
                               cngwb_payload_type_ == payload_type ||
                               cngswb_payload_type_ == payload_type ||
                               cngfb_payload_type_ == payload_type)) {
      // Only set a marker bit when we change payload type to a non CNG
      return false;
    }

    // payload_type differ
    if (last_payload_type_ == -1) {
      if (frame_type != kAudioFrameCN) {
        // first packet and NOT CNG
        return true;
      } else {
        // first packet and CNG
        inband_vad_active_ = true;
        return false;
      }
    }

    // not first packet AND
    // not CNG AND
    // payload_type changed

    // set a marker bit when we change payload type
    marker_bit = true;
  }

  // For G.723 G.729, AMR etc we can have inband VAD
  if (frame_type == kAudioFrameCN) {
    inband_vad_active_ = true;
  } else if (inband_vad_active_) {
    inband_vad_active_ = false;
    marker_bit = true;
  }
  return marker_bit;
}

bool RTPSenderAudio::SendAudio(FrameType frame_type,
                               int8_t payload_type,
                               uint32_t rtp_timestamp,
                               const uint8_t* payload_data,
                               size_t payload_size,
                               const RTPFragmentationHeader* fragmentation) {
  // TODO(pwestin) Breakup function in smaller functions.
  uint16_t dtmf_length_ms = 0;
  uint8_t key = 0;
  uint8_t audio_level_dbov;
  int8_t dtmf_payload_type;
  uint16_t packet_size_samples;
  {
    rtc::CritScope cs(&send_audio_critsect_);
    audio_level_dbov = audio_level_dbov_;
    dtmf_payload_type = dtmf_payload_type_;
    packet_size_samples = packet_size_samples_;
  }

  // Check if we have pending DTMFs to send
  if (!dtmf_event_is_on_ && dtmf_queue_.PendingDTMF()) {
    int64_t delaySinceLastDTMF =
        clock_->TimeInMilliseconds() - dtmf_time_last_sent_;

    if (delaySinceLastDTMF > 100) {
      // New tone to play
      dtmf_timestamp_ = rtp_timestamp;
      if (dtmf_queue_.NextDTMF(&key, &dtmf_length_ms, &dtmf_level_) >= 0) {
        dtmf_event_first_packet_sent_ = false;
        dtmf_key_ = key;
        dtmf_length_samples_ = (kDtmfFrequencyHz / 1000) * dtmf_length_ms;
        dtmf_event_is_on_ = true;
      }
    }
  }

  // A source MAY send events and coded audio packets for the same time
  // but we don't support it
  if (dtmf_event_is_on_) {
    if (frame_type == kEmptyFrame) {
      // kEmptyFrame is used to drive the DTMF when in CN mode
      // it can be triggered more frequently than we want to send the
      // DTMF packets.
      if (packet_size_samples > (rtp_timestamp - dtmf_timestamp_last_sent_)) {
        // not time to send yet
        return true;
      }
    }
    dtmf_timestamp_last_sent_ = rtp_timestamp;
    uint32_t dtmf_duration_samples = rtp_timestamp - dtmf_timestamp_;
    bool ended = false;
    bool send = true;

    if (dtmf_length_samples_ > dtmf_duration_samples) {
      if (dtmf_duration_samples <= 0) {
        // Skip send packet at start, since we shouldn't use duration 0
        send = false;
      }
    } else {
      ended = true;
      dtmf_event_is_on_ = false;
      dtmf_time_last_sent_ = clock_->TimeInMilliseconds();
    }
    if (send) {
      if (dtmf_duration_samples > 0xffff) {
        // RFC 4733 2.5.2.3 Long-Duration Events
        SendTelephoneEventPacket(ended, dtmf_payload_type, dtmf_timestamp_,
                                 static_cast<uint16_t>(0xffff), false);

        // set new timestap for this segment
        dtmf_timestamp_ = rtp_timestamp;
        dtmf_duration_samples -= 0xffff;
        dtmf_length_samples_ -= 0xffff;

        return SendTelephoneEventPacket(
            ended, dtmf_payload_type, dtmf_timestamp_,
            static_cast<uint16_t>(dtmf_duration_samples), false);
      } else {
        if (!SendTelephoneEventPacket(ended, dtmf_payload_type, dtmf_timestamp_,
                                      dtmf_duration_samples,
                                      !dtmf_event_first_packet_sent_)) {
          return false;
        }
        dtmf_event_first_packet_sent_ = true;
        return true;
      }
    }
    return true;
  }
  if (payload_size == 0 || payload_data == NULL) {
    if (frame_type == kEmptyFrame) {
      // we don't send empty audio RTP packets
      // no error since we use it to drive DTMF when we use VAD
      return true;
    }
    return false;
  }
  std::unique_ptr<RtpPacketToSend> packet = rtp_sender_->AllocatePacket();
  packet->SetMarker(MarkerBit(frame_type, payload_type));
  packet->SetPayloadType(payload_type);
  packet->SetTimestamp(rtp_timestamp);
  packet->set_capture_time_ms(clock_->TimeInMilliseconds());
  // Update audio level extension, if included.
  packet->SetExtension<AudioLevel>(frame_type == kAudioFrameSpeech,
                                   audio_level_dbov);

  if (fragmentation && fragmentation->fragmentationVectorSize > 0) {
    // Use the fragment info if we have one.
    uint8_t* payload =
        packet->AllocatePayload(1 + fragmentation->fragmentationLength[0]);
    if (!payload)  // Too large payload buffer.
      return false;
    payload[0] = fragmentation->fragmentationPlType[0];
    memcpy(payload + 1, payload_data + fragmentation->fragmentationOffset[0],
           fragmentation->fragmentationLength[0]);
  } else {
    uint8_t* payload = packet->AllocatePayload(payload_size);
    if (!payload)  // Too large payload buffer.
      return false;
    memcpy(payload, payload_data, payload_size);
  }

  if (!rtp_sender_->AssignSequenceNumber(packet.get()))
    return false;

  {
    rtc::CritScope cs(&send_audio_critsect_);
    last_payload_type_ = payload_type;
  }
  TRACE_EVENT_ASYNC_END2("webrtc", "Audio", rtp_timestamp, "timestamp",
                         packet->Timestamp(), "seqnum",
                         packet->SequenceNumber());
  bool send_result = rtp_sender_->SendToNetwork(
      std::move(packet), kAllowRetransmission, RtpPacketSender::kHighPriority);
  if (first_packet_sent_()) {
    LOG(LS_INFO) << "First audio RTP packet sent to pacer";
  }
  return send_result;
}

// Audio level magnitude and voice activity flag are set for each RTP packet
int32_t RTPSenderAudio::SetAudioLevel(uint8_t level_dbov) {
  if (level_dbov > 127) {
    return -1;
  }
  rtc::CritScope cs(&send_audio_critsect_);
  audio_level_dbov_ = level_dbov;
  return 0;
}

// Send a TelephoneEvent tone using RFC 2833 (4733)
int32_t RTPSenderAudio::SendTelephoneEvent(uint8_t key,
                                           uint16_t time_ms,
                                           uint8_t level) {
  {
    rtc::CritScope lock(&send_audio_critsect_);
    if (dtmf_payload_type_ < 0) {
      // TelephoneEvent payloadtype not configured
      return -1;
    }
  }
  return dtmf_queue_.AddDTMF(key, time_ms, level);
}

bool RTPSenderAudio::SendTelephoneEventPacket(bool ended,
                                              int8_t dtmf_payload_type,
                                              uint32_t dtmf_timestamp,
                                              uint16_t duration,
                                              bool marker_bit) {
  uint8_t send_count = 1;
  bool result = true;

  if (ended) {
    // resend last packet in an event 3 times
    send_count = 3;
  }
  do {
    // Send DTMF data.
    constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
    constexpr size_t kDtmfSize = 4;
    std::unique_ptr<RtpPacketToSend> packet(
        new RtpPacketToSend(kNoExtensions, kRtpHeaderSize + kDtmfSize));
    packet->SetPayloadType(dtmf_payload_type);
    packet->SetMarker(marker_bit);
    packet->SetSsrc(rtp_sender_->SSRC());
    packet->SetTimestamp(dtmf_timestamp);
    packet->set_capture_time_ms(clock_->TimeInMilliseconds());
    if (!rtp_sender_->AssignSequenceNumber(packet.get()))
      return false;

    // Create DTMF data.
    uint8_t* dtmfbuffer = packet->AllocatePayload(kDtmfSize);
    RTC_DCHECK(dtmfbuffer);
    /*    From RFC 2833:
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     event     |E|R| volume    |          duration             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    // R bit always cleared
    uint8_t R = 0x00;
    uint8_t volume = dtmf_level_;

    // First packet un-ended
    uint8_t E = ended ? 0x80 : 0x00;

    // First byte is Event number, equals key number
    dtmfbuffer[0] = dtmf_key_;
    dtmfbuffer[1] = E | R | volume;
    ByteWriter<uint16_t>::WriteBigEndian(dtmfbuffer + 2, duration);

    TRACE_EVENT_INSTANT2(
        TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "Audio::SendTelephoneEvent",
        "timestamp", packet->Timestamp(), "seqnum", packet->SequenceNumber());
    result = rtp_sender_->SendToNetwork(std::move(packet), kAllowRetransmission,
                                        RtpPacketSender::kHighPriority);
    send_count--;
  } while (send_count > 0 && result);

  return result;
}
}  // namespace webrtc
