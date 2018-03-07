/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_impl.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
bool InOrderPacket(rtc::Optional<uint16_t> latest_sequence_number,
                   uint16_t current_sequence_number) {
  if (!latest_sequence_number)
    return true;

  // We need to distinguish between a late or retransmitted packet,
  // and a sequence number discontinuity.
  if (IsNewerSequenceNumber(current_sequence_number, *latest_sequence_number)) {
    return true;
  } else {
    // If we have a restart of the remote side this packet is still in order.
    return !IsNewerSequenceNumber(
        current_sequence_number,
        *latest_sequence_number - kDefaultMaxReorderingThreshold);
  }
}

}  // namespace

using RtpUtility::Payload;

// Only return the sources in the last 10 seconds.
const int64_t kGetSourcesTimeoutMs = 10000;

RtpReceiver* RtpReceiver::CreateVideoReceiver(
    Clock* clock,
    RtpData* incoming_payload_callback,
    RtpFeedback* incoming_messages_callback,
    RTPPayloadRegistry* rtp_payload_registry) {
  RTC_DCHECK(incoming_payload_callback != nullptr);
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
  RTC_DCHECK(incoming_payload_callback != nullptr);
  if (!incoming_messages_callback)
    incoming_messages_callback = NullObjectRtpFeedback();
  return new RtpReceiverImpl(
      clock, incoming_messages_callback, rtp_payload_registry,
      RTPReceiverStrategy::CreateAudioStrategy(incoming_payload_callback));
}

int32_t RtpReceiver::RegisterReceivePayload(const CodecInst& audio_codec) {
  return RegisterReceivePayload(audio_codec.pltype,
                                CodecInstToSdp(audio_codec));
}

RtpReceiverImpl::RtpReceiverImpl(Clock* clock,
                                 RtpFeedback* incoming_messages_callback,
                                 RTPPayloadRegistry* rtp_payload_registry,
                                 RTPReceiverStrategy* rtp_media_receiver)
    : clock_(clock),
      rtp_payload_registry_(rtp_payload_registry),
      rtp_media_receiver_(rtp_media_receiver),
      cb_rtp_feedback_(incoming_messages_callback),
      ssrc_(0),
      num_csrcs_(0),
      current_remote_csrc_(),
      last_received_timestamp_(0),
      last_received_frame_time_ms_(-1) {
  assert(incoming_messages_callback);

  memset(current_remote_csrc_, 0, sizeof(current_remote_csrc_));
}

RtpReceiverImpl::~RtpReceiverImpl() {
  for (int i = 0; i < num_csrcs_; ++i) {
    cb_rtp_feedback_->OnIncomingCSRCChanged(current_remote_csrc_[i], false);
  }
}

int32_t RtpReceiverImpl::RegisterReceivePayload(
    int payload_type,
    const SdpAudioFormat& audio_format) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);

  // TODO(phoglund): Try to streamline handling of the RED codec and some other
  // cases which makes it necessary to keep track of whether we created a
  // payload or not.
  bool created_new_payload = false;
  int32_t result = rtp_payload_registry_->RegisterReceivePayload(
      payload_type, audio_format, &created_new_payload);
  if (created_new_payload) {
    if (rtp_media_receiver_->OnNewPayloadTypeCreated(payload_type,
                                                     audio_format) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to register payload: " << audio_format.name
                        << "/" << payload_type;
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

bool RtpReceiverImpl::IncomingRtpPacket(const RTPHeader& rtp_header,
                                        const uint8_t* payload,
                                        size_t payload_length,
                                        PayloadUnion payload_specific) {
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
    RTC_LOG(LS_WARNING) << "Receiving invalid payload type.";
    return false;
  }

  WebRtcRTPHeader webrtc_rtp_header;
  memset(&webrtc_rtp_header, 0, sizeof(webrtc_rtp_header));
  webrtc_rtp_header.header = rtp_header;
  CheckCSRC(webrtc_rtp_header);

  auto audio_level =
      rtp_header.extension.hasAudioLevel
          ? rtc::Optional<uint8_t>(rtp_header.extension.audioLevel)
          : rtc::nullopt;
  UpdateSources(audio_level);

  int32_t ret_val = rtp_media_receiver_->ParseRtpPacket(
      &webrtc_rtp_header, payload_specific, is_red, payload, payload_length,
      clock_->TimeInMilliseconds());

  if (ret_val < 0) {
    return false;
  }

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    // TODO(nisse): Do not rely on InOrderPacket for recovered packets, when
    // packet is passed as RtpPacketReceived and that information is available.
    // We should ideally never record timestamps for retransmitted or recovered
    // packets.
    if (InOrderPacket(last_received_sequence_number_,
                      rtp_header.sequenceNumber)) {
      last_received_sequence_number_.emplace(rtp_header.sequenceNumber);
      last_received_timestamp_ = rtp_header.timestamp;
      last_received_frame_time_ms_ = clock_->TimeInMilliseconds();
    }
  }

  return true;
}

TelephoneEventHandler* RtpReceiverImpl::GetTelephoneEventHandler() {
  return rtp_media_receiver_->GetTelephoneEventHandler();
}

