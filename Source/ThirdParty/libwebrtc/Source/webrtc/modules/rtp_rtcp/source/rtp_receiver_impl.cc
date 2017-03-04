/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_impl.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/base/logging.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"

namespace webrtc {

using RtpUtility::Payload;

RtpReceiver* RtpReceiver::CreateVideoReceiver(
    Clock* clock,
    RtpData* incoming_payload_callback,
    RtpFeedback* incoming_messages_callback,
    RTPPayloadRegistry* rtp_payload_registry) {
  if (!incoming_payload_callback)
    incoming_payload_callback = NullObjectRtpData();
  if (!incoming_messages_callback)
    incoming_messages_callback = NullObjectRtpFeedback();
  return new RtpReceiverImpl(
      clock, incoming_messages_callback, rtp_payload_registry,
      RTPReceiverStrategy::CreateVideoStrategy(incoming_payload_callback));
}

RtpReceiver* RtpReceiver::CreateAudioReceiver(
    Clock* clock,
    RtpData* incoming_payload_callback,
    RtpFeedback* incoming_messages_callback,
    RTPPayloadRegistry* rtp_payload_registry) {
  if (!incoming_payload_callback)
    incoming_payload_callback = NullObjectRtpData();
  if (!incoming_messages_callback)
    incoming_messages_callback = NullObjectRtpFeedback();
  return new RtpReceiverImpl(
      clock, incoming_messages_callback, rtp_payload_registry,
      RTPReceiverStrategy::CreateAudioStrategy(incoming_payload_callback));
}

RtpReceiverImpl::RtpReceiverImpl(
    Clock* clock,
    RtpFeedback* incoming_messages_callback,
    RTPPayloadRegistry* rtp_payload_registry,
    RTPReceiverStrategy* rtp_media_receiver)
    : clock_(clock),
      rtp_payload_registry_(rtp_payload_registry),
      rtp_media_receiver_(rtp_media_receiver),
      cb_rtp_feedback_(incoming_messages_callback),
      last_receive_time_(0),
      last_received_payload_length_(0),
      ssrc_(0),
      num_csrcs_(0),
      current_remote_csrc_(),
      last_received_timestamp_(0),
      last_received_frame_time_ms_(-1),
      last_received_sequence_number_(0) {
  assert(incoming_messages_callback);

  memset(current_remote_csrc_, 0, sizeof(current_remote_csrc_));
}

RtpReceiverImpl::~RtpReceiverImpl() {
  for (int i = 0; i < num_csrcs_; ++i) {
    cb_rtp_feedback_->OnIncomingCSRCChanged(current_remote_csrc_[i], false);
  }
}

int32_t RtpReceiverImpl::RegisterReceivePayload(const CodecInst& audio_codec) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);

  // TODO(phoglund): Try to streamline handling of the RED codec and some other
  // cases which makes it necessary to keep track of whether we created a
  // payload or not.
  bool created_new_payload = false;
  int32_t result = rtp_payload_registry_->RegisterReceivePayload(
      audio_codec, &created_new_payload);
  if (created_new_payload) {
    if (rtp_media_receiver_->OnNewPayloadTypeCreated(audio_codec) != 0) {
      LOG(LS_ERROR) << "Failed to register payload: " << audio_codec.plname
                    << "/" << static_cast<int>(audio_codec.pltype);
      return -1;
    }
  }
  return result;
}

int32_t RtpReceiverImpl::RegisterReceivePayload(const VideoCodec& video_codec) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  return rtp_payload_registry_->RegisterReceivePayload(video_codec);
}

int32_t RtpReceiverImpl::DeRegisterReceivePayload(
    const int8_t payload_type) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  return rtp_payload_registry_->DeRegisterReceivePayload(payload_type);
}

uint32_t RtpReceiverImpl::SSRC() const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  return ssrc_;
}

// Get remote CSRC.
int32_t RtpReceiverImpl::CSRCs(uint32_t array_of_csrcs[kRtpCsrcSize]) const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);

  assert(num_csrcs_ <= kRtpCsrcSize);

  if (num_csrcs_ > 0) {
    memcpy(array_of_csrcs, current_remote_csrc_, sizeof(uint32_t)*num_csrcs_);
  }
  return num_csrcs_;
}

int32_t RtpReceiverImpl::Energy(
    uint8_t array_of_energy[kRtpCsrcSize]) const {
  return rtp_media_receiver_->Energy(array_of_energy);
}

