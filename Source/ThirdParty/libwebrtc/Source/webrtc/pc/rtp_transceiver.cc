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
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
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
#include "call/call.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "pc/audio_rtp_receiver.h"
#include "pc/channel.h"
#include "pc/channel_interface.h"
#include "pc/codec_vendor.h"
#include "pc/connection_context.h"
#include "pc/dtls_transport.h"
#include "pc/legacy_stats_collector_interface.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_receiver_proxy.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_sender_proxy.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "pc/video_rtp_receiver.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/plan_b_only.h"
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
    return LOG_ERROR(RTCError::InvalidModification()
                     << "Invalid codec preferences: Missing codec from codec "
                        "capabilities.");
  }
  // If `codecs` only contains entries for RTX, RED, FEC or Comfort Noise, throw
  // InvalidModificationError.
  if (!HasAnyMediaCodec(codecs)) {
    return LOG_ERROR(RTCError::InvalidModification()
                     << "Invalid codec preferences: codec list must have a non "
                        "RTX, RED, FEC or Comfort Noise entry.");
  }
  return RTCError::OK();
}

// Set default header extensions depending on whether simulcast/SVC is used.
void ConfigureExtraVideoHeaderExtensions(
    const std::vector<RtpEncodingParameters>& encodings,
    std::vector<RtpHeaderExtensionCapability>& extensions) {
  bool uses_simulcast = encodings.size() > 1;
  bool uses_svc = !encodings.empty() &&
                  encodings[0].scalability_mode.has_value() &&
                  encodings[0].scalability_mode !=
                      ScalabilityModeToString(ScalabilityMode::kL1T1);
  if (!uses_simulcast && !uses_svc)
    return;

  // Enable DD and VLA extensions, can be deactivated by the API. Skip this if
  // the GFD extension was enabled via field trial for backward compatibility
  // reasons.
  bool uses_frame_descriptor =
      absl::c_any_of(extensions, [](const RtpHeaderExtensionCapability& ext) {
        return ext.uri == RtpExtension::kGenericFrameDescriptorUri00 &&
               ext.direction != RtpTransceiverDirection::kStopped;
      });
  if (!uses_frame_descriptor) {
    for (RtpHeaderExtensionCapability& ext : extensions) {
      if (ext.uri == RtpExtension::kVideoLayersAllocationUri ||
          ext.uri == RtpExtension::kDependencyDescriptorUri) {
        ext.direction = RtpTransceiverDirection::kSendRecv;
      }
    }
  }
}

void ConfigureSendCodecs(CodecVendor& codec_vendor,
                         MediaType media_type,
                         RtpSenderInternal* sender) {
  sender->SetSendCodecs(media_type == MediaType::VIDEO
                            ? codec_vendor.video_send_codecs().codecs()
                            : codec_vendor.audio_send_codecs().codecs());
}

scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> CreateSender(
    MediaType media_type,
    const Environment& env,
    ConnectionContext* context,
    LegacyStatsCollectorInterface* legacy_stats,
    RtpSenderBase::SetStreamsObserver* set_streams_observer,
    absl::string_view sender_id,
    MediaSendChannelInterface* media_send_channel) {
  if (media_type == MediaType::AUDIO) {
    return RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
        context->signaling_thread(),
        AudioRtpSender::Create(
            env, context->signaling_thread(), context->worker_thread(),
            sender_id, legacy_stats, set_streams_observer,
            static_cast<VoiceMediaSendChannelInterface*>(media_send_channel)));
  }
  RTC_DCHECK_EQ(media_type, MediaType::VIDEO);
  return RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
      context->signaling_thread(),
      VideoRtpSender::Create(
          env, context->signaling_thread(), context->worker_thread(), sender_id,
          set_streams_observer,
          static_cast<VideoMediaSendChannelInterface*>(media_send_channel)));
}

void ConfigureSender(
    scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>& sender,
    MediaStreamTrackInterface* track,
    const std::vector<std::string>& stream_ids,
    const std::vector<RtpEncodingParameters>& send_encodings,
    CodecVendor& codec_vendor) {
  bool set_track_succeeded = sender->SetTrack(track);
  RTC_DCHECK(set_track_succeeded);
  auto* internal = sender->internal();
  internal->set_stream_ids(stream_ids);
  internal->set_init_send_encodings(send_encodings);
  ConfigureSendCodecs(codec_vendor, sender->media_type(), internal);
}

