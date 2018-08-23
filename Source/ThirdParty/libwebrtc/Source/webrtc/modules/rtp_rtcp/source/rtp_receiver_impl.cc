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
bool InOrderPacket(absl::optional<uint16_t> latest_sequence_number,
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
    RTPPayloadRegistry* rtp_payload_registry) {
  RTC_DCHECK(incoming_payload_callback != nullptr);
  return new RtpReceiverImpl(
      clock, rtp_payload_registry,
      RTPReceiverStrategy::CreateVideoStrategy(incoming_payload_callback));
}

RtpReceiver* RtpReceiver::CreateAudioReceiver(
    Clock* clock,
    RtpData* incoming_payload_callback,
    RTPPayloadRegistry* rtp_payload_registry) {
  RTC_DCHECK(incoming_payload_callback != nullptr);
  return new RtpReceiverImpl(
      clock, rtp_payload_registry,
      RTPReceiverStrategy::CreateAudioStrategy(incoming_payload_callback));
}

int32_t RtpReceiver::RegisterReceivePayload(const CodecInst& audio_codec) {
  return RegisterReceivePayload(audio_codec.pltype,
                                CodecInstToSdp(audio_codec));
}

RtpReceiverImpl::RtpReceiverImpl(Clock* clock,
                                 RTPPayloadRegistry* rtp_payload_registry,
                                 RTPReceiverStrategy* rtp_media_receiver)
    : clock_(clock),
      rtp_payload_registry_(rtp_payload_registry),
      rtp_media_receiver_(rtp_media_receiver),
      ssrc_(0),
      last_received_timestamp_(0),
      last_received_frame_time_ms_(-1) {}

RtpReceiverImpl::~RtpReceiverImpl() {}

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
  return result;
}

int32_t RtpReceiverImpl::RegisterReceivePayload(const VideoCodec& video_codec) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  return rtp_payload_registry_->RegisterReceivePayload(video_codec);
}

int32_t RtpReceiverImpl::DeRegisterReceivePayload(const int8_t payload_type) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  return rtp_payload_registry_->DeRegisterReceivePayload(payload_type);
}

uint32_t RtpReceiverImpl::SSRC() const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  return ssrc_;
}

bool RtpReceiverImpl::IncomingRtpPacket(const RTPHeader& rtp_header,
                                        const uint8_t* payload,
                                        size_t payload_length,
                                        PayloadUnion payload_specific) {
  // Trigger our callbacks.
  CheckSSRCChanged(rtp_header);

  if (payload_length == 0) {
    // OK, keep-alive packet.
    return true;
  }
  int64_t now_ms = clock_->TimeInMilliseconds();

  {
    rtc::CritScope lock(&critical_section_rtp_receiver_);

    csrcs_.Update(
        now_ms, rtc::MakeArrayView(rtp_header.arrOfCSRCs, rtp_header.numCSRCs));
  }

  WebRtcRTPHeader webrtc_rtp_header{};
  webrtc_rtp_header.header = rtp_header;

  auto audio_level =
      rtp_header.extension.hasAudioLevel
          ? absl::optional<uint8_t>(rtp_header.extension.audioLevel)
          : absl::nullopt;
  UpdateSources(audio_level);

  int32_t ret_val = rtp_media_receiver_->ParseRtpPacket(
      &webrtc_rtp_header, payload_specific, payload, payload_length, now_ms);

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

std::vector<RtpSource> RtpReceiverImpl::GetSources() const {
  rtc::CritScope lock(&critical_section_rtp_receiver_);

  int64_t now_ms = clock_->TimeInMilliseconds();
  RTC_DCHECK(std::is_sorted(ssrc_sources_.begin(), ssrc_sources_.end(),
                            [](const RtpSource& lhs, const RtpSource& rhs) {
                              return lhs.timestamp_ms() < rhs.timestamp_ms();
                            }));
  std::vector<RtpSource> sources = csrcs_.GetSources(now_ms);

  std::set<uint32_t> selected_ssrcs;
  for (auto rit = ssrc_sources_.rbegin(); rit != ssrc_sources_.rend(); ++rit) {
    if ((now_ms - rit->timestamp_ms()) > kGetSourcesTimeoutMs) {
      break;
    }
    if (selected_ssrcs.insert(rit->source_id()).second) {
      sources.push_back(*rit);
    }
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

// TODO(nisse): Delete.
// Implementation note: must not hold critsect when called.
void RtpReceiverImpl::CheckSSRCChanged(const RTPHeader& rtp_header) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  ssrc_ = rtp_header.ssrc;
}

void RtpReceiverImpl::UpdateSources(
    const absl::optional<uint8_t>& ssrc_audio_level) {
  rtc::CritScope lock(&critical_section_rtp_receiver_);
  int64_t now_ms = clock_->TimeInMilliseconds();

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
