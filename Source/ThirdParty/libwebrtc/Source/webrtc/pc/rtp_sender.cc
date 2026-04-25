/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_sender.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/audio_options.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/dtmf_sender_interface.h"
#include "api/environment/environment.h"
#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/audio_source.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_engine.h"
#include "pc/dtmf_sender.h"
#include "pc/legacy_stats_collector_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

// This function is only expected to be called on the signaling thread.
// On the other hand, some test or even production setups may use
// several signaling threads.
int GenerateUniqueId() {
  static std::atomic<int> g_unique_id{0};

  return ++g_unique_id;
}

// Returns true if a "per-sender" encoding parameter contains a value that isn't
// its default. Currently max_bitrate_bps and bitrate_priority both are
// implemented "per-sender," meaning that these encoding parameters
// are used for the RtpSender as a whole, not for a specific encoding layer.
// This is done by setting these encoding parameters at index 0 of
// RtpParameters.encodings. This function can be used to check if these
// parameters are set at any index other than 0 of RtpParameters.encodings,
// because they are currently unimplemented to be used for a specific encoding
// layer.
bool PerSenderRtpEncodingParameterHasValue(
    const RtpEncodingParameters& encoding_params) {
  if (encoding_params.bitrate_priority != kDefaultBitratePriority ||
      encoding_params.network_priority != Priority::kLow) {
    return true;
  }
  return false;
}

void RemoveEncodingLayers(const std::vector<std::string>& rids,
                          std::vector<RtpEncodingParameters>* encodings) {
  RTC_DCHECK(encodings);
  std::erase_if(*encodings, [&rids](const RtpEncodingParameters& encoding) {
    return absl::c_linear_search(rids, encoding.rid);
  });
}

RtpParameters RestoreEncodingLayers(
    const RtpParameters& parameters,
    const std::vector<std::string>& removed_rids,
    const std::vector<RtpEncodingParameters>& all_layers) {
  RTC_CHECK_EQ(parameters.encodings.size() + removed_rids.size(),
               all_layers.size());
  RtpParameters result(parameters);
  result.encodings.clear();
  size_t index = 0;
  for (const RtpEncodingParameters& encoding : all_layers) {
    if (absl::c_linear_search(removed_rids, encoding.rid)) {
      result.encodings.push_back(encoding);
      continue;
    }
    result.encodings.push_back(parameters.encodings[index++]);
  }
  return result;
}

// Checks that the codec parameters are valid.
RTCError CheckCodecParameters(const RtpParameters& parameters,
                              const std::vector<Codec>& send_codecs,
                              const std::optional<Codec>& send_codec) {
  // Match the currently used codec against the codec preferences to gather
  // the SVC capabilities.
  std::optional<Codec> send_codec_with_svc_info;
  if (send_codec && send_codec->type == Codec::Type::kVideo) {
    auto codec_match = absl::c_find_if(
        send_codecs, [&](auto& codec) { return send_codec->Matches(codec); });
    if (codec_match != send_codecs.end()) {
      send_codec_with_svc_info = *codec_match;
    }
  }

  return CheckScalabilityModeValues(parameters, send_codecs,
                                    send_codec_with_svc_info);
}

// Logic that runs on the worker thread to set the parameters.
// Returns an error if the parameters check failed or if the set failed.
void SetRtpParametersOnWorkerThread(
    MediaSendChannelInterface* media_channel,
    const std::vector<Codec>& send_codecs,
    const std::vector<std::string>& disabled_rids,
    const Environment& env,
    uint32_t ssrc,
    RtpParameters parameters,
    SetParametersCallback callback) {
  RTC_DCHECK(media_channel);
  RtpParameters old_parameters = media_channel->GetRtpSendParameters(ssrc);
  // Add the inactive layers if disabled_rids isn't empty.
  RtpParameters rtp_parameters =
      disabled_rids.empty() ? parameters
                            : RestoreEncodingLayers(parameters, disabled_rids,
                                                    old_parameters.encodings);

  RTCError result = CheckRtpParametersInvalidModificationAndValues(
      old_parameters, rtp_parameters, env.field_trials());
  if (!result.ok()) {
    std::move(callback)(std::move(result));
    return;
  }

  result = CheckCodecParameters(rtp_parameters, send_codecs,
                                media_channel->GetSendCodec());
  if (!result.ok()) {
    std::move(callback)(std::move(result));
    return;
  }

  media_channel->SetRtpSendParameters(ssrc, rtp_parameters,
                                      std::move(callback));
}

}  // namespace

// Returns true if any RtpParameters member that isn't implemented contains a
// value.
bool UnimplementedRtpParameterHasValue(const RtpParameters& parameters) {
  if (!parameters.mid.empty()) {
    return true;
  }
  for (size_t i = 0; i < parameters.encodings.size(); ++i) {
    // Encoding parameters that are per-sender should only contain value at
    // index 0.
    if (i != 0 &&
        PerSenderRtpEncodingParameterHasValue(parameters.encodings[i])) {
      return true;
    }
  }
  return false;
}