template <typename RtpReceiverT, typename ReceiveInterface>
scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
CreateReceiverOfType(Thread* signaling_thread,
                     Thread* worker_thread,
                     absl::string_view receiver_id,
                     MediaReceiveChannelInterface* receive_channel) {
  return RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
      signaling_thread, worker_thread,
      make_ref_counted<RtpReceiverT>(
          worker_thread, receiver_id, std::vector<std::string>(),
          static_cast<ReceiveInterface*>(receive_channel)));
}

scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>> CreateReceiver(
    MediaType media_type,
    Thread* signaling_thread,
    Thread* worker_thread,
    absl::string_view receiver_id,
    MediaReceiveChannelInterface* receive_channel) {
  if (media_type == MediaType::AUDIO) {
    return CreateReceiverOfType<AudioRtpReceiver,
                                VoiceMediaReceiveChannelInterface>(
        signaling_thread, worker_thread, receiver_id, receive_channel);
  }
  RTC_DCHECK_EQ(media_type, MediaType::VIDEO);
  return CreateReceiverOfType<VideoRtpReceiver,
                              VideoMediaReceiveChannelInterface>(
      signaling_thread, worker_thread, receiver_id, receive_channel);
}

std::pair<std::unique_ptr<MediaSendChannelInterface>,
          std::unique_ptr<MediaReceiveChannelInterface>>
CreateMediaContentChannels(
    MediaType media_type,
    const Environment& env,
    MediaEngineInterface* media_engine,
    Call* call,
    const MediaConfig& media_config,
    const AudioOptions& audio_options,
    const VideoOptions& video_options,
    const CryptoOptions& crypto_options,
    VideoBitrateAllocatorFactory* video_bitrate_allocator_factory) {
  if (media_type == MediaType::AUDIO) {
    return {media_engine->voice().CreateSendChannel(
                env, call, media_config, audio_options, crypto_options),
            media_engine->voice().CreateReceiveChannel(
                env, call, media_config, audio_options, crypto_options)};
  }
  return {media_engine->video().CreateSendChannel(
              env, call, media_config, video_options, crypto_options,
              video_bitrate_allocator_factory),
          media_engine->video().CreateReceiveChannel(
              env, call, media_config, video_options, crypto_options)};
}

// Helper template to wrap the construction of either a VoiceChannel
// or VideoChannel object from a given send and receive channel objects.
template <typename Channel, typename Send, typename Receive>
std::unique_ptr<ChannelInterface> CreateMediaChannel(
    ConnectionContext* context,
    std::unique_ptr<MediaSendChannelInterface>& send,
    std::unique_ptr<MediaReceiveChannelInterface>& receive,
    absl::string_view mid,
    bool srtp_required,
    CryptoOptions crypto_options) {
  return std::make_unique<Channel>(
      context->worker_thread(), context->network_thread(),
      context->signaling_thread(),
      std::unique_ptr<Send>(static_cast<Send*>(send.release())),
      std::unique_ptr<Receive>(static_cast<Receive*>(receive.release())), mid,
      srtp_required, crypto_options, context->ssrc_generator());
}

std::vector<absl::AnyInvocable<void() &&>> DetachAndGetStopTasksForSenders(
    std::vector<scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>&
        senders) {
  std::vector<absl::AnyInvocable<void() &&>> tasks;
  for (const auto& sender : senders) {
    auto task = sender->internal()->DetachTrackAndGetStopTask();
    if (task)
      tasks.push_back(std::move(task));
  }
  return tasks;
}

}  // namespace

