/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_transceiver.h"

#include <string>

#include "absl/algorithm/container.h"
#include "pc/channel_manager.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_parameters_conversion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpTransceiver::RtpTransceiver(cricket::MediaType media_type)
    : unified_plan_(false), media_type_(media_type) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
}

RtpTransceiver::RtpTransceiver(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver,
    cricket::ChannelManager* channel_manager)
    : unified_plan_(true),
      media_type_(sender->media_type()),
      channel_manager_(channel_manager) {
  RTC_DCHECK(media_type_ == cricket::MEDIA_TYPE_AUDIO ||
             media_type_ == cricket::MEDIA_TYPE_VIDEO);
  RTC_DCHECK_EQ(sender->media_type(), receiver->media_type());
  senders_.push_back(sender);
  receivers_.push_back(receiver);
}

RtpTransceiver::~RtpTransceiver() {
  Stop();
}

void RtpTransceiver::SetChannel(cricket::ChannelInterface* channel) {
  // Cannot set a non-null channel on a stopped transceiver.
  if (stopped_ && channel) {
    return;
  }

  if (channel) {
    RTC_DCHECK_EQ(media_type(), channel->media_type());
  }

  if (channel_) {
    channel_->SignalFirstPacketReceived().disconnect(this);
  }

  channel_ = channel;

  if (channel_) {
    channel_->SignalFirstPacketReceived().connect(
        this, &RtpTransceiver::OnFirstPacketReceived);
  }

  for (const auto& sender : senders_) {
    sender->internal()->SetMediaChannel(channel_ ? channel_->media_channel()
                                                 : nullptr);
  }

  for (const auto& receiver : receivers_) {
    if (!channel_) {
      receiver->internal()->Stop();
    }

    receiver->internal()->SetMediaChannel(channel_ ? channel_->media_channel()
                                                   : nullptr);
  }
}

void RtpTransceiver::AddSender(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender) {
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(sender);
  RTC_DCHECK_EQ(media_type(), sender->media_type());
  RTC_DCHECK(!absl::c_linear_search(senders_, sender));
  senders_.push_back(sender);
}

bool RtpTransceiver::RemoveSender(RtpSenderInterface* sender) {
  RTC_DCHECK(!unified_plan_);
  if (sender) {
    RTC_DCHECK_EQ(media_type(), sender->media_type());
  }
  auto it = absl::c_find(senders_, sender);
  if (it == senders_.end()) {
    return false;
  }
  (*it)->internal()->Stop();
  senders_.erase(it);
  return true;
}

void RtpTransceiver::AddReceiver(
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver) {
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(receiver);
  RTC_DCHECK_EQ(media_type(), receiver->media_type());
  RTC_DCHECK(!absl::c_linear_search(receivers_, receiver));
  receivers_.push_back(receiver);
}

bool RtpTransceiver::RemoveReceiver(RtpReceiverInterface* receiver) {
  RTC_DCHECK(!unified_plan_);
  if (receiver) {
    RTC_DCHECK_EQ(media_type(), receiver->media_type());
  }
  auto it = absl::c_find(receivers_, receiver);
  if (it == receivers_.end()) {
    return false;
  }
  (*it)->internal()->Stop();
  receivers_.erase(it);
  return true;
}

rtc::scoped_refptr<RtpSenderInternal> RtpTransceiver::sender_internal() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, senders_.size());
  return senders_[0]->internal();
}

rtc::scoped_refptr<RtpReceiverInternal> RtpTransceiver::receiver_internal()
    const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, receivers_.size());
  return receivers_[0]->internal();
}

cricket::MediaType RtpTransceiver::media_type() const {
  return media_type_;
}

absl::optional<std::string> RtpTransceiver::mid() const {
  return mid_;
}

void RtpTransceiver::OnFirstPacketReceived(cricket::ChannelInterface*) {
  for (const auto& receiver : receivers_) {
    receiver->internal()->NotifyFirstPacketReceived();
  }
}

rtc::scoped_refptr<RtpSenderInterface> RtpTransceiver::sender() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, senders_.size());
  return senders_[0];
}

rtc::scoped_refptr<RtpReceiverInterface> RtpTransceiver::receiver() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, receivers_.size());
  return receivers_[0];
}

void RtpTransceiver::set_current_direction(RtpTransceiverDirection direction) {
  RTC_LOG(LS_INFO) << "Changing transceiver (MID=" << mid_.value_or("<not set>")
                   << ") current direction from "
                   << (current_direction_ ? RtpTransceiverDirectionToString(
                                                *current_direction_)
                                          : "<not set>")
                   << " to " << RtpTransceiverDirectionToString(direction)
                   << ".";
  current_direction_ = direction;
  if (RtpTransceiverDirectionHasSend(*current_direction_)) {
    has_ever_been_used_to_send_ = true;
  }
}

void RtpTransceiver::set_fired_direction(RtpTransceiverDirection direction) {
  fired_direction_ = direction;
}

bool RtpTransceiver::stopped() const {
  return stopped_;
}

RtpTransceiverDirection RtpTransceiver::direction() const {
  return direction_;
}

void RtpTransceiver::SetDirection(RtpTransceiverDirection new_direction) {
  if (stopped()) {
    return;
  }
  if (new_direction == direction_) {
    return;
  }
  direction_ = new_direction;
  SignalNegotiationNeeded();
}

absl::optional<RtpTransceiverDirection> RtpTransceiver::current_direction()
    const {
  return current_direction_;
}