RtpSenderBase::RtpSenderBase(const Environment& env,
                             Thread* signaling_thread,
                             Thread* worker_thread,
                             absl::string_view id,
                             MediaType media_type,
                             SetStreamsObserver* set_streams_observer,
                             MediaSendChannelInterface* media_channel)
    : env_(env),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      id_(id),
      media_type_(media_type),
      media_channel_(nullptr),  // Will be set in SetMediaChannel().
      set_streams_observer_(set_streams_observer),
      worker_safety_(PendingTaskSafetyFlag::CreateAttachedToTaskQueue(
          /*alive=*/media_channel != nullptr,
          worker_thread_)),
      signaling_safety_(
          PendingTaskSafetyFlag::CreateAttachedToTaskQueue(/*alive=*/true,
                                                           signaling_thread_)) {
  RTC_DCHECK(worker_thread_);
  init_parameters_.encodings.emplace_back();
  if (media_channel) {
    // When initialized with a valid media channel, we need to be running on the
    // worker thread in order to set things up properly.
    RTC_DCHECK_RUN_ON(worker_thread_);
    SetMediaChannel(media_channel);
  } else {
    // Otherwise, we're less picky (but probably running on the signaling
    // thread).
  }
}

RtpSenderBase::~RtpSenderBase() {
  RTC_DCHECK(!media_channel_) << "Missing call to SetMediaChannel(nullptr)";
}

void RtpSenderBase::SetFrameEncryptor(
    scoped_refptr<FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_) {
    return;
  }
  // Special Case: Set the frame encryptor to any value on any existing channel.
  worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    frame_encryptor_ = std::move(frame_encryptor);
    if (media_channel_) {
      media_channel_->SetFrameEncryptor(ssrc, frame_encryptor_);
    }
  });
}

void RtpSenderBase::SetEncoderSelector(
    std::unique_ptr<VideoEncoderFactory::EncoderSelectorInterface>
        encoder_selector) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  encoder_selector_ = std::move(encoder_selector);
  SetEncoderSelectorOnChannel();
}

void RtpSenderBase::SetEncoderSelectorOnChannel() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_ || ssrc_ == 0) {
    return;
  }
  worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (media_channel_)
      media_channel_->SetEncoderSelector(ssrc, encoder_selector_.get());
  });
}

void RtpSenderBase::SetCachedParameters(RtpParameters parameters) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  cached_parameters_ = std::move(parameters);
}

void RtpSenderBase::SetMediaChannel(MediaSendChannelInterface* media_channel) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(!media_channel || media_channel->media_type() == media_type_);
  if (media_channel_ == media_channel) {
    return;
  }

  // Note that setting the media_channel_ to nullptr and clearing the send state
  // via ClearSend_w, are separate operations. Stopping the actual send
  // operation, needs to be done via any of the paths that end up with a call to
  // ClearSend_w(), such as DetachTrackAndGetStopTask().
  media_channel_ = media_channel;
  media_channel_ ? worker_safety_->SetAlive() : worker_safety_->SetNotAlive();
}

RtpParameters RtpSenderBase::GetParametersInternal(bool may_use_cache,
                                                   bool with_all_layers) const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_) {
    return RtpParameters();
  }
  if (ssrc_ == 0) {
    return init_parameters_;
  }

  RtpParameters result;
  if (may_use_cache && cached_parameters_) {
    result = *cached_parameters_;
  } else {
    bool success = worker_thread_->BlockingCall([&, ssrc = ssrc_]() {
      RTC_DCHECK_RUN_ON(worker_thread_);
      if (!media_channel_) {
        return false;
      }
      result = media_channel_->GetRtpSendParameters(ssrc);
      return true;
    });
    if (!success) {
      cached_parameters_.reset();
      return init_parameters_;
    }
    cached_parameters_ = result;
  }

  if (!with_all_layers) {
    RemoveEncodingLayers(disabled_rids_, &result.encodings);
  }
  return result;
}

RtpParameters RtpSenderBase::GetParametersInternalWithAllLayers() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return GetParametersInternal(/*may_use_cache=*/true,
                               /*with_all_layers=*/true);
}