RtpTransceiver::RtpTransceiver(const Environment& env,
                               MediaType media_type,
                               ConnectionContext* context,
                               CodecLookupHelper* codec_lookup_helper,
                               LegacyStatsCollectorInterface* legacy_stats)
    : env_(env),
      thread_(context->signaling_thread()),
      unified_plan_(false),
      media_type_(media_type),
      network_thread_safety_(PendingTaskSafetyFlag::CreateAttachedToTaskQueue(
          true,
          context->network_thread())),
      context_(context),
      codec_lookup_helper_(codec_lookup_helper),
      legacy_stats_(legacy_stats) {
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
    absl::AnyInvocable<void()> on_negotiation_needed)
    : env_(env),
      thread_(context->signaling_thread()),
      unified_plan_(true),
      media_type_(sender->media_type()),
      network_thread_safety_(PendingTaskSafetyFlag::CreateAttachedToTaskQueue(
          true,
          context->network_thread())),
      context_(context),
      codec_lookup_helper_(codec_lookup_helper),
      legacy_stats_(nullptr),
      header_extensions_to_negotiate_(
          std::move(header_extensions_to_negotiate)),
      on_negotiation_needed_(std::move(on_negotiation_needed)) {
  RTC_DCHECK(context_);
  RTC_DCHECK(context_->is_configured_for_media());
  RTC_DCHECK(media_type_ == MediaType::AUDIO ||
             media_type_ == MediaType::VIDEO);
  RTC_DCHECK(codec_lookup_helper_);
  RTC_DCHECK_EQ(sender->media_type(), receiver->media_type());
  RTC_DCHECK_EQ(media_type_, sender->media_type());
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
  auto* sender_internal = sender->internal();
  senders_.push_back(std::move(sender));
  receivers_.push_back(std::move(receiver));
  if (media_type_ == MediaType::VIDEO) {
    ConfigureExtraVideoHeaderExtensions(
        sender_internal
            ->GetParametersInternal(/*may_use_cache*/ true,
                                    /*with_all_layers=*/false)
            .encodings,
        header_extensions_to_negotiate_);
  }
  ConfigureSendCodecs(codec_vendor(), media_type_, sender_internal);
}

RtpTransceiver::RtpTransceiver(
    const Environment& env,
    Call* call,
    const MediaConfig& media_config,
    absl::string_view sender_id,
    absl::string_view receiver_id,
    MediaType media_type,
    scoped_refptr<MediaStreamTrackInterface> track,
    const std::vector<std::string>& stream_ids,
    const std::vector<RtpEncodingParameters>& init_send_encodings,
    ConnectionContext* context,
    CodecLookupHelper* codec_lookup_helper,
    LegacyStatsCollectorInterface* legacy_stats,
    RtpSenderBase::SetStreamsObserver* set_streams_observer,
    const AudioOptions& audio_options,
    const VideoOptions& video_options,
    const CryptoOptions& crypto_options,
    VideoBitrateAllocatorFactory* video_bitrate_allocator_factory,
    std::vector<RtpHeaderExtensionCapability> header_extensions_to_negotiate,
    absl::AnyInvocable<void()> on_negotiation_needed)
    : env_(env),
      thread_(context->signaling_thread()),
      unified_plan_(true),
      media_type_(media_type),
      network_thread_safety_(PendingTaskSafetyFlag::CreateAttachedToTaskQueue(
          true,
          context->network_thread())),
      media_engine_ref_(nullptr),
      context_(context),
      codec_lookup_helper_(codec_lookup_helper),
      legacy_stats_(legacy_stats),
      set_streams_observer_(set_streams_observer),
      header_extensions_to_negotiate_(
          std::move(header_extensions_to_negotiate)),
      on_negotiation_needed_(std::move(on_negotiation_needed)) {
  RTC_DCHECK(context_);
  RTC_DCHECK(context_->is_configured_for_media());
  RTC_DCHECK(media_type_ == MediaType::AUDIO ||
             media_type_ == MediaType::VIDEO);
  RTC_LOG_THREAD_BLOCK_COUNT();
  if (media_type_ == MediaType::VIDEO) {
    ConfigureExtraVideoHeaderExtensions(init_send_encodings,
                                        header_extensions_to_negotiate_);
  }

  // This should be possible without a blocking call to the worker, perhaps done
  // asynchronously. At the moment this is complicated by the fact that
  // construction of the channels actually changes the settings of the engine.
  context_->worker_thread()->BlockingCall([&]() mutable {
    RTC_DCHECK_RUN_ON(this->context()->worker_thread());
    auto channels = CreateMediaContentChannels(
        media_type_, env_, media_engine(), call, media_config, audio_options,
        video_options, crypto_options, video_bitrate_allocator_factory);
    owned_send_channel_ = std::move(channels.first);
    owned_receive_channel_ = std::move(channels.second);
    senders_.push_back(CreateSender(media_type_, env_, context_, legacy_stats_,
                                    set_streams_observer_, sender_id,
                                    owned_send_channel_.get()));
  });

  ConfigureSender(senders_.back(), track.get(), stream_ids, init_send_encodings,
                  codec_vendor());

  receivers_.push_back(CreateReceiver(
      media_type_, context_->signaling_thread(), context_->worker_thread(),
      receiver_id.empty() ? CreateRandomUuid() : receiver_id,
      owned_receive_channel_.get()));
  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);
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
  RTC_DCHECK(!media_engine_ref_);
  RTC_DCHECK(!owned_send_channel_);
  RTC_DCHECK(!owned_receive_channel_);
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
    absl::AnyInvocable<RtpTransportInternal*(absl::string_view) &&>
        transport_lookup) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!channel_);
  RTC_DCHECK(!mid_ || mid_.value() == mid);
  RTC_DCHECK(!stopped_);

  mid_ = mid;

  std::unique_ptr<ChannelInterface> new_channel;
  // TODO(bugs.webrtc.org/11992): CreateVideoChannel internally switches to
  // the worker thread. We shouldn't be using the `call_ptr_` hack here but
  // simply be on the worker thread and use `call_` (update upstream code).
  context()->worker_thread()->BlockingCall([&] {
    RTC_DCHECK_RUN_ON(context()->worker_thread());

    std::unique_ptr<MediaSendChannelInterface> media_send_channel;
    std::unique_ptr<MediaReceiveChannelInterface> media_receive_channel;

    if (owned_send_channel_) {
      RTC_DCHECK(owned_receive_channel_);
      media_send_channel = std::move(owned_send_channel_);
      media_receive_channel = std::move(owned_receive_channel_);
      // Apply options to the voice channels for audio and send channel for
      // video. Note that the video options are primarily for sending.
      if (media_type() == MediaType::AUDIO) {
        media_send_channel->AsVoiceSendChannel()->SetOptions(audio_options);
        media_receive_channel->AsVoiceReceiveChannel()->SetOptions(
            audio_options);
      } else if (media_type() == MediaType::VIDEO) {
        media_send_channel->AsVideoSendChannel()->SetOptions(video_options);
      }
    } else {
      auto channels = CreateMediaContentChannels(
          media_type(), env_, media_engine(), call_ptr, media_config,
          audio_options, video_options, crypto_options,
          video_bitrate_allocator_factory);
      media_send_channel = std::move(channels.first);
      media_receive_channel = std::move(channels.second);
      SetMediaChannels(media_send_channel.get(), media_receive_channel.get());
    }
    // Note that this is safe because both sending and
    // receiving channels will be deleted at the same time.
    media_send_channel->SetSsrcListChangedCallback(
        [receive_channel =
             media_receive_channel.get()](const std::set<uint32_t>& choices) {
          receive_channel->ChooseReceiverReportSsrc(choices);
        });

    if (media_type() == MediaType::AUDIO) {
      new_channel =
          CreateMediaChannel<VoiceChannel, VoiceMediaSendChannelInterface,
                             VoiceMediaReceiveChannelInterface>(
              context(), media_send_channel, media_receive_channel, mid,
              srtp_required, crypto_options);
    } else {
      new_channel =
          CreateMediaChannel<VideoChannel, VideoMediaSendChannelInterface,
                             VideoMediaReceiveChannelInterface>(
              context(), media_send_channel, media_receive_channel, mid,
              srtp_required, crypto_options);
    }
  });
  return SetChannel(std::move(new_channel), std::move(transport_lookup),
                    /*set_media_channels=*/false);
}

