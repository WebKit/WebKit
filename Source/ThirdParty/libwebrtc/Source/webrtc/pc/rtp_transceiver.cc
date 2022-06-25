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

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/rtp_parameters.h"
#include "api/sequence_checker.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "pc/channel_manager.h"
#include "pc/rtp_media_utils.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace {
template <class T>
RTCError VerifyCodecPreferences(const std::vector<RtpCodecCapability>& codecs,
                                const std::vector<T>& send_codecs,
                                const std::vector<T>& recv_codecs) {
  // If the intersection between codecs and
  // RTCRtpSender.getCapabilities(kind).codecs or the intersection between
  // codecs and RTCRtpReceiver.getCapabilities(kind).codecs only contains RTX,
  // RED or FEC codecs or is an empty set, throw InvalidModificationError.
  // This ensures that we always have something to offer, regardless of
  // transceiver.direction.

  if (!absl::c_any_of(codecs, [&recv_codecs](const RtpCodecCapability& codec) {
        return codec.name != cricket::kRtxCodecName &&
               codec.name != cricket::kRedCodecName &&
               codec.name != cricket::kFlexfecCodecName &&
               absl::c_any_of(recv_codecs, [&codec](const T& recv_codec) {
                 return recv_codec.MatchesCapability(codec);
               });
      })) {
    return RTCError(RTCErrorType::INVALID_MODIFICATION,
                    "Invalid codec preferences: Missing codec from recv "
                    "codec capabilities.");
  }

  if (!absl::c_any_of(codecs, [&send_codecs](const RtpCodecCapability& codec) {
        return codec.name != cricket::kRtxCodecName &&
               codec.name != cricket::kRedCodecName &&
               codec.name != cricket::kFlexfecCodecName &&
               absl::c_any_of(send_codecs, [&codec](const T& send_codec) {
                 return send_codec.MatchesCapability(codec);
               });
      })) {
    return RTCError(RTCErrorType::INVALID_MODIFICATION,
                    "Invalid codec preferences: Missing codec from send "
                    "codec capabilities.");
  }

  // Let codecCapabilities be the union of
  // RTCRtpSender.getCapabilities(kind).codecs and
  // RTCRtpReceiver.getCapabilities(kind).codecs. For each codec in codecs, If
  // codec is not in codecCapabilities, throw InvalidModificationError.
  for (const auto& codec_preference : codecs) {
    bool is_recv_codec =
        absl::c_any_of(recv_codecs, [&codec_preference](const T& codec) {
          return codec.MatchesCapability(codec_preference);
        });

    bool is_send_codec =
        absl::c_any_of(send_codecs, [&codec_preference](const T& codec) {
          return codec.MatchesCapability(codec_preference);
        });

    if (!is_recv_codec && !is_send_codec) {
      return RTCError(
          RTCErrorType::INVALID_MODIFICATION,
          std::string("Invalid codec preferences: invalid codec with name \"") +
              codec_preference.name + "\".");
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

  return RTCError::OK();
}

TaskQueueBase* GetCurrentTaskQueueOrThread() {
  TaskQueueBase* current = TaskQueueBase::Current();
  if (!current)
    current = rtc::ThreadManager::Instance()->CurrentThread();
  return current;
}

}  // namespace

RtpTransceiver::RtpTransceiver(
    cricket::MediaType media_type,
    cricket::ChannelManager* channel_manager /* = nullptr*/)
    : thread_(GetCurrentTaskQueueOrThread()),
      unified_plan_(false),
      media_type_(media_type),
      channel_manager_(channel_manager) {
  RTC_DCHECK(media_type == cricket::MEDIA_TYPE_AUDIO ||
             media_type == cricket::MEDIA_TYPE_VIDEO);
  RTC_DCHECK(channel_manager_);
}

RtpTransceiver::RtpTransceiver(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
    rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
        receiver,
    cricket::ChannelManager* channel_manager,
    std::vector<RtpHeaderExtensionCapability> header_extensions_offered,
    std::function<void()> on_negotiation_needed)
    : thread_(GetCurrentTaskQueueOrThread()),
      unified_plan_(true),
      media_type_(sender->media_type()),
      channel_manager_(channel_manager),
      header_extensions_to_offer_(std::move(header_extensions_offered)),
      on_negotiation_needed_(std::move(on_negotiation_needed)) {
  RTC_DCHECK(media_type_ == cricket::MEDIA_TYPE_AUDIO ||
             media_type_ == cricket::MEDIA_TYPE_VIDEO);
  RTC_DCHECK_EQ(sender->media_type(), receiver->media_type());
  RTC_DCHECK(channel_manager_);
  senders_.push_back(sender);
  receivers_.push_back(receiver);
}

RtpTransceiver::~RtpTransceiver() {
  // TODO(tommi): On Android, when running PeerConnectionClientTest (e.g.
  // PeerConnectionClientTest#testCameraSwitch), the instance doesn't get
  // deleted on `thread_`. See if we can fix that.
  if (!stopped_) {
    RTC_DCHECK_RUN_ON(thread_);
    StopInternal();
  }
}

void RtpTransceiver::SetChannel(cricket::ChannelInterface* channel) {
  RTC_DCHECK_RUN_ON(thread_);
  // Cannot set a non-null channel on a stopped transceiver.
  if (stopped_ && channel) {
    return;
  }

  RTC_DCHECK(channel || channel_);

  RTC_LOG_THREAD_BLOCK_COUNT();

  if (channel_) {
    signaling_thread_safety_->SetNotAlive();
    signaling_thread_safety_ = nullptr;
  }

  if (channel) {
    RTC_DCHECK_EQ(media_type(), channel->media_type());
    signaling_thread_safety_ = PendingTaskSafetyFlag::Create();
  }

  // An alternative to this, could be to require SetChannel to be called
  // on the network thread. The channel object operates for the most part
  // on the network thread, as part of its initialization being on the network
  // thread is required, so setting a channel object as part of the construction
  // (without thread hopping) might be the more efficient thing to do than
  // how SetChannel works today.
  // Similarly, if the channel() accessor is limited to the network thread, that
  // helps with keeping the channel implementation requirements being met and
  // avoids synchronization for accessing the pointer or network related state.
  channel_manager_->network_thread()->Invoke<void>(RTC_FROM_HERE, [&]() {
    if (channel_) {
      channel_->SetFirstPacketReceivedCallback(nullptr);
    }

    channel_ = channel;

    if (channel_) {
      channel_->SetFirstPacketReceivedCallback(
          [thread = thread_, flag = signaling_thread_safety_, this]() mutable {
            thread->PostTask(ToQueuedTask(
                std::move(flag), [this]() { OnFirstPacketReceived(); }));
          });
    }
  });

  for (const auto& sender : senders_) {
    sender->internal()->SetMediaChannel(channel_ ? channel_->media_channel()
                                                 : nullptr);
  }

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);

  for (const auto& receiver : receivers_) {
    if (!channel_) {
      receiver->internal()->Stop();
    } else {
      receiver->internal()->SetMediaChannel(channel_->media_channel());
    }
  }
}