RtpParameters RtpSenderBase::GetParameters() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
#if RTC_DCHECK_IS_ON
  // TODO(tommi): Here, we can use `last_transaction_id_` to allow for
  // multiple GetParameters() calls in a row return cached parameters
  // (we could still generate a new transaction_id every time). Since
  // `last_transaction_id_` will be reset whenever the parameters change, we
  // could reliably cache the currently active parameters and whenever
  // `last_transaction_id_` has been reset, only then take the penalty of
  // refreshing the cached value (or even rely on the `changed` callback to
  // refresh the cached parameters). Alternatively, we could maintain such a
  // cache only at the GetParametersInternal() level that's used internally in
  // webrtc, e.g. for stats purposes, and use the cache only when
  // GetParametersInternal() is called directly and not via GetParameters().
  //
  // This `cached` variable and the `RTC_DCHECK` below are here temporarily
  // to verify the correctness of the cache as the first implementation of it
  // lands. Once we have confidence that the cache is reliably up to date,
  // we can update GetParameters() to use the cache without having to thread
  // hop.
  auto cached = cached_parameters_;
#endif

  RtpParameters result = GetParametersInternal(/*may_use_cache=*/false,
                                               /*with_all_layers=*/false);
  // Start a new transaction. `last_transaction_id_` will be reset whenever
  // the parameters change.
  last_transaction_id_ = CreateRandomUuid();
  result.transaction_id = last_transaction_id_.value();

#if RTC_DCHECK_IS_ON
  // The internal cache is only used when not stopped and ssrc_ is not 0.
  // `cached_parameters_` might get reset if the media channel is gone.
  if (cached && !stopped_ && ssrc_ != 0 && cached_parameters_) {
    RtpParameters cached_filtered = *cached;
    RemoveEncodingLayers(disabled_rids_, &cached_filtered.encodings);
    RTC_DCHECK(cached_filtered == result)
        << "The cached value should have been equal (filtered)";
  }
#endif
  return result;
}

std::optional<RTCError> RtpSenderBase::ValidateAndMaybeUpdateInitParameters(
    const RtpParameters& parameters) {
  if (UnimplementedRtpParameterHasValue(parameters)) {
    return LOG_ERROR(RTCError::UnsupportedParameter()
                     << "Attempted to set an unimplemented parameter of "
                        "RtpParameters.");
  }
  if (ssrc_ == 0) {
    auto result = CheckRtpParametersInvalidModificationAndValues(
        init_parameters_, parameters, send_codecs_, std::nullopt,
        env_.field_trials());
    if (result.ok()) {
      init_parameters_ = parameters;
    }
    return result;
  }
  return std::nullopt;
}

RTCError RtpSenderBase::SetParametersInternalWorkaround(
    const RtpParameters& parameters) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTCError error = RTCError::InvalidState();
  RtpParameters fetched_parameters;
  worker_thread_->BlockingCall([&, ssrc = ssrc_]() {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (!media_channel_)
      return;
    Event done_event;
    SetRtpParametersOnWorkerThread(media_channel_, send_codecs_, disabled_rids_,
                                   env_, ssrc, parameters, [&](RTCError err) {
                                     error = std::move(err);
                                     done_event.Set();
                                   });
    done_event.Wait(Event::kForever);
    if (error.ok()) {
      fetched_parameters = media_channel_->GetRtpSendParameters(ssrc);
    }
  });
  if (error.ok()) {
    init_parameters_ = fetched_parameters;
    cached_parameters_ = std::move(fetched_parameters);
  }
  return error;
}

