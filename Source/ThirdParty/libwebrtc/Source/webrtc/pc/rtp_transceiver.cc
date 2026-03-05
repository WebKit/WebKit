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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/scalability_mode.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "pc/channel.h"
#include "pc/channel_interface.h"
#include "pc/codec_vendor.h"
#include "pc/connection_context.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_receiver_proxy.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_sender_proxy.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace {

bool HasAnyMediaCodec(const std::vector<RtpCodecCapability>& codecs) {
  return absl::c_any_of(codecs, [](const RtpCodecCapability& codec) {
    return codec.IsMediaCodec();
  });
}

RTCError VerifyCodecPreferences(const std::vector<RtpCodecCapability>& codecs,
                                const std::vector<Codec>& send_codecs,
                                const std::vector<Codec>& recv_codecs) {
  // `codec_capabilities` is the union of `send_codecs` and `recv_codecs`.
  std::vector<Codec> codec_capabilities;
  codec_capabilities.reserve(send_codecs.size() + recv_codecs.size());
  codec_capabilities.insert(codec_capabilities.end(), send_codecs.begin(),
                            send_codecs.end());
  codec_capabilities.insert(codec_capabilities.end(), recv_codecs.begin(),
                            recv_codecs.end());
  // If a media codec is not recognized from `codec_capabilities`, throw
  // InvalidModificationError.
  if (!absl::c_all_of(codecs, [&codec_capabilities](
                                  const RtpCodecCapability& codec) {
        return !codec.IsMediaCodec() ||
               absl::c_any_of(codec_capabilities,
                              [&codec](const Codec& codec_capability) {
                                return IsSameRtpCodec(codec_capability, codec);
                              });
      })) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "Invalid codec preferences: Missing codec from codec "
                         "capabilities.");
  }
  // If `codecs` only contains entries for RTX, RED, FEC or Comfort Noise, throw
  // InvalidModificationError.
  if (!HasAnyMediaCodec(codecs)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Invalid codec preferences: codec list must have a non "
        "RTX, RED, FEC or Comfort Noise entry.");
  }
  return RTCError::OK();
}

TaskQueueBase* GetCurrentTaskQueueOrThread() {
  TaskQueueBase* current = TaskQueueBase::Current();
  if (!current)
    current = ThreadManager::Instance()->CurrentThread();
  return current;
}

}  // namespace

RtpTransceiver::RtpTransceiver(const Environment& env,
                               MediaType media_type,
                               ConnectionContext* context,
                               CodecLookupHelper* codec_lookup_helper)
    : env_(env),
      thread_(GetCurrentTaskQueueOrThread()),
      unified_plan_(false),
      media_type_(media_type),
      context_(context),
      codec_lookup_helper_(codec_lookup_helper) {
  RTC_DCHECK(media_type == MediaType::AUDIO || media_type == MediaType::VIDEO);
  RTC_DCHECK(context_);
  RTC_DCHECK(context_->is_configured_for_media());
  RTC_DCHECK(codec_lookup_helper_);
}