bool RtpReceiverImpl::IncomingRtpPacket(
  const RTPHeader& rtp_header,
  const uint8_t* payload,
  size_t payload_length,
  PayloadUnion payload_specific,
  bool in_order) {
  // Trigger our callbacks.
  CheckSSRCChanged(rtp_header);

  int8_t first_payload_byte = payload_length > 0 ? payload[0] : 0;
  bool is_red = false;

  if (CheckPayloadChanged(rtp_header, first_payload_byte, &is_red,
                          &payload_specific) == -1) {
    if (payload_length == 0) {
      // OK, keep-alive packet.
      return true;
    }
    LOG(LS_WARNING) << "Receiving invalid payload type.";
    return false;
  }

  WebRtcRTPHeader webrtc_rtp_header;
  memset(&webrtc_rtp_header, 0, sizeof(webrtc_rtp_header));
  webrtc_rtp_header.header = rtp_header;
  CheckCSRC(webrtc_rtp_header);

  size_t payload_data_length = payload_length - rtp_header.paddingLength;

  bool is_first_packet_in_frame = false;
  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);
    if (HaveReceivedFrame()) {
      is_first_packet_in_frame =
          last_received_sequence_number_ + 1 == rtp_header.sequenceNumber &&
          last_received_timestamp_ != rtp_header.timestamp;
    } else {
      is_first_packet_in_frame = true;
    }
  }

  int32_t ret_val = rtp_media_receiver_->ParseRtpPacket(
      &webrtc_rtp_header, payload_specific, is_red, payload, payload_length,
      clock_->TimeInMilliseconds(), is_first_packet_in_frame);

  if (ret_val < 0) {
    return false;
  }

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    last_receive_time_ = clock_->TimeInMilliseconds();
    last_received_payload_length_ = payload_data_length;

    if (in_order) {
      if (last_received_timestamp_ != rtp_header.timestamp) {
        last_received_timestamp_ = rtp_header.timestamp;
        last_received_frame_time_ms_ = clock_->TimeInMilliseconds();
      }
      last_received_sequence_number_ = rtp_header.sequenceNumber;
    }
  }
  return true;
}

TelephoneEventHandler* RtpReceiverImpl::GetTelephoneEventHandler() {
  return rtp_media_receiver_->GetTelephoneEventHandler();
}

bool RtpReceiverImpl::Timestamp(uint32_t* timestamp) const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  if (!HaveReceivedFrame())
    return false;
  *timestamp = last_received_timestamp_;
  return true;
}

bool RtpReceiverImpl::LastReceivedTimeMs(int64_t* receive_time_ms) const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  if (!HaveReceivedFrame())
    return false;
  *receive_time_ms = last_received_frame_time_ms_;
  return true;
}

bool RtpReceiverImpl::HaveReceivedFrame() const {
  return last_received_frame_time_ms_ >= 0;
}

// Implementation note: must not hold critsect when called.
void RtpReceiverImpl::CheckSSRCChanged(const RTPHeader& rtp_header) {
  bool new_ssrc = false;
  bool re_initialize_decoder = false;
  char payload_name[RTP_PAYLOAD_NAME_SIZE];
  size_t channels = 1;
  uint32_t rate = 0;

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    int8_t last_received_payload_type =
        rtp_payload_registry_->last_received_payload_type();
    if (ssrc_ != rtp_header.ssrc ||
        (last_received_payload_type == -1 && ssrc_ == 0)) {
      // We need the payload_type_ to make the call if the remote SSRC is 0.
      new_ssrc = true;

      last_received_timestamp_ = 0;
      last_received_sequence_number_ = 0;
      last_received_frame_time_ms_ = -1;

      // Do we have a SSRC? Then the stream is restarted.
      if (ssrc_ != 0) {
        // Do we have the same codec? Then re-initialize coder.
        if (rtp_header.payloadType == last_received_payload_type) {
          re_initialize_decoder = true;

          const Payload* payload = rtp_payload_registry_->PayloadTypeToPayload(
              rtp_header.payloadType);
          if (!payload) {
            return;
          }
          payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
          strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
          if (payload->audio) {
            channels = payload->typeSpecific.Audio.channels;
            rate = payload->typeSpecific.Audio.rate;
          }
        }
      }
      ssrc_ = rtp_header.ssrc;
    }
  }

  if (new_ssrc) {
    // We need to get this to our RTCP sender and receiver.
    // We need to do this outside critical section.
    cb_rtp_feedback_->OnIncomingSSRCChanged(rtp_header.ssrc);
  }

  if (re_initialize_decoder) {
    if (-1 ==
        cb_rtp_feedback_->OnInitializeDecoder(
            rtp_header.payloadType, payload_name,
            rtp_header.payload_type_frequency, channels, rate)) {
      // New stream, same codec.
      LOG(LS_ERROR) << "Failed to create decoder for payload type: "
                    << static_cast<int>(rtp_header.payloadType);
    }
  }
}