RTCError RtpSenderBase::SetParametersInternal(const RtpParameters& parameters,
                                              SetParametersCallback callback,
                                              bool blocking) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(!blocking || !callback) << "Callback must be null if blocking";

  if (auto error = ValidateAndMaybeUpdateInitParameters(parameters)) {
    if (callback) {
      std::move(callback)(*error);
    }
    return *error;
  }

  // Invalidate the cache to ensure that GetParameters() doesn't use a stale
  // cache while the worker thread is updating the parameters.
  cached_parameters_.reset();

  if (blocking && worker_thread_ == signaling_thread_) {
    return SetParametersInternalWorkaround(parameters);
  }

  // Specific handling for when a blocking operation is requested.
  Event done_event;
  RTCError blocking_error = RTCError::OK();
  std::unique_ptr<RtpParameters> blocking_applied_parameters;
  if (blocking) {
    callback = [&done_event, &blocking_error](RTCError error) {
      blocking_error = std::move(error);
      done_event.Set();
    };
  }

  // A wrapper callback that fetches the parameters on the worker thread
  // immediately after they have been set, then posts a task to the signaling
  // thread to update the cache and invoke the original callback.
  // This ensures strict ordering: Set -> Fetch -> Update Cache -> Callback.
  //
  // Note: The callback might be invoked on a thread other than the worker
  // thread (e.g. the encoder queue). In that case, we must post a task back
  // to the worker thread to safely access `media_channel_`.
  auto callback_wrapper =
      [this, blocking, &blocking_applied_parameters,
       signaling_safety = signaling_safety_.flag(),
       worker_safety_flag = worker_safety_, input_parameters = parameters,
       callback = std::move(callback), ssrc = ssrc_](RTCError error) mutable {
        auto on_worker_thread = [this, blocking, &blocking_applied_parameters,
                                 signaling_safety = std::move(signaling_safety),
                                 input_parameters = std::move(input_parameters),
                                 callback = std::move(callback), ssrc,
                                 error = std::move(error)]() mutable {
          RTC_DCHECK_RUN_ON(worker_thread_);
          std::unique_ptr<RtpParameters> fetched_parameters;
          if (error.ok()) {
            fetched_parameters = std::make_unique<RtpParameters>(
                media_channel_->GetRtpSendParameters(ssrc));
          }

          if (blocking) {
            blocking_applied_parameters = std::move(fetched_parameters);
            std::move(callback)(std::move(error));
          } else {
            signaling_thread_->PostTask(SafeTask(
                std::move(signaling_safety),
                [this, callback = std::move(callback), error = std::move(error),
                 fetched_parameters = std::move(fetched_parameters),
                 input_parameters = std::move(input_parameters)]() mutable {
                  RTC_DCHECK_RUN_ON(signaling_thread_);
                  if (error.ok()) {
                    init_parameters_ = std::move(input_parameters);
                    cached_parameters_ = *fetched_parameters;
                  }
                  std::move(callback)(std::move(error));
                }));
          }
        };

        if (worker_thread_->IsCurrent()) {
          on_worker_thread();
        } else {
          worker_thread_->PostTask(SafeTask(std::move(worker_safety_flag),
                                            std::move(on_worker_thread)));
        }
      };

  auto task = [&, callback = std::move(callback_wrapper),
               parameters = parameters, ssrc = ssrc_]() mutable {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (media_channel_) {
      SetRtpParametersOnWorkerThread(media_channel_, send_codecs_,
                                     disabled_rids_, env_, ssrc, parameters,
                                     std::move(callback));
    } else {
      std::move(callback)(RTCError::InvalidState());
    }
  };

  if (blocking) {
    worker_thread_->BlockingCall(task);
    done_event.Wait(Event::kForever);
    if (blocking_error.ok()) {
      cached_parameters_ = std::move(*blocking_applied_parameters);
      init_parameters_ = *cached_parameters_;
    }
    return blocking_error;
  }

  worker_thread_->PostTask(SafeTask(worker_safety_, std::move(task)));
  return RTCError::OK();
}

RTCError RtpSenderBase::SetParametersInternalWithAllLayers(
    const RtpParameters& parameters) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(!stopped_);

  if (auto error = ValidateAndMaybeUpdateInitParameters(parameters)) {
    return *error;
  }
  RtpParameters applied_parameters;
  RTCError error = worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    RTCError err = media_channel_ ? media_channel_->SetRtpSendParameters(
                                        ssrc, parameters, nullptr)
                                  : RTCError::InvalidState();
    if (err.ok()) {
      applied_parameters = media_channel_->GetRtpSendParameters(ssrc);
    }
    return err;
  });

  if (error.ok()) {
    cached_parameters_ = std::move(applied_parameters);
  }

  return error;
}

RTCError RtpSenderBase::CheckSetParameters(const RtpParameters& parameters) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_) {
    return LOG_ERROR(RTCError::InvalidState()
                     << "Cannot set parameters on a stopped sender.");
  }
  if (!last_transaction_id_) {
    return LOG_ERROR(RTCError::InvalidState()
                     << "Failed to set parameters since getParameters() has "
                        "never been called"
                        " on this sender");
  }
  if (last_transaction_id_ != parameters.transaction_id) {
    return LOG_ERROR(RTCError::InvalidModification()
                     << "Failed to set parameters since the transaction_id "
                        "doesn't match"
                        " the last value returned from getParameters()");
  }

  return RTCError::OK();
}

RTCError RtpSenderBase::SetParameters(const RtpParameters& parameters) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  TRACE_EVENT0("webrtc", "RtpSenderBase::SetParameters");
  RTCError result = CheckSetParameters(parameters);
  if (!result.ok())
    return result;

  result = SetParametersInternal(parameters, nullptr, /*blocking=*/true);
  last_transaction_id_.reset();
  return result;
}

void RtpSenderBase::SetParametersAsync(const RtpParameters& parameters,
                                       SetParametersCallback callback) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(callback);
  TRACE_EVENT0("webrtc", "RtpSenderBase::SetParametersAsync");
  RTCError result = CheckSetParameters(parameters);
  if (!result.ok()) {
    std::move(callback)(result);
    return;
  }

  SetParametersInternal(
      parameters,
      [this, callback = std::move(callback)](RTCError error) mutable {
        RTC_DCHECK_RUN_ON(signaling_thread_);
        last_transaction_id_.reset();
        std::move(callback)(error);
      },
      /*blocking=*/false);
}

void RtpSenderBase::SetObserver(RtpSenderObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  observer_ = observer;
  // Deliver any notifications the observer may have missed by being set late.
  if (sent_first_packet_ && observer_) {
    observer_->OnFirstPacketSent(media_type());
  }
}