RTCError RtpTransceiver::SetChannel(
    std::unique_ptr<ChannelInterface> channel,
    absl::AnyInvocable<RtpTransportInternal*(const std::string&) &&>
        transport_lookup,
    bool set_media_channels) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(channel);
  RTC_DCHECK(transport_lookup);
  RTC_DCHECK(!channel_);
  // Cannot set a channel on a stopped transceiver.
  if (stopped_) {
    return RTCError::InvalidState();
  }

  RTC_LOG_THREAD_BLOCK_COUNT();

  RTC_DCHECK_EQ(media_type(), channel->media_type());
  RTC_DCHECK(mid_ || channel->mid().empty());
  signaling_thread_safety_ = PendingTaskSafetyFlag::Create();
  channel_ = std::move(channel);
  transport_name_ = std::nullopt;

  // An alternative to this, could be to require SetChannel to be called
  // on the network thread. The channel object operates for the most part
  // on the network thread, as part of its initialization being on the network
  // thread is required, so setting a channel object as part of the construction
  // (without thread hopping) might be the more efficient thing to do than
  // how SetChannel works today.
  // Similarly, if the channel() accessor is limited to the network thread, that
  // helps with keeping the channel implementation requirements being met and
  // avoids synchronization for accessing the pointer or network related state.
  std::optional<std::string> transport_name;
  RTCError err = context()->network_thread()->BlockingCall(
      [&, flag = signaling_thread_safety_, channel = channel_.get()]() {
        RtpTransportInternal* transport =
            std::move(transport_lookup)(channel->mid());
        if (!channel->SetRtpTransport(transport)) {
          return RTCError::InvalidParameter()
                 << "Invalid transport for mid=" << channel->mid();
        }
        if (transport) {
          transport_name = transport->transport_name();
        }
        channel->SetFirstPacketReceivedCallback([thread = thread_, flag = flag,
                                                 this]() mutable {
          thread->PostTask(
              SafeTask(std::move(flag), [this]() { OnFirstPacketReceived(); }));
        });
        channel->SetFirstPacketSentCallback([thread = thread_, flag = flag,
                                             this]() mutable {
          thread->PostTask(
              SafeTask(std::move(flag), [this]() { OnFirstPacketSent(); }));
        });
        channel->SetPacketReceivedCallback_n([this, flag = flag]() {
          RTC_DCHECK_RUN_ON(context()->network_thread());
          OnPacketReceived(flag);
        });
        return RTCError::OK();
      });

  if (err.ok()) {
    transport_name_ = std::move(transport_name);
    if (set_media_channels) {
      PushNewMediaChannel();
    }
  }

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(2);

  return err;
}