// Implementation note: must not hold critsect when called.
// TODO(phoglund): Move as much as possible of this code path into the media
// specific receivers. Basically this method goes through a lot of trouble to
// compute something which is only used by the media specific parts later. If
// this code path moves we can get rid of some of the rtp_receiver ->
// media_specific interface (such as CheckPayloadChange, possibly get/set
// last known payload).
int32_t RtpReceiverImpl::CheckPayloadChanged(const RTPHeader& rtp_header,
                                             const int8_t first_payload_byte,
                                             bool* is_red,
                                             PayloadUnion* specific_payload) {
  bool re_initialize_decoder = false;

  char payload_name[RTP_PAYLOAD_NAME_SIZE];
  int8_t payload_type = rtp_header.payloadType;

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    int8_t last_received_payload_type =
        rtp_payload_registry_->last_received_payload_type();
    // TODO(holmer): Remove this code when RED parsing has been broken out from
    // RtpReceiverAudio.
    if (payload_type != last_received_payload_type) {
      if (rtp_payload_registry_->red_payload_type() == payload_type) {
        // Get the real codec payload type.
        payload_type = first_payload_byte & 0x7f;
        *is_red = true;

        if (rtp_payload_registry_->red_payload_type() == payload_type) {
          // Invalid payload type, traced by caller. If we proceeded here,
          // this would be set as |_last_received_payload_type|, and we would no
          // longer catch corrupt packets at this level.
          return -1;
        }

        // When we receive RED we need to check the real payload type.
        if (payload_type == last_received_payload_type) {
          rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);
          return 0;
        }
      }
      bool should_discard_changes = false;

      rtp_media_receiver_->CheckPayloadChanged(
        payload_type, specific_payload,
        &should_discard_changes);

      if (should_discard_changes) {
        *is_red = false;
        return 0;
      }

      const Payload* payload =
          rtp_payload_registry_->PayloadTypeToPayload(payload_type);
      if (!payload) {
        // Not a registered payload type.
        return -1;
      }
      payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
      strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);

      rtp_payload_registry_->set_last_received_payload_type(payload_type);

      re_initialize_decoder = true;

      rtp_media_receiver_->SetLastMediaSpecificPayload(payload->typeSpecific);
      rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);

      if (!payload->audio) {
        bool media_type_unchanged =
            rtp_payload_registry_->ReportMediaPayloadType(payload_type);
        if (media_type_unchanged) {
          // Only reset the decoder if the media codec type has changed.
          re_initialize_decoder = false;
        }
      }
    } else {
      rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);
      *is_red = false;
    }
  }  // End critsect.

  if (re_initialize_decoder) {
    if (-1 ==
        rtp_media_receiver_->InvokeOnInitializeDecoder(
            cb_rtp_feedback_, payload_type, payload_name, *specific_payload)) {
      return -1;  // Wrong payload type.
    }
  }
  return 0;
}

// Implementation note: must not hold critsect when called.
void RtpReceiverImpl::CheckCSRC(const WebRtcRTPHeader& rtp_header) {
  int32_t num_csrcs_diff = 0;
  uint32_t old_remote_csrc[kRtpCsrcSize];
  uint8_t old_num_csrcs = 0;

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    if (!rtp_media_receiver_->ShouldReportCsrcChanges(
        rtp_header.header.payloadType)) {
      return;
    }
    old_num_csrcs  = num_csrcs_;
    if (old_num_csrcs > 0) {
      // Make a copy of old.
      memcpy(old_remote_csrc, current_remote_csrc_,
             num_csrcs_ * sizeof(uint32_t));
    }
    const uint8_t num_csrcs = rtp_header.header.numCSRCs;
    if ((num_csrcs > 0) && (num_csrcs <= kRtpCsrcSize)) {
      // Copy new.
      memcpy(current_remote_csrc_,
             rtp_header.header.arrOfCSRCs,
             num_csrcs * sizeof(uint32_t));
    }
    if (num_csrcs > 0 || old_num_csrcs > 0) {
      num_csrcs_diff = num_csrcs - old_num_csrcs;
      num_csrcs_ = num_csrcs;  // Update stored CSRCs.
    } else {
      // No change.
      return;
    }
  }  // End critsect.

  bool have_called_callback = false;
  // Search for new CSRC in old array.
  for (uint8_t i = 0; i < rtp_header.header.numCSRCs; ++i) {
    const uint32_t csrc = rtp_header.header.arrOfCSRCs[i];

    bool found_match = false;
    for (uint8_t j = 0; j < old_num_csrcs; ++j) {
      if (csrc == old_remote_csrc[j]) {  // old list
        found_match = true;
        break;
      }
    }
    if (!found_match && csrc) {
      // Didn't find it, report it as new.
      have_called_callback = true;
      cb_rtp_feedback_->OnIncomingCSRCChanged(csrc, true);
    }
  }
  // Search for old CSRC in new array.
  for (uint8_t i = 0; i < old_num_csrcs; ++i) {
    const uint32_t csrc = old_remote_csrc[i];

    bool found_match = false;
    for (uint8_t j = 0; j < rtp_header.header.numCSRCs; ++j) {
      if (csrc == rtp_header.header.arrOfCSRCs[j]) {
        found_match = true;
        break;
      }
    }
    if (!found_match && csrc) {
      // Did not find it, report as removed.
      have_called_callback = true;
      cb_rtp_feedback_->OnIncomingCSRCChanged(csrc, false);
    }
  }
  if (!have_called_callback) {
    // If the CSRC list contain non-unique entries we will end up here.
    // Using CSRC 0 to signal this event, not interop safe, other
    // implementations might have CSRC 0 as a valid value.
    if (num_csrcs_diff > 0) {
      cb_rtp_feedback_->OnIncomingCSRCChanged(0, true);
    } else if (num_csrcs_diff < 0) {
      cb_rtp_feedback_->OnIncomingCSRCChanged(0, false);
    }
  }
}

}  // namespace webrtc