void RtpSenderBase::NotifyFirstPacketSent() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (observer_) {
    observer_->OnFirstPacketSent(media_type());
  }
  sent_first_packet_ = true;
}

void RtpSenderBase::set_stream_ids(const std::vector<std::string>& stream_ids) {
  stream_ids_.clear();
  absl::c_copy_if(stream_ids, std::back_inserter(stream_ids_),
                  [this](const std::string& stream_id) {
                    return !absl::c_linear_search(stream_ids_, stream_id);
                  });
}

void RtpSenderBase::SetStreams(const std::vector<std::string>& stream_ids) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  set_stream_ids(stream_ids);
  if (set_streams_observer_ && !stopped_)
    set_streams_observer_->OnSetStreams();
}

bool RtpSenderBase::SetTrack(MediaStreamTrackInterface* track) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  TRACE_EVENT0("webrtc", "RtpSenderBase::SetTrack");
  if (stopped_) {
    RTC_LOG(LS_ERROR) << "SetTrack can't be called on a stopped RtpSender.";
    return false;
  }
  if (track && track->kind() != track_kind()) {
    RTC_LOG(LS_ERROR) << "SetTrack with " << track->kind()
                      << " called on RtpSender with " << track_kind()
                      << " track.";
    return false;
  }

  // Detach from old track.
  if (track_) {
    DetachTrack();
    track_->UnregisterObserver(this);
    RemoveTrackFromStats();
  }

  // Attach to new track.
  bool prev_can_send_track = can_send_track();
  // Keep a reference to the old track to keep it alive until we call SetSend.
  scoped_refptr<MediaStreamTrackInterface> old_track = track_;
  track_ = track;
  if (track_) {
    track_->RegisterObserver(this);
    AttachTrack();
  }

  // Update channel.
  if (can_send_track()) {
    SetSend();
    AddTrackToStats();
  } else if (prev_can_send_track) {
    ClearSend();
  }
  attachment_id_ = (track_ ? GenerateUniqueId() : 0);
  return true;
}

void RtpSenderBase::SetSsrc(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  TRACE_EVENT0("webrtc", "RtpSenderBase::SetSsrc");
  if (stopped_ || ssrc == ssrc_) {
    return;
  }

  cached_parameters_.reset();

  // If we are already sending with a particular SSRC, stop sending.
  if (can_send_track()) {
    ClearSend();
    RemoveTrackFromStats();
  }
  ssrc_ = ssrc;
  if (can_send_track()) {
    SetSend();
    AddTrackToStats();
  }

  const bool update_parameters =
      (ssrc_ != 0 && (!init_parameters_.encodings.empty() ||
                      init_parameters_.degradation_preference.has_value()));
  RtpParameters current_parameters;
  bool params_modified = false;
  worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (update_parameters) {
      RTC_DCHECK(media_channel_);
      // Get the current parameters, which are constructed from the SDP. The
      // number of layers in the SDP is currently authoritative to support SDP
      // munging for Plan-B simulcast with "a=ssrc-group:SIM <ssrc-id>..." lines
      // as described in RFC 5576. All fields should be default constructed and
      // the SSRC field set, which we need to copy.
      current_parameters = media_channel_->GetRtpSendParameters(ssrc);
      // SSRC 0 has special meaning as "no stream". In this case,
      // current_parameters may have size 0.
      RTC_CHECK_GE(current_parameters.encodings.size(),
                   init_parameters_.encodings.size());
      for (size_t i = 0; i < init_parameters_.encodings.size(); ++i) {
        init_parameters_.encodings[i].ssrc =
            current_parameters.encodings[i].ssrc;
        init_parameters_.encodings[i].rid = current_parameters.encodings[i].rid;
        current_parameters.encodings[i] = init_parameters_.encodings[i];
      }
      current_parameters.degradation_preference =
          init_parameters_.degradation_preference;
      params_modified =
          media_channel_
              ->SetRtpSendParameters(ssrc, current_parameters, nullptr)
              .ok();
      if (params_modified) {
        // The parameters may change as they're applied.
        current_parameters = media_channel_->GetRtpSendParameters(ssrc);
      }
    }

    // While we're on the worker thread, attach the frame decryptor, transformer
    // and selector to the current media channel.
    if (frame_encryptor_) {
      media_channel_->SetFrameEncryptor(ssrc, frame_encryptor_);
    }
    if (frame_transformer_) {
      media_channel_->SetEncoderToPacketizerFrameTransformer(
          ssrc, frame_transformer_);
    }
    if (encoder_selector_) {
      media_channel_->SetEncoderSelector(ssrc, encoder_selector_.get());
    }
  });
  if (params_modified) {
    // As a result of the `SetRtpSendParameters` call, an async task will be
    // queued to update `cached_parameters_` - unless the parameters didn't
    // really change. In any case, we might as well stash away the current
    // parameters right away.
    cached_parameters_ = std::move(current_parameters);
  }
}