absl::AnyInvocable<void() &&> RtpTransceiver::GetClearChannelNetworkTask() {
  RTC_DCHECK_RUN_ON(thread_);
  // GetClearChannelNetworkTask must be called before GetDeleteChannelWorkerTask
  // since that's where we clear the `channel_` pointer. Perhaps we should
  // combine these into one function to avoid an ordering mistake?

  if (!channel_) {
    RTC_DCHECK(!signaling_thread_safety_);
    return nullptr;
  }

  signaling_thread_safety_->SetNotAlive();
  signaling_thread_safety_ = nullptr;

  ChannelInterface* channel = channel_.get();
  return [channel, flag = network_thread_safety_] {
    flag->SetNotAlive();
    channel->SetFirstPacketReceivedCallback(nullptr);
    channel->SetFirstPacketSentCallback(nullptr);
    channel->SetPacketReceivedCallback_n(nullptr);
    channel->SetRtpTransport(nullptr);
  };
}

absl::AnyInvocable<void() &&> RtpTransceiver::GetDeleteChannelWorkerTask(
    bool stop_senders) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(signaling_thread_safety_ == nullptr)
      << "GetClearChannelNetworkTask() must be called first";

  if (!channel_) {
    return nullptr;
  }

  std::vector<absl::AnyInvocable<void() &&>> stop;
  if (stop_senders) {
    stop = DetachAndGetStopTasksForSenders(senders_);
  }

  transport_name_ = std::nullopt;

  // Ensure that channel_ is not reachable via the transceiver, but is deleted
  // only after clearing the references in senders_ and receivers_.
  return [this, channel = std::move(channel_), senders = senders_,
          receivers = receivers_, stop = std::move(stop)]() mutable {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    for (auto& task : stop) {
      std::move(task)();
    }
    ClearMediaChannelReferences();
    channel.reset();
  };
}

void RtpTransceiver::ClearChannel() {
  RTC_DCHECK_RUN_ON(thread_);
  if (!channel_) {
    return;
  }

  absl::AnyInvocable<void() &&> network_task = GetClearChannelNetworkTask();
  if (network_task) {
    context()->network_thread()->BlockingCall(
        [&] { std::move(network_task)(); });
  }

  absl::AnyInvocable<void() &&> worker_task =
      GetDeleteChannelWorkerTask(/*stop_senders=*/false);
  if (worker_task) {
    context()->worker_thread()->BlockingCall([&] { std::move(worker_task)(); });
  }
}

void RtpTransceiver::PushNewMediaChannel() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(channel_);
  if (senders_.empty() && receivers_.empty()) {
    return;
  }
  context()->worker_thread()->BlockingCall([&, channel = channel_.get()]() {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    SetMediaChannels(channel->media_send_channel(),
                     channel->media_receive_channel());
  });
}

// RTC_RUN_ON(context()->worker_thread());
void RtpTransceiver::SetMediaChannels(MediaSendChannelInterface* send,
                                      MediaReceiveChannelInterface* receive) {
  for (const auto& sender : senders_) {
    sender->internal()->SetMediaChannel(send);
  }
  for (const auto& receiver : receivers_) {
    receiver->internal()->SetMediaChannel(receive);
  }
}