RtpTransceiver::RtpTransceiver(
    const Environment& env,
    scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
    scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>> receiver,
    ConnectionContext* context,
    CodecLookupHelper* codec_lookup_helper,
    std::vector<RtpHeaderExtensionCapability> header_extensions_to_negotiate,
    std::function<void()> on_negotiation_needed)
    : env_(env),
      thread_(GetCurrentTaskQueueOrThread()),
      unified_plan_(true),
      media_type_(sender->media_type()),
      context_(context),
      codec_lookup_helper_(codec_lookup_helper),
      header_extensions_to_negotiate_(
          std::move(header_extensions_to_negotiate)),
      on_negotiation_needed_(std::move(on_negotiation_needed)) {
  RTC_DCHECK(context_);
  RTC_DCHECK(context_->is_configured_for_media());
  RTC_DCHECK(media_type_ == MediaType::AUDIO ||
             media_type_ == MediaType::VIDEO);
  RTC_DCHECK_EQ(sender->media_type(), receiver->media_type());
  sender->internal()->SetSendCodecs(
      sender->media_type() == MediaType::VIDEO
          ? codec_vendor().video_send_codecs().codecs()
          : codec_vendor().audio_send_codecs().codecs());
  senders_.push_back(sender);
  receivers_.push_back(receiver);

  // Set default header extensions depending on whether simulcast/SVC is used.
  RtpParameters parameters = sender->internal()->GetParametersInternal();
  bool uses_simulcast = parameters.encodings.size() > 1;
  bool uses_svc = !parameters.encodings.empty() &&
                  parameters.encodings[0].scalability_mode.has_value() &&
                  parameters.encodings[0].scalability_mode !=
                      ScalabilityModeToString(ScalabilityMode::kL1T1);
  if (uses_simulcast || uses_svc) {
    // Enable DD and VLA extensions, can be deactivated by the API.
    // Skip this if the GFD extension was enabled via field trial
    // for backward compability reasons.
    bool uses_gfd =
        absl::c_find_if(
            header_extensions_to_negotiate_,
            [](const RtpHeaderExtensionCapability& ext) {
              return ext.uri == RtpExtension::kGenericFrameDescriptorUri00 &&
                     ext.direction != RtpTransceiverDirection::kStopped;
            }) != header_extensions_to_negotiate_.end();
    if (!uses_gfd) {
      for (RtpHeaderExtensionCapability& ext :
           header_extensions_to_negotiate_) {
        if (ext.uri == RtpExtension::kVideoLayersAllocationUri ||
            ext.uri == RtpExtension::kDependencyDescriptorUri) {
          ext.direction = RtpTransceiverDirection::kSendRecv;
        }
      }
    }
  }
}

RtpTransceiver::~RtpTransceiver() {
  // TODO(tommi): On Android, when running PeerConnectionClientTest (e.g.
  // PeerConnectionClientTest#testCameraSwitch), the instance doesn't get
  // deleted on `thread_`. See if we can fix that.
  if (!stopped_) {
    RTC_DCHECK_RUN_ON(thread_);
    StopInternal();
  }

  RTC_CHECK(!channel_) << "Missing call to ClearChannel?";
}

RTCError RtpTransceiver::CreateChannel(
    absl::string_view mid,
    Call* call_ptr,
    const MediaConfig& media_config,
    bool srtp_required,
    CryptoOptions crypto_options,
    const AudioOptions& audio_options,
    const VideoOptions& video_options,
    VideoBitrateAllocatorFactory* video_bitrate_allocator_factory,
    std::function<RtpTransportInternal*(absl::string_view)> transport_lookup) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!channel());

  std::unique_ptr<ChannelInterface> new_channel;
  if (media_type() == MediaType::AUDIO) {
    // TODO(bugs.webrtc.org/11992): CreateVideoChannel internally switches to
    // the worker thread. We shouldn't be using the `call_ptr_` hack here but
    // simply be on the worker thread and use `call_` (update upstream code).
    RTC_DCHECK(call_ptr);
    // TODO(bugs.webrtc.org/11992): Remove this workaround after updates in
    // PeerConnection and add the expectation that we're already on the right
    // thread.
    context()->worker_thread()->BlockingCall([&] {
      RTC_DCHECK_RUN_ON(context()->worker_thread());

      AudioCodecPairId codec_pair_id = AudioCodecPairId::Create();

      std::unique_ptr<VoiceMediaSendChannelInterface> media_send_channel =
          media_engine()->voice().CreateSendChannel(
              env_, call_ptr, media_config, audio_options, crypto_options,
              codec_pair_id);
      std::unique_ptr<VoiceMediaReceiveChannelInterface> media_receive_channel =
          media_engine()->voice().CreateReceiveChannel(
              env_, call_ptr, media_config, audio_options, crypto_options,
              codec_pair_id);
      // Note that this is safe because both sending and
      // receiving channels will be deleted at the same time.
      media_send_channel->SetSsrcListChangedCallback(
          [receive_channel =
               media_receive_channel.get()](const std::set<uint32_t>& choices) {
            receive_channel->ChooseReceiverReportSsrc(choices);
          });

      new_channel = std::make_unique<VoiceChannel>(
          context()->worker_thread(), context()->network_thread(),
          context()->signaling_thread(), std::move(media_send_channel),
          std::move(media_receive_channel), mid, srtp_required, crypto_options,
          context()->ssrc_generator());
    });
  } else {
    RTC_DCHECK_EQ(MediaType::VIDEO, media_type());

    // TODO(bugs.webrtc.org/11992): CreateVideoChannel internally switches to
    // the worker thread. We shouldn't be using the `call_ptr_` hack here but
    // simply be on the worker thread and use `call_` (update upstream code).
    context()->worker_thread()->BlockingCall([&] {
      RTC_DCHECK_RUN_ON(context()->worker_thread());

      std::unique_ptr<VideoMediaSendChannelInterface> media_send_channel =
          media_engine()->video().CreateSendChannel(
              env_, call_ptr, media_config, video_options, crypto_options,
              video_bitrate_allocator_factory);
      std::unique_ptr<VideoMediaReceiveChannelInterface> media_receive_channel =
          media_engine()->video().CreateReceiveChannel(
              env_, call_ptr, media_config, video_options, crypto_options);
      // Note that this is safe because both sending and
      // receiving channels will be deleted at the same time.
      media_send_channel->SetSsrcListChangedCallback(
          [receive_channel =
               media_receive_channel.get()](const std::set<uint32_t>& choices) {
            receive_channel->ChooseReceiverReportSsrc(choices);
          });

      new_channel = std::make_unique<VideoChannel>(
          context()->worker_thread(), context()->network_thread(),
          context()->signaling_thread(), std::move(media_send_channel),
          std::move(media_receive_channel), mid, srtp_required, crypto_options,
          context()->ssrc_generator());
    });
  }
  SetChannel(std::move(new_channel), transport_lookup);
  return RTCError::OK();
}