void RtpSenderBase::Stop() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  TRACE_EVENT0("webrtc", "RtpSenderBase::Stop");
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (stopped_) {
    return;
  }
  if (track_) {
    DetachTrack();
    track_->UnregisterObserver(this);
  }

  bool clear_send = can_send_track();
  if (clear_send) {
    RemoveTrackFromStats();
  }

  worker_thread_->BlockingCall([this, clear_send, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (clear_send) {
      ClearSend_w(ssrc);
    }
    SetMediaChannel(nullptr);
  });

  stopped_ = true;
  cached_parameters_.reset();
}

absl::AnyInvocable<void() &&> RtpSenderBase::DetachTrackAndGetStopTask() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK_DISALLOW_THREAD_BLOCKING_CALLS();
  TRACE_EVENT0("webrtc", "RtpSenderBase::DetachTrackAndGetStopTask");
  if (stopped_) {
    return nullptr;
  }
  if (track_) {
    DetachTrack();
    track_->UnregisterObserver(this);
  }

  bool clear_send = can_send_track();
  if (clear_send) {
    RemoveTrackFromStats();
  }

  stopped_ = true;
  cached_parameters_.reset();

  return [this, clear_send, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (clear_send) {
      ClearSend_w(ssrc);
    }
    SetMediaChannel(nullptr);
  };
}

RTCError RtpSenderBase::DisableEncodingLayers(
    const std::vector<std::string>& rids) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_) {
    return LOG_ERROR(RTCError::InvalidState()
                     << "Cannot disable encodings on a stopped sender.");
  }

  if (rids.empty()) {
    return RTCError::OK();
  }

  // Check that all the specified layers exist and disable them in the channel.
  RtpParameters parameters = GetParametersInternalWithAllLayers();
  for (const std::string& rid : rids) {
    if (absl::c_none_of(parameters.encodings,
                        [&rid](const RtpEncodingParameters& encoding) {
                          return encoding.rid == rid;
                        })) {
      return LOG_ERROR(RTCError::InvalidParameter()
                       << "RID: " << rid
                       << " does not refer to a valid layer.");
    }
  }

  if (ssrc_ == 0) {
    RemoveEncodingLayers(rids, &init_parameters_.encodings);
    // Invalidate any transaction upon success.
    last_transaction_id_.reset();
    return RTCError::OK();
  }

  for (RtpEncodingParameters& encoding : parameters.encodings) {
    // Remain active if not in the disable list.
    encoding.active &= absl::c_none_of(
        rids,
        [&encoding](const std::string& rid) { return encoding.rid == rid; });
  }

  RTCError result = SetParametersInternalWithAllLayers(parameters);
  if (result.ok()) {
    for (const auto& rid : rids) {
      // Avoid inserting duplicates.
      if (std::find(disabled_rids_.begin(), disabled_rids_.end(), rid) ==
          disabled_rids_.end()) {
        disabled_rids_.push_back(rid);
      }
    }
    // Invalidate any transaction upon success.
    last_transaction_id_.reset();
  }
  return result;
}

void RtpSenderBase::SetFrameTransformer(
    scoped_refptr<FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  frame_transformer_ = std::move(frame_transformer);
  if (ssrc_ && !stopped_) {
    worker_thread_->BlockingCall([&, ssrc = ssrc_] {
      RTC_DCHECK_RUN_ON(worker_thread_);
      if (media_channel_) {
        media_channel_->SetEncoderToPacketizerFrameTransformer(
            ssrc, frame_transformer_);
      }
    });
  }
}

LocalAudioSinkAdapter::LocalAudioSinkAdapter() : sink_(nullptr) {}

LocalAudioSinkAdapter::~LocalAudioSinkAdapter() {
  MutexLock lock(&lock_);
  if (sink_)
    sink_->OnClose();
}

void LocalAudioSinkAdapter::OnData(
    const void* audio_data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    std::optional<int64_t> absolute_capture_timestamp_ms) {
  TRACE_EVENT2("webrtc", "LocalAudioSinkAdapter::OnData", "sample_rate",
               sample_rate, "number_of_frames", number_of_frames);
  MutexLock lock(&lock_);
  if (sink_) {
    sink_->OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                  number_of_frames, absolute_capture_timestamp_ms);
    num_preferred_channels_ = sink_->NumPreferredChannels();
  }
}

void LocalAudioSinkAdapter::SetSink(AudioSource::Sink* sink) {
  MutexLock lock(&lock_);
  RTC_DCHECK(!sink || !sink_);
  sink_ = sink;
}