// RTC_RUN_ON(context()->worker_thread());
void RtpTransceiver::ClearMediaChannelReferences() {
  SetMediaChannels(nullptr, nullptr);
  owned_send_channel_ = nullptr;
  owned_receive_channel_ = nullptr;
  media_engine_ref_ = nullptr;
}

PLAN_B_ONLY void RtpTransceiver::AddSenderPlanB(
    scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(sender);
  RTC_DCHECK_EQ(media_type(), sender->media_type());
  RTC_DCHECK(!absl::c_linear_search(senders_, sender));
  ConfigureSendCodecs(codec_vendor(), media_type(), sender->internal());
  senders_.push_back(sender);
}

PLAN_B_ONLY scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
RtpTransceiver::AddSenderPlanB(
    scoped_refptr<MediaStreamTrackInterface> track,
    absl::string_view sender_id,
    const std::vector<std::string>& stream_ids,
    const std::vector<RtpEncodingParameters>& send_encodings) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(media_type_ == MediaType::AUDIO ||
             media_type_ == MediaType::VIDEO);
  context_->worker_thread()->BlockingCall([&]() mutable {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    senders_.push_back(CreateSender(
        media_type_, env_, context_, legacy_stats_, set_streams_observer_,
        sender_id, channel_ ? channel_->media_send_channel() : nullptr));
  });
  ConfigureSender(senders_.back(), track.get(), stream_ids, send_encodings,
                  codec_vendor());
  return senders_.back();
}

PLAN_B_ONLY bool RtpTransceiver::RemoveSenderPlanB(RtpSenderInterface* sender) {
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK_EQ(media_type(), sender->media_type());
  auto it = absl::c_find(senders_, sender);
  if (it == senders_.end()) {
    return false;
  }
  (*it)->internal()->Stop();
  senders_.erase(it);
  return true;
}

PLAN_B_ONLY void RtpTransceiver::AddReceiverPlanB(
    scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>> receiver) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK(receiver);
  RTC_DCHECK_EQ(media_type(), receiver->media_type());
  RTC_DCHECK(!absl::c_linear_search(receivers_, receiver));
  receivers_.push_back(receiver);
}

PLAN_B_ONLY bool RtpTransceiver::RemoveReceiverPlanB(
    RtpReceiverInterface* receiver) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!unified_plan_);
  RTC_DCHECK_EQ(media_type(), receiver->media_type());
  auto it = absl::c_find(receivers_, receiver);
  if (it == receivers_.end()) {
    return false;
  }

  (*it)->internal()->Stop();
  context()->worker_thread()->BlockingCall([&]() {
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

// RTC_RUN_ON(context()->network_thread())
void RtpTransceiver::OnPacketReceived(
    scoped_refptr<PendingTaskSafetyFlag> safety) {
  if (!receptive_n_) {
    return;
  }
  if (packet_notified_after_receptive_) {
    return;
  }
  packet_notified_after_receptive_ = true;
  thread_->PostTask(SafeTask(safety, [this]() {
    RTC_DCHECK_RUN_ON(thread_);
    if (stopping() || stopped() || !receptive_) {
      return;
    }
    for (const auto& receiver : receivers_) {
      receiver->internal()->NotifyFirstPacketReceivedAfterReceptiveChange();
    }
  }));
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
    return LOG_ERROR(RTCError::InvalidState()
                     << "Cannot set direction on a stopping transceiver.");
  }
  if (new_direction == direction_)
    return RTCError::OK();

  if (new_direction == RtpTransceiverDirection::kStopped) {
    return LOG_ERROR(RTCError::InvalidParameter()
                     << "The set direction 'stopped' is invalid.");
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

bool RtpTransceiver::receptive() const {
  RTC_DCHECK_RUN_ON(thread_);
  return receptive_;
}

void RtpTransceiver::set_receptive(bool receptive) {
  RTC_DCHECK_RUN_ON(thread_);
  if (receptive != receptive_) {
    receptive_ = receptive;
    context()->network_thread()->PostTask(
        SafeTask(network_thread_safety_, [this, receptive = receptive]() {
          RTC_DCHECK_RUN_ON(context()->network_thread());
          receptive_n_ = receptive;
          packet_notified_after_receptive_ = false;
        }));
  }
}

void RtpTransceiver::StopSendingAndReceiving() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!stopping_);
  // 1. Let sender be transceiver.[[Sender]].
  // 2. Let receiver be transceiver.[[Receiver]].

  RTC_LOG_THREAD_BLOCK_COUNT();

  // Signal to receiver sources that we're stopping.
  for (const auto& receiver : receivers_) {
    receiver->internal()->Stop();
  }

  // 4. Stop sending media with sender.
  // We do this *after* the media channel has been set to nullptr on the
  // worker thread to avoid each sender doing that within `Stop()`.
  // Senders will have already cleared send when the media channel was set to
  // nullptr.
  std::vector<absl::AnyInvocable<void() &&>> stop =
      DetachAndGetStopTasksForSenders(senders_);

  // 3. Send an RTCP BYE for each RTP stream that was being sent by sender, as
  // specified in [RFC3550].
  context()->worker_thread()->BlockingCall([&]() {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    for (auto& task : stop) {
      std::move(task)();
    }
    ClearMediaChannelReferences();
  });

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);

  stopping_ = true;
  direction_ = RtpTransceiverDirection::kInactive;

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);
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
  //    (Note: Checking for IsClosed() is implemented by the user agent).
  //
  // 4. If transceiver.[[Stopping]] is true, abort these steps.
  if (stopping_) {
    return RTCError::OK();
  }

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

  // 3. Set transceiver.[[Receptive]] to false.
  receptive_ = false;

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
bool IsMandatoryHeaderExtension(absl::string_view uri) {
  return uri == RtpExtension::kMidUri;
}