void RtpTransceiver::SetChannel(
    std::unique_ptr<ChannelInterface> channel,
    std::function<RtpTransportInternal*(const std::string&)> transport_lookup) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(channel);
  RTC_DCHECK(transport_lookup);
  RTC_DCHECK(!channel_);
  // Cannot set a channel on a stopped transceiver.
  if (stopped_) {
    return;
  }

  RTC_LOG_THREAD_BLOCK_COUNT();

  RTC_DCHECK_EQ(media_type(), channel->media_type());
  signaling_thread_safety_ = PendingTaskSafetyFlag::Create();
  channel_ = std::move(channel);

  // An alternative to this, could be to require SetChannel to be called
  // on the network thread. The channel object operates for the most part
  // on the network thread, as part of its initialization being on the network
  // thread is required, so setting a channel object as part of the construction
  // (without thread hopping) might be the more efficient thing to do than
  // how SetChannel works today.
  // Similarly, if the channel() accessor is limited to the network thread, that
  // helps with keeping the channel implementation requirements being met and
  // avoids synchronization for accessing the pointer or network related state.
  context()->network_thread()->BlockingCall([&]() {
    channel_->SetRtpTransport(transport_lookup(channel_->mid()));
    channel_->SetFirstPacketReceivedCallback(
        [thread = thread_, flag = signaling_thread_safety_, this]() mutable {
          thread->PostTask(
              SafeTask(std::move(flag), [this]() { OnFirstPacketReceived(); }));
        });
    channel_->SetFirstPacketSentCallback(
        [thread = thread_, flag = signaling_thread_safety_, this]() mutable {
          thread->PostTask(
              SafeTask(std::move(flag), [this]() { OnFirstPacketSent(); }));
        });
  });
  PushNewMediaChannel();

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(2);
}

void RtpTransceiver::ClearChannel() {
  RTC_DCHECK_RUN_ON(thread_);

  if (!channel_) {
    return;
  }

  RTC_LOG_THREAD_BLOCK_COUNT();

  signaling_thread_safety_->SetNotAlive();
  signaling_thread_safety_ = nullptr;

  context()->network_thread()->BlockingCall([&]() {
    channel_->SetFirstPacketReceivedCallback(nullptr);
    channel_->SetFirstPacketSentCallback(nullptr);
    channel_->SetRtpTransport(nullptr);
  });

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);
  DeleteChannel();

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(2);
}