void RtpTransceiver::AddSender(
    rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender) {
  RTC_DCHECK_RUN_ON(thread_);
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
  RTC_DCHECK_RUN_ON(thread_);
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
  // `Stop()` will clear the internally cached pointer to the media channel.
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

void RtpTransceiver::OnFirstPacketReceived() {
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
  RTC_DCHECK_RUN_ON(thread_);
  return stopped_;
}

bool RtpTransceiver::stopping() const {
  RTC_DCHECK_RUN_ON(thread_);
  return stopping_;
}

RtpTransceiverDirection RtpTransceiver::direction() const {
  if (unified_plan_ && stopping())
    return webrtc::RtpTransceiverDirection::kStopped;

  return direction_;
}

RTCError RtpTransceiver::SetDirectionWithError(
    RtpTransceiverDirection new_direction) {
  if (unified_plan_ && stopping()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "Cannot set direction on a stopping transceiver.");
  }
  if (new_direction == direction_)
    return RTCError::OK();

  if (new_direction == RtpTransceiverDirection::kStopped) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "The set direction 'stopped' is invalid.");
  }

  direction_ = new_direction;
  on_negotiation_needed_();

  return RTCError::OK();
}

absl::optional<RtpTransceiverDirection> RtpTransceiver::current_direction()
    const {
  if (unified_plan_ && stopped())
    return webrtc::RtpTransceiverDirection::kStopped;

  return current_direction_;
}

absl::optional<RtpTransceiverDirection> RtpTransceiver::fired_direction()
    const {
  return fired_direction_;
}

void RtpTransceiver::StopSendingAndReceiving() {
  // 1. Let sender be transceiver.[[Sender]].
  // 2. Let receiver be transceiver.[[Receiver]].
  //
  // 3. Stop sending media with sender.
  //
  // 4. Send an RTCP BYE for each RTP stream that was being sent by sender, as
  // specified in [RFC3550].
  RTC_DCHECK_RUN_ON(thread_);
  for (const auto& sender : senders_)
    sender->internal()->Stop();

  // 5. Stop receiving media with receiver.
  for (const auto& receiver : receivers_)
    receiver->internal()->StopAndEndTrack();

  stopping_ = true;
  direction_ = webrtc::RtpTransceiverDirection::kInactive;
}

RTCError RtpTransceiver::StopStandard() {
  RTC_DCHECK_RUN_ON(thread_);
  // If we're on Plan B, do what Stop() used to do there.
  if (!unified_plan_) {
    StopInternal();
    return RTCError::OK();
  }
  // 1. Let transceiver be the RTCRtpTransceiver object on which the method is
  // invoked.
  //
  // 2. Let connection be the RTCPeerConnection object associated with
  // transceiver.
  //
  // 3. If connection.[[IsClosed]] is true, throw an InvalidStateError.
  if (is_pc_closed_) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_STATE,
                         "PeerConnection is closed.");
  }

  // 4. If transceiver.[[Stopping]] is true, abort these steps.
  if (stopping_)
    return RTCError::OK();

  // 5. Stop sending and receiving given transceiver, and update the
  // negotiation-needed flag for connection.
  StopSendingAndReceiving();
  on_negotiation_needed_();

  return RTCError::OK();
}