RTCError RtpTransceiver::SetHeaderExtensionsToNegotiate(
    ArrayView<const RtpHeaderExtensionCapability> header_extensions) {
  RTC_DCHECK_RUN_ON(thread_);
  // https://w3c.github.io/webrtc-extensions/#dom-rtcrtptransceiver-setheaderextensionstonegotiate
  if (header_extensions.size() != header_extensions_to_negotiate_.size()) {
    return RTCError::InvalidModification()
           << "Size of extensions to negotiate does not match.";
  }
  // For each index i of extensions, run the following steps: ...
  for (size_t i = 0; i < header_extensions.size(); i++) {
    const auto& extension = header_extensions[i];
    if (extension.uri != header_extensions_to_negotiate_[i].uri) {
      return RTCError::InvalidModification()
             << "Reordering extensions is not allowed.";
    }
    if (IsMandatoryHeaderExtension(extension.uri) &&
        extension.direction != RtpTransceiverDirection::kSendRecv) {
      return RTCError::InvalidModification()
             << "Attempted to stop a mandatory extension.";
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
    if (env_.field_trials().IsEnabled(
            "WebRTC-HeaderExtensionNegotiateMemory")) {
      header_extensions_to_negotiate_ = GetNegotiatedHeaderExtensions();
    }
  } else if (sdp_type == SdpType::kOffer) {
    if (env_.field_trials().IsEnabled(
            "WebRTC-HeaderExtensionNegotiateMemory")) {
      header_extensions_for_rollback_ = header_extensions_to_negotiate_;
      header_extensions_to_negotiate_ =
          GetOfferedAndImplementedHeaderExtensions(content);
    }
  } else if (sdp_type == SdpType::kRollback) {
    if (env_.field_trials().IsEnabled(
            "WebRTC-HeaderExtensionNegotiateMemory")) {
      RTC_CHECK(!header_extensions_for_rollback_.empty());
      header_extensions_to_negotiate_ = header_extensions_for_rollback_;
    }
  }
}

bool RtpTransceiver::SetChannelRtpTransport(
    RtpTransportInternal* rtp_transport) {
  RTC_DCHECK_RUN_ON(context()->network_thread());
  RTC_DCHECK(channel_);
  return channel_->SetRtpTransport(rtp_transport);
}

bool RtpTransceiver::SetChannelLocalContent(
    const MediaContentDescription* content,
    SdpType type,
    std::string& error_desc) {
  RTC_DCHECK_RUN_ON(context()->signaling_thread());
  return SetChannelContent([&]() {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    return channel_->SetLocalContent(content, type, error_desc);
  });
}

bool RtpTransceiver::SetChannelRemoteContent(
    const MediaContentDescription* content,
    SdpType type,
    std::string& error_desc) {
  RTC_DCHECK_RUN_ON(context()->signaling_thread());
  return SetChannelContent([&]() {
    RTC_DCHECK_RUN_ON(context()->worker_thread());
    return channel_->SetRemoteContent(content, type, error_desc);
  });
}

bool RtpTransceiver::SetChannelContent(
    absl::AnyInvocable<bool() &&> set_content) {
  RTC_DCHECK_RUN_ON(context()->signaling_thread());
  if (!channel_) {
    return false;
  }

  struct SenderParameters {
    const uint32_t ssrc;
    RtpSenderInternal* const sender;
    std::optional<RtpParameters> parameters;
  };

  std::vector<SenderParameters> sender_parameters;
  sender_parameters.reserve(senders_.size());
  for (const auto& sender : senders_) {
    sender_parameters.push_back(
        {.ssrc = sender->ssrc(), .sender = sender->internal()});
  }

  // Calls the callback on the worker thread, fetches and returns the
  // RtpParameters for the senders.
  bool result = context()->worker_thread()->BlockingCall([&]() {
    if (!std::move(set_content)()) {
      return false;
    }
    for (auto& entry : sender_parameters) {
      if (entry.ssrc != 0) {
        entry.parameters =
            channel_->media_send_channel()->GetRtpSendParameters(entry.ssrc);
      }
    }
    return true;
  });

  for (auto& entry : sender_parameters) {
    if (entry.parameters) {
      entry.sender->SetCachedParameters(std::move(*entry.parameters));
    }
  }

  return result;
}

bool RtpTransceiver::SetChannelPayloadTypeDemuxingEnabled(bool enabled) {
  RTC_DCHECK_RUN_ON(context()->worker_thread());
  RTC_DCHECK(channel_);
  return channel_->SetPayloadTypeDemuxingEnabled(enabled);
}

void RtpTransceiver::EnableChannel(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(channel_);
  channel_->Enable(enable);
}

const std::vector<StreamParams>& RtpTransceiver::channel_local_streams() const {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(channel_);
  return channel_->local_streams();
}

const std::vector<StreamParams>& RtpTransceiver::channel_remote_streams()
    const {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(channel_);
  return channel_->remote_streams();
}

absl::string_view RtpTransceiver::channel_transport_name() const {
  RTC_DCHECK_RUN_ON(context()->network_thread());
  RTC_DCHECK(channel_);
  return channel_->transport_name();
}

MediaSendChannelInterface* RtpTransceiver::media_send_channel() {
  RTC_DCHECK_RUN_ON(thread_);
  return channel_ ? channel_->media_send_channel() : nullptr;
}

const MediaSendChannelInterface* RtpTransceiver::media_send_channel() const {
  RTC_DCHECK_RUN_ON(thread_);
  return channel_ ? channel_->media_send_channel() : nullptr;
}

MediaReceiveChannelInterface* RtpTransceiver::media_receive_channel() {
  RTC_DCHECK_RUN_ON(thread_);
  return channel_ ? channel_->media_receive_channel() : nullptr;
}

const MediaReceiveChannelInterface* RtpTransceiver::media_receive_channel()
    const {
  RTC_DCHECK_RUN_ON(thread_);
  return channel_ ? channel_->media_receive_channel() : nullptr;
}

VideoMediaSendChannelInterface* RtpTransceiver::video_media_send_channel() {
  // Accessed from multiple threads.
  // See https://issues.webrtc.org/475126742
  return channel_ ? channel_->video_media_send_channel() : nullptr;
}

VoiceMediaSendChannelInterface* RtpTransceiver::voice_media_send_channel() {
  // Accessed from multiple threads.
  // See https://issues.webrtc.org/475126742
  return channel_ ? channel_->voice_media_send_channel() : nullptr;
}

VideoMediaReceiveChannelInterface*
RtpTransceiver::video_media_receive_channel() {
  // Accessed from multiple threads.
  // See https://issues.webrtc.org/475126742
  return channel_ ? channel_->video_media_receive_channel() : nullptr;
}

VoiceMediaReceiveChannelInterface*
RtpTransceiver::voice_media_receive_channel() {
  // Accessed from multiple threads.
  // See https://issues.webrtc.org/475126742
  return channel_ ? channel_->voice_media_receive_channel() : nullptr;
}

void RtpTransceiver::SetTransport(scoped_refptr<DtlsTransport> transport,
                                  std::optional<std::string> transport_name) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(HasChannel() || !transport);
  RTC_DCHECK((transport && transport_name.has_value()) ||
             (!transport && !transport_name));
  RTC_DCHECK(!transport_name.has_value() || !transport_name.value().empty());
  transport_name_ = std::move(transport_name);
  for (auto& sender : senders_) {
    sender->internal()->set_transport(transport);
  }
  for (auto& receiver : receivers_) {
    receiver->internal()->set_transport(transport);
  }
}

}  // namespace webrtc