std::vector<RtpSource> RtpReceiverImpl::GetSources() const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);

  int64_t now_ms = clock_->TimeInMilliseconds();
  std::vector<RtpSource> sources;

  RTC_DCHECK(std::is_sorted(ssrc_sources_.begin(), ssrc_sources_.end(),
                            [](const RtpSource& lhs, const RtpSource& rhs) {
                              return lhs.timestamp_ms() < rhs.timestamp_ms();
                            }));
  RTC_DCHECK(std::is_sorted(csrc_sources_.begin(), csrc_sources_.end(),
                            [](const RtpSource& lhs, const RtpSource& rhs) {
                              return lhs.timestamp_ms() < rhs.timestamp_ms();
                            }));

  std::set<uint32_t> selected_ssrcs;
  for (auto rit = ssrc_sources_.rbegin(); rit != ssrc_sources_.rend(); ++rit) {
    if ((now_ms - rit->timestamp_ms()) > kGetSourcesTimeoutMs) {
      break;
    }
    if (selected_ssrcs.insert(rit->source_id()).second) {
      sources.push_back(*rit);
    }
  }

  for (auto rit = csrc_sources_.rbegin(); rit != csrc_sources_.rend(); ++rit) {
    if ((now_ms - rit->timestamp_ms()) > kGetSourcesTimeoutMs) {
      break;
    }
    sources.push_back(*rit);
  }
  return sources;
}

bool RtpReceiverImpl::GetLatestTimestamps(uint32_t* timestamp,
                                          int64_t* receive_time_ms) const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  if (!last_received_sequence_number_)
    return false;

  *timestamp = last_received_timestamp_;
  *receive_time_ms = last_received_frame_time_ms_;

  return true;
}

// Implementation note: must not hold critsect when called.
void RtpReceiverImpl::CheckSSRCChanged(const RTPHeader& rtp_header) {
  bool new_ssrc = false;
  rtc::Optional<AudioPayload> reinitialize_audio_payload;

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    int8_t last_received_payload_type =
        rtp_payload_registry_->last_received_payload_type();
    if (ssrc_ != rtp_header.ssrc ||
        (last_received_payload_type == -1 && ssrc_ == 0)) {
      // We need the payload_type_ to make the call if the remote SSRC is 0.
      new_ssrc = true;

      last_received_timestamp_ = 0;
      last_received_frame_time_ms_ = -1;

      // Do we have a SSRC? Then the stream is restarted.
      if (ssrc_ != 0) {
        // Do we have the same codec? Then re-initialize coder.
        if (rtp_header.payloadType == last_received_payload_type) {
          const auto payload = rtp_payload_registry_->PayloadTypeToPayload(
              rtp_header.payloadType);
          if (!payload) {
            return;
          }
          if (payload->typeSpecific.is_audio()) {
            reinitialize_audio_payload.emplace(
                payload->typeSpecific.audio_payload());
          } else {
            // OnInitializeDecoder() is only used for audio.
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

  if (reinitialize_audio_payload) {
    if (-1 == cb_rtp_feedback_->OnInitializeDecoder(
                  rtp_header.payloadType, reinitialize_audio_payload->format,
                  reinitialize_audio_payload->rate)) {
      // New stream, same codec.
      RTC_LOG(LS_ERROR) << "Failed to create decoder for payload type: "
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

      const auto payload =
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

      if (!payload->typeSpecific.is_audio()) {
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

void RtpReceiverImpl::UpdateSources(
    const rtc::Optional<uint8_t>& ssrc_audio_level) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  int64_t now_ms = clock_->TimeInMilliseconds();

  for (size_t i = 0; i < num_csrcs_; ++i) {
    auto map_it = iterator_by_csrc_.find(current_remote_csrc_[i]);
    if (map_it == iterator_by_csrc_.end()) {
      // If it is a new CSRC, append a new object to the end of the list.
      csrc_sources_.emplace_back(now_ms, current_remote_csrc_[i],
                                 RtpSourceType::CSRC);
    } else {
      // If it is an existing CSRC, move the object to the end of the list.
      map_it->second->update_timestamp_ms(now_ms);
      csrc_sources_.splice(csrc_sources_.end(), csrc_sources_, map_it->second);
    }
    // Update the unordered_map.
    iterator_by_csrc_[current_remote_csrc_[i]] = std::prev(csrc_sources_.end());
  }

  // If this is the first packet or the SSRC is changed, insert a new
  // contributing source that uses the SSRC.
  if (ssrc_sources_.empty() || ssrc_sources_.rbegin()->source_id() != ssrc_) {
    ssrc_sources_.emplace_back(now_ms, ssrc_, RtpSourceType::SSRC);
  } else {
    ssrc_sources_.rbegin()->update_timestamp_ms(now_ms);
  }

  ssrc_sources_.back().set_audio_level(ssrc_audio_level);

  RemoveOutdatedSources(now_ms);
}

void RtpReceiverImpl::RemoveOutdatedSources(int64_t now_ms) {
  std::list<RtpSource>::iterator it;
  for (it = csrc_sources_.begin(); it != csrc_sources_.end(); ++it) {
    if ((now_ms - it->timestamp_ms()) <= kGetSourcesTimeoutMs) {
      break;
    }
    iterator_by_csrc_.erase(it->source_id());
  }
  csrc_sources_.erase(csrc_sources_.begin(), it);

  std::vector<RtpSource>::iterator vec_it;
  for (vec_it = ssrc_sources_.begin(); vec_it != ssrc_sources_.end();
       ++vec_it) {
    if ((now_ms - vec_it->timestamp_ms()) <= kGetSourcesTimeoutMs) {
      break;
    }
  }
  ssrc_sources_.erase(ssrc_sources_.begin(), vec_it);
}

}  // namespace webrtc