void RtpTransceiver::StopInternal() {
  RTC_DCHECK_RUN_ON(thread_);
  StopTransceiverProcedure();
}

void RtpTransceiver::StopTransceiverProcedure() {
  RTC_DCHECK_RUN_ON(thread_);
  // As specified in the "Stop the RTCRtpTransceiver" procedure
  // 1. If transceiver.[[Stopping]] is false, stop sending and receiving given
  // transceiver.
  if (!stopping_)
    StopSendingAndReceiving();

  // 2. Set transceiver.[[Stopped]] to true.
  stopped_ = true;

  // Signal the updated change to the senders.
  for (const auto& sender : senders_)
    sender->internal()->SetTransceiverAsStopped();

  // 3. Set transceiver.[[Receptive]] to false.
  // 4. Set transceiver.[[CurrentDirection]] to null.
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

  // 6. to 8.
  RTCError result;
  if (media_type_ == cricket::MEDIA_TYPE_AUDIO) {
    std::vector<cricket::AudioCodec> recv_codecs, send_codecs;
    channel_manager_->GetSupportedAudioReceiveCodecs(&recv_codecs);
    channel_manager_->GetSupportedAudioSendCodecs(&send_codecs);

    result = VerifyCodecPreferences(codecs, send_codecs, recv_codecs);
  } else if (media_type_ == cricket::MEDIA_TYPE_VIDEO) {
    std::vector<cricket::VideoCodec> recv_codecs, send_codecs;
    channel_manager_->GetSupportedVideoReceiveCodecs(&recv_codecs);
    channel_manager_->GetSupportedVideoSendCodecs(&send_codecs);

    result = VerifyCodecPreferences(codecs, send_codecs, recv_codecs);
  }

  if (result.ok()) {
    codec_preferences_ = codecs;
  }

  return result;
}

std::vector<RtpHeaderExtensionCapability>
RtpTransceiver::HeaderExtensionsToOffer() const {
  return header_extensions_to_offer_;
}

std::vector<RtpHeaderExtensionCapability>
RtpTransceiver::HeaderExtensionsNegotiated() const {
  RTC_DCHECK_RUN_ON(thread_);
  std::vector<RtpHeaderExtensionCapability> result;
  for (const auto& ext : negotiated_header_extensions_) {
    result.emplace_back(ext.uri, ext.id, RtpTransceiverDirection::kSendRecv);
  }
  return result;
}

RTCError RtpTransceiver::SetOfferedRtpHeaderExtensions(
    rtc::ArrayView<const RtpHeaderExtensionCapability>
        header_extensions_to_offer) {
  for (const auto& entry : header_extensions_to_offer) {
    // Handle unsupported requests for mandatory extensions as per
    // https://w3c.github.io/webrtc-extensions/#rtcrtptransceiver-interface.
    // Note:
    // - We do not handle setOfferedRtpHeaderExtensions algorithm step 2.1,
    //   this has to be checked on a higher level. We naturally error out
    //   in the handling of Step 2.2 if an unset URI is encountered.

    // Step 2.2.
    // Handle unknown extensions.
    auto it = std::find_if(
        header_extensions_to_offer_.begin(), header_extensions_to_offer_.end(),
        [&entry](const auto& offered) { return entry.uri == offered.uri; });
    if (it == header_extensions_to_offer_.end()) {
      return RTCError(RTCErrorType::UNSUPPORTED_PARAMETER,
                      "Attempted to modify an unoffered extension.");
    }

    // Step 2.4-2.5.
    // - Use of the transceiver interface indicates unified plan is in effect,
    //   hence the MID extension needs to be enabled.
    // - Also handle the mandatory video orientation extensions.
    if ((entry.uri == RtpExtension::kMidUri ||
         entry.uri == RtpExtension::kVideoRotationUri) &&
        entry.direction != RtpTransceiverDirection::kSendRecv) {
      return RTCError(RTCErrorType::INVALID_MODIFICATION,
                      "Attempted to stop a mandatory extension.");
    }
  }

  // Apply mutation after error checking.
  for (const auto& entry : header_extensions_to_offer) {
    auto it = std::find_if(
        header_extensions_to_offer_.begin(), header_extensions_to_offer_.end(),
        [&entry](const auto& offered) { return entry.uri == offered.uri; });
    it->direction = entry.direction;
  }

  return RTCError::OK();
}

void RtpTransceiver::OnNegotiationUpdate(
    SdpType sdp_type,
    const cricket::MediaContentDescription* content) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(content);
  if (sdp_type == SdpType::kAnswer)
    negotiated_header_extensions_ = content->rtp_header_extensions();
}

void RtpTransceiver::SetPeerConnectionClosed() {
  is_pc_closed_ = true;
}

}  // namespace webrtc