scoped_refptr<AudioRtpSender> AudioRtpSender::Create(
    const Environment& env,
    Thread* signaling_thread,
    Thread* worker_thread,
    absl::string_view id,
    LegacyStatsCollectorInterface* stats,
    SetStreamsObserver* set_streams_observer,
    MediaSendChannelInterface* media_channel) {
  return make_ref_counted<AudioRtpSender>(env, signaling_thread, worker_thread,
                                          id, stats, set_streams_observer,
                                          media_channel);
}

AudioRtpSender::AudioRtpSender(const Environment& env,
                               Thread* signaling_thread,
                               Thread* worker_thread,
                               absl::string_view id,
                               LegacyStatsCollectorInterface* stats,
                               SetStreamsObserver* set_streams_observer,
                               MediaSendChannelInterface* media_channel)
    : RtpSenderBase(env,
                    signaling_thread,
                    worker_thread,
                    id,
                    MediaType::AUDIO,
                    set_streams_observer,
                    media_channel),
      legacy_stats_(stats),
      dtmf_sender_(DtmfSender::Create(signaling_thread, this)),
      dtmf_sender_proxy_(
          DtmfSenderProxy::Create(signaling_thread, dtmf_sender_)),
      sink_adapter_(new LocalAudioSinkAdapter()) {}

AudioRtpSender::~AudioRtpSender() {
  dtmf_sender_->OnDtmfProviderDestroyed();
  Stop();
}

bool AudioRtpSender::CanInsertDtmf() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_) {
    return false;
  }
  // Check that this RTP sender is active (description has been applied that
  // matches an SSRC to its ID).
  if (ssrc_ == 0) {
    RTC_LOG(LS_ERROR) << "CanInsertDtmf: Sender does not have SSRC.";
    return false;
  }
  return worker_thread_->BlockingCall([&] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return media_channel_ ? voice_media_channel()->CanInsertDtmf() : false;
  });
}

bool AudioRtpSender::InsertDtmf(int code, int duration) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_) {
    return false;
  }
  if (ssrc_ == 0) {
    RTC_LOG(LS_ERROR) << "InsertDtmf: Sender does not have SSRC.";
    return false;
  }
  return worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return media_channel_
               ? voice_media_channel()->InsertDtmf(ssrc, code, duration)
               : false;
  });
}

void AudioRtpSender::OnChanged() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  TRACE_EVENT0("webrtc", "AudioRtpSender::OnChanged");
  RTC_DCHECK(!stopped_);
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    if (can_send_track()) {
      SetSend();
    }
  }
}

void AudioRtpSender::DetachTrack() {
  RTC_DCHECK(track_);
  audio_track()->RemoveSink(sink_adapter_.get());
}

void AudioRtpSender::AttachTrack() {
  RTC_DCHECK(track_);
  cached_track_enabled_ = track_->enabled();
  audio_track()->AddSink(sink_adapter_.get());
}

void AudioRtpSender::AddTrackToStats() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (can_send_track() && legacy_stats_) {
    legacy_stats_->AddLocalAudioTrack(audio_track().get(), ssrc_);
  }
}

void AudioRtpSender::RemoveTrackFromStats() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (can_send_track() && legacy_stats_) {
    legacy_stats_->RemoveLocalAudioTrack(audio_track().get(), ssrc_);
  }
}

scoped_refptr<DtmfSenderInterface> AudioRtpSender::GetDtmfSender() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return dtmf_sender_proxy_;
}

RTCError AudioRtpSender::GenerateKeyFrame(
    const std::vector<std::string>& rids) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DLOG(LS_ERROR) << "Tried to get generate a key frame for audio.";
  return RTCError::UnsupportedOperation()
         << "Generating key frames for audio is not supported.";
}

void AudioRtpSender::SetSend() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(can_send_track());
  if (stopped_) {
    return;
  }
  AudioOptions options;
#if !defined(WEBRTC_CHROMIUM_BUILD) && !defined(WEBRTC_WEBKIT_BUILD)
  // TODO(tommi): Remove this hack when we move CreateAudioSource out of
  // PeerConnection.  This is a bit of a strange way to apply local audio
  // options since it is also applied to all streams/channels, local or remote.
  if (track_->enabled() && audio_track()->GetSource() &&
      !audio_track()->GetSource()->remote()) {
    options = audio_track()->GetSource()->options();
  }
#endif

  // `track_->enabled()` hops to the signaling thread, so call it before we hop
  // to the worker thread or else it will deadlock.
  bool track_enabled = track_->enabled();
  bool success = worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return media_channel_
               ? voice_media_channel()->SetAudioSend(
                     ssrc, track_enabled, &options, sink_adapter_.get())
               : false;
  });
  if (!success) {
    RTC_LOG(LS_ERROR) << "SetAudioSend: ssrc is incorrect: " << ssrc_;
  }
}