absl::optional<RtpTransceiverDirection> RtpTransceiver::fired_direction()
    const {
  return fired_direction_;
}

void RtpTransceiver::Stop() {
  for (const auto& sender : senders_) {
    sender->internal()->Stop();
  }
  for (const auto& receiver : receivers_) {
    receiver->internal()->Stop();
  }
  stopped_ = true;
  current_direction_ = absl::nullopt;
}

RTCError RtpTransceiver::SetCodecPreferences(
    rtc::ArrayView<RtpCodecCapability> codec_capabilities) {
  RTC_DCHECK(unified_plan_);

  // 3. If codecs is an empty list, set transceiver's [[PreferredCodecs]] slot
  // to codecs and abort these steps.
  if (codec_capabilities.empty()) {
    codec_preferences_.clear();
    return RTCError::OK();
  }

  // 4. Remove any duplicate values in codecs.
  std::vector<RtpCodecCapability> codecs;
  absl::c_remove_copy_if(codec_capabilities, std::back_inserter(codecs),
                         [&codecs](const RtpCodecCapability& codec) {
                           return absl::c_linear_search(codecs, codec);
                         });

  if (media_type_ == cricket::MEDIA_TYPE_AUDIO) {
    std::vector<cricket::AudioCodec> audio_codecs;

    std::vector<cricket::AudioCodec> recv_codecs, send_codecs;
    channel_manager_->GetSupportedAudioReceiveCodecs(&recv_codecs);
    channel_manager_->GetSupportedAudioSendCodecs(&send_codecs);

    // 6. If the intersection between codecs and
    // RTCRtpSender.getCapabilities(kind).codecs or the intersection between
    // codecs and RTCRtpReceiver.getCapabilities(kind).codecs only contains RTX,
    // RED or FEC codecs or is an empty set, throw InvalidModificationError.
    // This ensures that we always have something to offer, regardless of
    // transceiver.direction.

    if (!absl::c_any_of(
            codecs, [&recv_codecs](const RtpCodecCapability& codec) {
              return codec.name != cricket::kRtxCodecName &&
                     codec.name != cricket::kRedCodecName &&
                     codec.name != cricket::kFlexfecCodecName &&
                     absl::c_any_of(
                         recv_codecs,
                         [&codec](const cricket::AudioCodec& recv_codec) {
                           return recv_codec.MatchesCapability(codec);
                         });
            })) {
      return RTCError(RTCErrorType::INVALID_MODIFICATION,
                      "Invalid codec preferences: Missing codec from recv "
                      "codec capabilities.");
    }

    if (!absl::c_any_of(
            codecs, [&send_codecs](const RtpCodecCapability& codec) {
              return codec.name != cricket::kRtxCodecName &&
                     codec.name != cricket::kRedCodecName &&
                     codec.name != cricket::kFlexfecCodecName &&
                     absl::c_any_of(
                         send_codecs,
                         [&codec](const cricket::AudioCodec& send_codec) {
                           return send_codec.MatchesCapability(codec);
                         });
            })) {
      return RTCError(RTCErrorType::INVALID_MODIFICATION,
                      "Invalid codec preferences: Missing codec from send "
                      "codec capabilities.");
    }

    // 7. Let codecCapabilities be the union of
    // RTCRtpSender.getCapabilities(kind).codecs and
    // RTCRtpReceiver.getCapabilities(kind).codecs. 8.1 For each codec in
    // codecs, If codec is not in codecCapabilities, throw
    // InvalidModificationError.
    for (const auto& codec_preference : codecs) {
      bool is_recv_codec = absl::c_any_of(
          recv_codecs, [&codec_preference](const cricket::AudioCodec& codec) {
            return codec.MatchesCapability(codec_preference);
          });

      bool is_send_codec = absl::c_any_of(
          send_codecs, [&codec_preference](const cricket::AudioCodec& codec) {
            return codec.MatchesCapability(codec_preference);
          });

      if (!is_recv_codec && !is_send_codec) {
        return RTCError(
            RTCErrorType::INVALID_MODIFICATION,
            std::string(
                "Invalid codec preferences: invalid codec with name \"") +
                codec_preference.name + "\".");
      }
    }
  } else if (media_type_ == cricket::MEDIA_TYPE_VIDEO) {
    std::vector<cricket::VideoCodec> video_codecs;
    // Video codecs are both for the receive and send side, so the checks are
    // simpler than the audio ones.
    channel_manager_->GetSupportedVideoCodecs(&video_codecs);

    // Validate codecs
    for (const auto& codec_preference : codecs) {
      if (!absl::c_any_of(video_codecs, [&codec_preference](
                                            const cricket::VideoCodec& codec) {
            return codec.MatchesCapability(codec_preference);
          })) {
        return RTCError(
            RTCErrorType::INVALID_MODIFICATION,
            std::string(
                "Invalid codec preferences: invalid codec with name \"") +
                codec_preference.name + "\".");
      }
    }
  }

  // Check we have a real codec (not just rtx, red or fec)
  if (absl::c_all_of(codecs, [](const RtpCodecCapability& codec) {
        return codec.name == cricket::kRtxCodecName ||
               codec.name == cricket::kRedCodecName ||
               codec.name == cricket::kUlpfecCodecName;
      })) {
    return RTCError(RTCErrorType::INVALID_MODIFICATION,
                    "Invalid codec preferences: codec list must have a non "
                    "RTX, RED or FEC entry.");
  }

  codec_preferences_ = codecs;

  return RTCError::OK();
}

}  // namespace webrtc