void RtpTransceiver::PushNewMediaChannel() {
  RTC_DCHECK(channel_);
  if (senders_.empty() && receivers_.empty()) {
    return;
  }
  context()->worker_thread()->BlockingCall([&]() {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    // Push down the new media_channel.
    auto* media_send_channel = channel_->media_send_channel();
    for (const auto& sender : senders_) {
      sender->internal()->SetMediaChannel(media_send_channel);
    }

    auto* media_receive_channel = channel_->media_receive_channel();
    for (const auto& receiver : receivers_) {
      receiver->internal()->SetMediaChannel(media_receive_channel);
    }
  });
}

void RtpTransceiver::DeleteChannel() {
  RTC_DCHECK(channel_);
  // Ensure that channel_ is not reachable via transceiver, but is deleted
  // only after clearing the references in senders_ and receivers_.
  context()->worker_thread()->BlockingCall([&]() {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    auto channel_to_delete = std::move(channel_);
    // Clear the media channel reference from senders and receivers.
    for (const auto& sender : senders_) {
      sender->internal()->SetMediaChannel(nullptr);
    }
    for (const auto& receiver : receivers_) {
      receiver->internal()->SetMediaChannel(nullptr);
    }
    // The channel is destroyed here, on the worker thread as it needs to
    // be.
    channel_to_delete.reset();
    media_engine_ref_.reset();
  });
}

void RtpTransceiver::AddSender(
    scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(sender);
  RTC_DCHECK_EQ(media_type(), sender->media_type());
  RTC_DCHECK(!absl::c_linear_search(senders_, sender));

  std::vector<Codec> send_codecs =
      media_type() == MediaType::VIDEO
          ? codec_vendor().video_send_codecs().codecs()
          : codec_vendor().audio_send_codecs().codecs();
  sender->internal()->SetSendCodecs(send_codecs);
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
    scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>> receiver) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(receiver);
  RTC_DCHECK_EQ(media_type(), receiver->media_type());
  RTC_DCHECK(!absl::c_linear_search(receivers_, receiver));
  receivers_.push_back(receiver);
}

bool RtpTransceiver::RemoveReceiver(RtpReceiverInterface* receiver) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!unified_plan_);
  if (receiver) {
    RTC_DCHECK_EQ(media_type(), receiver->media_type());
  }
  auto it = absl::c_find(receivers_, receiver);
  if (it == receivers_.end()) {
    return false;
  }

  (*it)->internal()->Stop();
  context()->worker_thread()->BlockingCall([&]() {
    // `Stop()` will clear the receiver's pointer to the media channel.
    (*it)->internal()->SetMediaChannel(nullptr);
  });

  receivers_.erase(it);
  return true;
}

scoped_refptr<RtpSenderInternal> RtpTransceiver::sender_internal() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, senders_.size());
  return scoped_refptr<RtpSenderInternal>(senders_[0]->internal());
}

scoped_refptr<RtpReceiverInternal> RtpTransceiver::receiver_internal() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, receivers_.size());
  return scoped_refptr<RtpReceiverInternal>(receivers_[0]->internal());
}

MediaType RtpTransceiver::media_type() const {
  return media_type_;
}

std::optional<std::string> RtpTransceiver::mid() const {
  return mid_;
}

// RTC_RUN_ON(context()->worker_thread())
MediaEngineInterface* RtpTransceiver::media_engine() {
  if (!media_engine_ref_) {
    media_engine_ref_ =
        std::make_unique<ConnectionContext::MediaEngineReference>(
            scoped_refptr<ConnectionContext>(context_));
  }
  return media_engine_ref_->media_engine();
}

void RtpTransceiver::OnFirstPacketReceived() {
  for (const auto& receiver : receivers_) {
    receiver->internal()->NotifyFirstPacketReceived();
  }
}

void RtpTransceiver::OnFirstPacketSent() {
  for (const auto& sender : senders_) {
    sender->internal()->NotifyFirstPacketSent();
  }
}

scoped_refptr<RtpSenderInterface> RtpTransceiver::sender() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, senders_.size());
  return senders_[0];
}