void AudioRtpSender::ClearSend() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(ssrc_ != 0);
  RTC_DCHECK(!stopped_);
  worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    ClearSend_w(ssrc);
  });
}

void AudioRtpSender::ClearSend_w(uint32_t ssrc) {
  if (media_channel_) {
    AudioOptions options;
    voice_media_channel()->SetAudioSend(ssrc, false, &options, nullptr);
  }
}

scoped_refptr<VideoRtpSender> VideoRtpSender::Create(
    const Environment& env,
    Thread* signaling_thread,
    Thread* worker_thread,
    absl::string_view id,
    SetStreamsObserver* set_streams_observer,
    MediaSendChannelInterface* media_channel) {
  return make_ref_counted<VideoRtpSender>(env, signaling_thread, worker_thread,
                                          id, set_streams_observer,
                                          media_channel);
}

VideoRtpSender::VideoRtpSender(const Environment& env,
                               Thread* signaling_thread,
                               Thread* worker_thread,
                               absl::string_view id,
                               SetStreamsObserver* set_streams_observer,
                               MediaSendChannelInterface* media_channel)
    : RtpSenderBase(env,
                    signaling_thread,
                    worker_thread,
                    id,
                    MediaType::VIDEO,
                    set_streams_observer,
                    media_channel) {}

VideoRtpSender::~VideoRtpSender() {
  Stop();
}

void VideoRtpSender::OnChanged() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  TRACE_EVENT0("webrtc", "VideoRtpSender::OnChanged");
  RTC_DCHECK(!stopped_);

  auto content_hint = video_track()->content_hint();
  if (cached_track_content_hint_ != content_hint) {
    cached_track_content_hint_ = content_hint;
    if (can_send_track()) {
      SetSend();
    }
  }
}

void VideoRtpSender::AttachTrack() {
  RTC_DCHECK(track_);
  cached_track_content_hint_ = video_track()->content_hint();
}

scoped_refptr<DtmfSenderInterface> VideoRtpSender::GetDtmfSender() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DLOG(LS_ERROR) << "Tried to get DTMF sender from video sender.";
  return nullptr;
}

RTCError VideoRtpSender::GenerateKeyFrame(
    const std::vector<std::string>& rids) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (stopped_ || ssrc_ == 0) {
    RTC_LOG(LS_WARNING) << "Tried to generate key frame for sender that is "
                           "stopped or has no media channel.";
    // Wouldn't it be more correct to return an error?
    return RTCError::OK();
  }

  const auto parameters = GetParametersInternal();
  for (const auto& rid : rids) {
    if (rid.empty()) {
      return LOG_ERROR(RTCError::InvalidParameter()
                       << "Attempted to specify an empty rid.");
    }
    if (!absl::c_any_of(parameters.encodings,
                        [&rid](const RtpEncodingParameters& parameters) {
                          return parameters.rid == rid;
                        })) {
      return LOG_ERROR(RTCError::InvalidParameter()
                       << "Attempted to specify a rid not configured.");
    }
  }
  worker_thread_->PostTask(SafeTask(worker_safety_, [this, rids, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    if (video_media_channel()) {
      video_media_channel()->GenerateSendKeyFrame(ssrc, rids);
    }
  }));

  return RTCError::OK();
}

void VideoRtpSender::SetSend() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(can_send_track());
  VideoOptions options;
  VideoTrackSourceInterface* source = video_track()->GetSource();
  if (source) {
    options.is_screencast = source->is_screencast();
    options.video_noise_reduction = source->needs_denoising();
  }
  options.content_hint = cached_track_content_hint_;
  switch (cached_track_content_hint_) {
    case VideoTrackInterface::ContentHint::kNone:
      break;
    case VideoTrackInterface::ContentHint::kFluid:
      options.is_screencast = false;
      break;
    case VideoTrackInterface::ContentHint::kDetailed:
    case VideoTrackInterface::ContentHint::kText:
      options.is_screencast = true;
      break;
  }
  auto* video_track = static_cast<VideoTrackInterface*>(track_.get());
  bool success = worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    return media_channel_ ? video_media_channel()->SetVideoSend(ssrc, &options,
                                                                video_track)
                          : false;
  });
  RTC_DCHECK(success);
}

void VideoRtpSender::ClearSend() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(ssrc_ != 0);
  RTC_DCHECK(!stopped_);
  // Allow SetVideoSend to fail since `enable` is false and `source` is null.
  // This the normal case when the underlying media channel has already been
  // deleted.
  worker_thread_->BlockingCall([&, ssrc = ssrc_] {
    RTC_DCHECK_RUN_ON(worker_thread_);
    ClearSend_w(ssrc);
  });
}

void VideoRtpSender::ClearSend_w(uint32_t ssrc) {
  if (media_channel_) {
    video_media_channel()->SetVideoSend(ssrc, nullptr, nullptr);
  }
}

}  // namespace webrtc