scoped_refptr<RtpReceiverInterface> RtpTransceiver::receiver() const {
  RTC_DCHECK(unified_plan_);
  RTC_CHECK_EQ(1u, receivers_.size());
  return receivers_[0];
}

void RtpTransceiver::set_current_direction(RtpTransceiverDirection direction) {
  if (current_direction_ == direction)
    return;
  RTC_LOG(LS_INFO) << "Changing transceiver (MID=" << mid_.value_or("<not set>")
                   << ") current direction from "
                   << (current_direction_ ? RtpTransceiverDirectionToString(
                                                *current_direction_)
                                          : "<not set>")
                   << " to " << RtpTransceiverDirectionToString(direction);
  current_direction_ = direction;
  if (RtpTransceiverDirectionHasSend(*current_direction_)) {
    has_ever_been_used_to_send_ = true;
  }
}

void RtpTransceiver::set_fired_direction(
    std::optional<RtpTransceiverDirection> direction) {
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
    return RtpTransceiverDirection::kStopped;

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

std::optional<RtpTransceiverDirection> RtpTransceiver::current_direction()
    const {
  if (unified_plan_ && stopped())
    return RtpTransceiverDirection::kStopped;

  return current_direction_;
}

std::optional<RtpTransceiverDirection> RtpTransceiver::fired_direction() const {
  return fired_direction_;
}

void RtpTransceiver::StopSendingAndReceiving() {
  // 1. Let sender be transceiver.[[Sender]].
  // 2. Let receiver be transceiver.[[Receiver]].
  //
  // 3. Stop sending media with sender.
  //
  RTC_DCHECK_RUN_ON(thread_);

  // 4. Send an RTCP BYE for each RTP stream that was being sent by sender, as
  // specified in [RFC3550].
  for (const auto& sender : senders_)
    sender->internal()->Stop();

  // Signal to receiver sources that we're stopping.
  for (const auto& receiver : receivers_)
    receiver->internal()->Stop();

  context()->worker_thread()->BlockingCall([&]() {
    // 5 Stop receiving media with receiver.
    for (const auto& receiver : receivers_)
      receiver->internal()->SetMediaChannel(nullptr);
  });

  stopping_ = true;
  direction_ = RtpTransceiverDirection::kInactive;
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
  current_direction_ = std::nullopt;
}

RTCError RtpTransceiver::SetCodecPreferences(
    ArrayView<RtpCodecCapability> codec_capabilities) {
  RTC_DCHECK(unified_plan_);
  // 3. If codecs is an empty list, set transceiver's [[PreferredCodecs]] slot
  // to codecs and abort these steps.
  if (codec_capabilities.empty()) {
    codec_preferences_.clear();
    sendrecv_codec_preferences_.clear();
    sendonly_codec_preferences_.clear();
    recvonly_codec_preferences_.clear();
    return RTCError::OK();
  }
  // 4. Remove any duplicate values in codecs.
  std::vector<RtpCodecCapability> codecs;
  absl::c_remove_copy_if(codec_capabilities, std::back_inserter(codecs),
                         [&codecs](const RtpCodecCapability& codec) {
                           return absl::c_linear_search(codecs, codec);
                         });
  // TODO(https://crbug.com/webrtc/391530822): Move logic in
  // MediaSessionDescriptionFactory to this level.
  return UpdateCodecPreferencesCaches(codecs);
}

RTCError RtpTransceiver::UpdateCodecPreferencesCaches(
    const std::vector<RtpCodecCapability>& codecs) {
  // Get codec capabilities from media engine.
  std::vector<Codec> send_codecs, recv_codecs;
  if (media_type_ == MediaType::AUDIO) {
    send_codecs = codec_vendor().audio_send_codecs().codecs();
    recv_codecs = codec_vendor().audio_recv_codecs().codecs();
  } else if (media_type_ == MediaType::VIDEO) {
    send_codecs = codec_vendor().video_send_codecs().codecs();
    recv_codecs = codec_vendor().video_recv_codecs().codecs();
  }
  RTCError error = VerifyCodecPreferences(codecs, send_codecs, recv_codecs);
  if (!error.ok()) {
    return error;
  }
  codec_preferences_ = codecs;
  // Update the filtered views of `codec_preferences_` so that we don't have
  // to query codec capabilities when calling filtered_codec_preferences() or
  // every time the direction changes.
  sendrecv_codec_preferences_.clear();
  sendonly_codec_preferences_.clear();
  recvonly_codec_preferences_.clear();
  for (const RtpCodecCapability& codec : codec_preferences_) {
    if (!codec.IsMediaCodec()) {
      // Non-media codecs don't need to be filtered at this level.
      sendrecv_codec_preferences_.push_back(codec);
      sendonly_codec_preferences_.push_back(codec);
      recvonly_codec_preferences_.push_back(codec);
      continue;
    }
    // Is this a send codec, receive codec or both?
    bool is_send_codec =
        absl::c_any_of(send_codecs, [&codec](const Codec& send_codec) {
          return IsSameRtpCodecIgnoringLevel(send_codec, codec);
        });
    bool is_recv_codec =
        absl::c_any_of(recv_codecs, [&codec](const Codec& recv_codec) {
          return IsSameRtpCodecIgnoringLevel(recv_codec, codec);
        });
    // The codec being neither for sending or receving is not possible because
    // of prior validation by VerifyCodecPreferences().
    RTC_CHECK(is_send_codec || is_recv_codec);
    if (is_send_codec && is_recv_codec) {
      sendrecv_codec_preferences_.push_back(codec);
    }
    if (is_send_codec) {
      sendonly_codec_preferences_.push_back(codec);
    }
    if (is_recv_codec) {
      recvonly_codec_preferences_.push_back(codec);
    }
  }
  // If filtering results in an empty list this is the same as not having any
  // preferences.
  if (!HasAnyMediaCodec(sendrecv_codec_preferences_)) {
    sendrecv_codec_preferences_.clear();
  }
  if (!HasAnyMediaCodec(sendonly_codec_preferences_)) {
    sendonly_codec_preferences_.clear();
  }
  if (!HasAnyMediaCodec(recvonly_codec_preferences_)) {
    recvonly_codec_preferences_.clear();
  }
  return RTCError::OK();
}

std::vector<RtpCodecCapability> RtpTransceiver::codec_preferences() const {
  return codec_preferences_;
}

std::vector<RtpCodecCapability> RtpTransceiver::filtered_codec_preferences()
    const {
  switch (direction_) {
    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kInactive:
    case RtpTransceiverDirection::kStopped:
      return sendrecv_codec_preferences_;
    case RtpTransceiverDirection::kSendOnly:
      return sendonly_codec_preferences_;
    case RtpTransceiverDirection::kRecvOnly:
      return recvonly_codec_preferences_;
  }
  return codec_preferences_;
}

std::vector<RtpHeaderExtensionCapability>
RtpTransceiver::GetHeaderExtensionsToNegotiate() const {
  RTC_DCHECK_RUN_ON(thread_);
  return header_extensions_to_negotiate_;
}

std::vector<RtpHeaderExtensionCapability> ModifyCapabilitiesAccordingToHeaders(
    const std::vector<RtpHeaderExtensionCapability>& old_values,
    const std::vector<RtpExtension>& extension_list) {
  std::vector<RtpHeaderExtensionCapability> result;
  result.reserve(old_values.size());
  // Create new capability objects that start as a copy of the old values.
  for (RtpHeaderExtensionCapability capability : old_values) {
    auto negotiated = absl::c_find_if(
        extension_list, [&capability](const RtpExtension& negotiated) {
          return negotiated.uri == capability.uri;
        });
    // TODO(bugs.webrtc.org/7477): extend when header extensions support
    // direction.
    if (negotiated != extension_list.end()) {
      capability.direction = RtpTransceiverDirection::kSendRecv;
      capability.preferred_id = negotiated->id;
      capability.preferred_encrypt = negotiated->encrypt;
    } else {
      capability.direction = RtpTransceiverDirection::kStopped;
    }
    result.push_back(capability);
  }
  return result;
}
std::vector<RtpHeaderExtensionCapability>
RtpTransceiver::GetNegotiatedHeaderExtensions() const {
  RTC_DCHECK_RUN_ON(thread_);
  return ModifyCapabilitiesAccordingToHeaders(header_extensions_to_negotiate_,
                                              negotiated_header_extensions_);
}

std::vector<RtpHeaderExtensionCapability>
RtpTransceiver::GetOfferedAndImplementedHeaderExtensions(
    const MediaContentDescription* content) const {
  RTC_DCHECK_RUN_ON(thread_);
  return ModifyCapabilitiesAccordingToHeaders(header_extensions_to_negotiate_,
                                              content->rtp_header_extensions());
}

// Helper function to determine mandatory-to-negotiate extensions.
// See https://www.rfc-editor.org/rfc/rfc8834#name-header-extensions
// and https://w3c.github.io/webrtc-extensions/#rtcrtptransceiver-interface
// Since BUNDLE is offered by default, MID is mandatory and can not be turned
// off via this API.
bool IsMandatoryHeaderExtension(const std::string& uri) {
  return uri == RtpExtension::kMidUri;
}

RTCError RtpTransceiver::SetHeaderExtensionsToNegotiate(
    ArrayView<const RtpHeaderExtensionCapability> header_extensions) {
  RTC_DCHECK_RUN_ON(thread_);
  // https://w3c.github.io/webrtc-extensions/#dom-rtcrtptransceiver-setheaderextensionstonegotiate
  if (header_extensions.size() != header_extensions_to_negotiate_.size()) {
    return RTCError(RTCErrorType::INVALID_MODIFICATION,
                    "Size of extensions to negotiate does not match.");
  }
  // For each index i of extensions, run the following steps: ...
  for (size_t i = 0; i < header_extensions.size(); i++) {
    const auto& extension = header_extensions[i];
    if (extension.uri != header_extensions_to_negotiate_[i].uri) {
      return RTCError(RTCErrorType::INVALID_MODIFICATION,
                      "Reordering extensions is not allowed.");
    }
    if (IsMandatoryHeaderExtension(extension.uri) &&
        extension.direction != RtpTransceiverDirection::kSendRecv) {
      return RTCError(RTCErrorType::INVALID_MODIFICATION,
                      "Attempted to stop a mandatory extension.");
    }

    // TODO(bugs.webrtc.org/7477): Currently there are no recvonly extensions so
    // this can not be checked: "When there exists header extension capabilities
    // that have directions other than kSendRecv, restrict extension.direction
    // as to not exceed that capability."
  }

  // Apply mutation after error checking.
  for (size_t i = 0; i < header_extensions.size(); i++) {
    header_extensions_to_negotiate_[i].direction =
        header_extensions[i].direction;
  }

  return RTCError::OK();
}

void RtpTransceiver::OnNegotiationUpdate(
    SdpType sdp_type,
    const MediaContentDescription* content) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(content);
  if (sdp_type == SdpType::kAnswer || sdp_type == SdpType::kPrAnswer) {
    negotiated_header_extensions_ = content->rtp_header_extensions();
    if (!env_.field_trials().IsDisabled(
            "WebRTC-HeaderExtensionNegotiateMemory")) {
      header_extensions_to_negotiate_ = GetNegotiatedHeaderExtensions();
    }
  } else if (sdp_type == SdpType::kOffer) {
    if (!env_.field_trials().IsDisabled(
            "WebRTC-HeaderExtensionNegotiateMemory")) {
      header_extensions_for_rollback_ = header_extensions_to_negotiate_;
      header_extensions_to_negotiate_ =
          GetOfferedAndImplementedHeaderExtensions(content);
    }
  } else if (sdp_type == SdpType::kRollback) {
    if (!env_.field_trials().IsDisabled(
            "WebRTC-HeaderExtensionNegotiateMemory")) {
      RTC_CHECK(!header_extensions_for_rollback_.empty());
      header_extensions_to_negotiate_ = header_extensions_for_rollback_;
    }
  }
}

void RtpTransceiver::SetPeerConnectionClosed() {
  is_pc_closed_ = true;
}

}  // namespace webrtc
