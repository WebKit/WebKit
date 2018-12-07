/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtpsender.h"

#include <utility>
#include <vector>

#include "api/mediastreaminterface.h"
#include "pc/localaudiosource.h"
#include "pc/statscollector.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

// This function is only expected to be called on the signalling thread.
int GenerateUniqueId() {
  static int g_unique_id = 0;

  return ++g_unique_id;
}

// Returns an true if any RtpEncodingParameters member that isn't implemented
// contains a value.
bool UnimplementedRtpEncodingParameterHasValue(
    const RtpEncodingParameters& encoding_params) {
  if (encoding_params.codec_payload_type.has_value() ||
      encoding_params.fec.has_value() || encoding_params.rtx.has_value() ||
      encoding_params.dtx.has_value() || encoding_params.ptime.has_value() ||
      !encoding_params.rid.empty() ||
      encoding_params.scale_resolution_down_by.has_value() ||
      encoding_params.scale_framerate_down_by.has_value() ||
      !encoding_params.dependency_rids.empty()) {
    return true;
  }
  return false;
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
      encoding_params.network_priority != kDefaultBitratePriority) {
    return true;
  }
  return false;
}

// Attempt to attach the frame decryptor to the current media channel on the
// correct worker thread only if both the media channel exists and a ssrc has
// been allocated to the stream.
void MaybeAttachFrameEncryptorToMediaChannel(
    const uint32_t ssrc,
    rtc::Thread* worker_thread,
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor,
    cricket::MediaChannel* media_channel,
    bool stopped) {
  if (media_channel && frame_encryptor && ssrc && !stopped) {
    worker_thread->Invoke<void>(RTC_FROM_HERE, [&] {
      media_channel->SetFrameEncryptor(ssrc, frame_encryptor);
    });
  }
}

}  // namespace

// Returns true if any RtpParameters member that isn't implemented contains a
// value.
bool UnimplementedRtpParameterHasValue(const RtpParameters& parameters) {
  if (!parameters.mid.empty()) {
    return true;
  }
  for (size_t i = 0; i < parameters.encodings.size(); ++i) {
    if (UnimplementedRtpEncodingParameterHasValue(parameters.encodings[i])) {
      return true;
    }
    // Encoding parameters that are per-sender should only contain value at
    // index 0.
    if (i != 0 &&
        PerSenderRtpEncodingParameterHasValue(parameters.encodings[i])) {
      return true;
    }
  }
  return false;
}

LocalAudioSinkAdapter::LocalAudioSinkAdapter() : sink_(nullptr) {}

LocalAudioSinkAdapter::~LocalAudioSinkAdapter() {
  rtc::CritScope lock(&lock_);
  if (sink_)
    sink_->OnClose();
}

void LocalAudioSinkAdapter::OnData(const void* audio_data,
                                   int bits_per_sample,
                                   int sample_rate,
                                   size_t number_of_channels,
                                   size_t number_of_frames) {
  rtc::CritScope lock(&lock_);
  if (sink_) {
    sink_->OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
                  number_of_frames);
  }
}

void LocalAudioSinkAdapter::SetSink(cricket::AudioSource::Sink* sink) {
  rtc::CritScope lock(&lock_);
  RTC_DCHECK(!sink || !sink_);
  sink_ = sink;
}

AudioRtpSender::AudioRtpSender(rtc::Thread* worker_thread,
                               const std::string& id,
                               StatsCollector* stats)
    : worker_thread_(worker_thread),
      id_(id),
      stats_(stats),
      dtmf_sender_proxy_(DtmfSenderProxy::Create(
          rtc::Thread::Current(),
          DtmfSender::Create(rtc::Thread::Current(), this))),
      sink_adapter_(new LocalAudioSinkAdapter()) {
  RTC_DCHECK(worker_thread);
  init_parameters_.encodings.emplace_back();
}

AudioRtpSender::~AudioRtpSender() {
  // For DtmfSender.
  SignalDestroyed();
  Stop();
}

bool AudioRtpSender::CanInsertDtmf() {
  if (!media_channel_) {
    RTC_LOG(LS_ERROR) << "CanInsertDtmf: No audio channel exists.";
    return false;
  }
  // Check that this RTP sender is active (description has been applied that
  // matches an SSRC to its ID).
  if (!ssrc_) {
    RTC_LOG(LS_ERROR) << "CanInsertDtmf: Sender does not have SSRC.";
    return false;
  }
  return worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, [&] { return media_channel_->CanInsertDtmf(); });
}

bool AudioRtpSender::InsertDtmf(int code, int duration) {
  if (!media_channel_) {
    RTC_LOG(LS_ERROR) << "InsertDtmf: No audio channel exists.";
    return false;
  }
  if (!ssrc_) {
    RTC_LOG(LS_ERROR) << "InsertDtmf: Sender does not have SSRC.";
    return false;
  }
  bool success = worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->InsertDtmf(ssrc_, code, duration);
  });
  if (!success) {
    RTC_LOG(LS_ERROR) << "Failed to insert DTMF to channel.";
  }
  return success;
}

sigslot::signal0<>* AudioRtpSender::GetOnDestroyedSignal() {
  return &SignalDestroyed;
}

void AudioRtpSender::OnChanged() {
  TRACE_EVENT0("webrtc", "AudioRtpSender::OnChanged");
  RTC_DCHECK(!stopped_);
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    if (can_send_track()) {
      SetAudioSend();
    }
  }
}

bool AudioRtpSender::SetTrack(MediaStreamTrackInterface* track) {
  TRACE_EVENT0("webrtc", "AudioRtpSender::SetTrack");
  if (stopped_) {
    RTC_LOG(LS_ERROR) << "SetTrack can't be called on a stopped RtpSender.";
    return false;
  }
  if (track && track->kind() != MediaStreamTrackInterface::kAudioKind) {
    RTC_LOG(LS_ERROR) << "SetTrack called on audio RtpSender with "
                      << track->kind() << " track.";
    return false;
  }
  AudioTrackInterface* audio_track = static_cast<AudioTrackInterface*>(track);

  // Detach from old track.
  if (track_) {
    track_->RemoveSink(sink_adapter_.get());
    track_->UnregisterObserver(this);
  }

  if (can_send_track() && stats_) {
    stats_->RemoveLocalAudioTrack(track_.get(), ssrc_);
  }

  // Attach to new track.
  bool prev_can_send_track = can_send_track();
  // Keep a reference to the old track to keep it alive until we call
  // SetAudioSend.
  rtc::scoped_refptr<AudioTrackInterface> old_track = track_;
  track_ = audio_track;
  if (track_) {
    cached_track_enabled_ = track_->enabled();
    track_->RegisterObserver(this);
    track_->AddSink(sink_adapter_.get());
  }

  // Update audio channel.
  if (can_send_track()) {
    SetAudioSend();
    if (stats_) {
      stats_->AddLocalAudioTrack(track_.get(), ssrc_);
    }
  } else if (prev_can_send_track) {
    ClearAudioSend();
  }
  attachment_id_ = (track_ ? GenerateUniqueId() : 0);
  return true;
}

RtpParameters AudioRtpSender::GetParameters() {
  if (stopped_) {
    return RtpParameters();
  }
  if (!media_channel_) {
    RtpParameters result = init_parameters_;
    last_transaction_id_ = rtc::CreateRandomUuid();
    result.transaction_id = last_transaction_id_.value();
    return result;
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    RtpParameters result = media_channel_->GetRtpSendParameters(ssrc_);
    last_transaction_id_ = rtc::CreateRandomUuid();
    result.transaction_id = last_transaction_id_.value();
    return result;
  });
}

RTCError AudioRtpSender::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "AudioRtpSender::SetParameters");
  if (stopped_) {
    return RTCError(RTCErrorType::INVALID_STATE);
  }
  if (!last_transaction_id_) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_STATE,
        "Failed to set parameters since getParameters() has never been called"
        " on this sender");
  }
  if (last_transaction_id_ != parameters.transaction_id) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Failed to set parameters since the transaction_id doesn't match"
        " the last value returned from getParameters()");
  }

  if (UnimplementedRtpParameterHasValue(parameters)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::UNSUPPORTED_PARAMETER,
        "Attempted to set an unimplemented parameter of RtpParameters.");
  }
  if (!media_channel_) {
    auto result = cricket::ValidateRtpParameters(init_parameters_, parameters);
    if (result.ok()) {
      init_parameters_ = parameters;
    }
    return result;
  }
  return worker_thread_->Invoke<RTCError>(RTC_FROM_HERE, [&] {
    RTCError result = media_channel_->SetRtpSendParameters(ssrc_, parameters);
    last_transaction_id_.reset();
    return result;
  });
}

rtc::scoped_refptr<DtmfSenderInterface> AudioRtpSender::GetDtmfSender() const {
  return dtmf_sender_proxy_;
}

void AudioRtpSender::SetFrameEncryptor(
    rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) {
  frame_encryptor_ = std::move(frame_encryptor);
  // Special Case: Set the frame encryptor to any value on any existing channel.
  if (media_channel_ && ssrc_ && !stopped_) {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      media_channel_->SetFrameEncryptor(ssrc_, frame_encryptor_);
    });
  }
}

rtc::scoped_refptr<FrameEncryptorInterface> AudioRtpSender::GetFrameEncryptor()
    const {
  return frame_encryptor_;
}

void AudioRtpSender::SetSsrc(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "AudioRtpSender::SetSsrc");
  if (stopped_ || ssrc == ssrc_) {
    return;
  }
  // If we are already sending with a particular SSRC, stop sending.
  if (can_send_track()) {
    ClearAudioSend();
    if (stats_) {
      stats_->RemoveLocalAudioTrack(track_.get(), ssrc_);
    }
  }
  ssrc_ = ssrc;
  if (can_send_track()) {
    SetAudioSend();
    if (stats_) {
      stats_->AddLocalAudioTrack(track_.get(), ssrc_);
    }
  }
  if (!init_parameters_.encodings.empty()) {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      RTC_DCHECK(media_channel_);
      // Get the current parameters, which are constructed from the SDP.
      // The number of layers in the SDP is currently authoritative to support
      // SDP munging for Plan-B simulcast with "a=ssrc-group:SIM <ssrc-id>..."
      // lines as described in RFC 5576.
      // All fields should be default constructed and the SSRC field set, which
      // we need to copy.
      RtpParameters current_parameters =
          media_channel_->GetRtpSendParameters(ssrc_);
      for (size_t i = 0; i < init_parameters_.encodings.size(); ++i) {
        init_parameters_.encodings[i].ssrc =
            current_parameters.encodings[i].ssrc;
        current_parameters.encodings[i] = init_parameters_.encodings[i];
      }
      current_parameters.degradation_preference =
          init_parameters_.degradation_preference;
      media_channel_->SetRtpSendParameters(ssrc_, current_parameters);
      init_parameters_.encodings.clear();
    });
  }
  // Each time there is an ssrc update.
  MaybeAttachFrameEncryptorToMediaChannel(
      ssrc_, worker_thread_, frame_encryptor_, media_channel_, stopped_);
}

void AudioRtpSender::Stop() {
  TRACE_EVENT0("webrtc", "AudioRtpSender::Stop");
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (stopped_) {
    return;
  }
  if (track_) {
    track_->RemoveSink(sink_adapter_.get());
    track_->UnregisterObserver(this);
  }
  if (can_send_track()) {
    ClearAudioSend();
    if (stats_) {
      stats_->RemoveLocalAudioTrack(track_.get(), ssrc_);
    }
  }
  media_channel_ = nullptr;
  stopped_ = true;
}

void AudioRtpSender::SetMediaChannel(cricket::MediaChannel* media_channel) {
  RTC_DCHECK(media_channel == nullptr ||
             media_channel->media_type() == media_type());
  media_channel_ = static_cast<cricket::VoiceMediaChannel*>(media_channel);
}

void AudioRtpSender::SetAudioSend() {
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(can_send_track());
  if (!media_channel_) {
    RTC_LOG(LS_ERROR) << "SetAudioSend: No audio channel exists.";
    return;
  }
  cricket::AudioOptions options;
#if !defined(WEBRTC_CHROMIUM_BUILD) && !defined(WEBRTC_WEBKIT_BUILD)
  // TODO(tommi): Remove this hack when we move CreateAudioSource out of
  // PeerConnection.  This is a bit of a strange way to apply local audio
  // options since it is also applied to all streams/channels, local or remote.
  if (track_->enabled() && track_->GetSource() &&
      !track_->GetSource()->remote()) {
    options = track_->GetSource()->options();
  }
#endif

  // |track_->enabled()| hops to the signaling thread, so call it before we hop
  // to the worker thread or else it will deadlock.
  bool track_enabled = track_->enabled();
  bool success = worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetAudioSend(ssrc_, track_enabled, &options,
                                        sink_adapter_.get());
  });
  if (!success) {
    RTC_LOG(LS_ERROR) << "SetAudioSend: ssrc is incorrect: " << ssrc_;
  }
}

void AudioRtpSender::ClearAudioSend() {
  RTC_DCHECK(ssrc_ != 0);
  RTC_DCHECK(!stopped_);
  if (!media_channel_) {
    RTC_LOG(LS_WARNING) << "ClearAudioSend: No audio channel exists.";
    return;
  }
  cricket::AudioOptions options;
  bool success = worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetAudioSend(ssrc_, false, &options, nullptr);
  });
  if (!success) {
    RTC_LOG(LS_WARNING) << "ClearAudioSend: ssrc is incorrect: " << ssrc_;
  }
}

VideoRtpSender::VideoRtpSender(rtc::Thread* worker_thread,
                               const std::string& id)
    : worker_thread_(worker_thread), id_(id) {
  RTC_DCHECK(worker_thread);
  init_parameters_.encodings.emplace_back();
}

VideoRtpSender::~VideoRtpSender() {
  Stop();
}

void VideoRtpSender::OnChanged() {
  TRACE_EVENT0("webrtc", "VideoRtpSender::OnChanged");
  RTC_DCHECK(!stopped_);
  if (cached_track_content_hint_ != track_->content_hint()) {
    cached_track_content_hint_ = track_->content_hint();
    if (can_send_track()) {
      SetVideoSend();
    }
  }
}

bool VideoRtpSender::SetTrack(MediaStreamTrackInterface* track) {
  TRACE_EVENT0("webrtc", "VideoRtpSender::SetTrack");
  if (stopped_) {
    RTC_LOG(LS_ERROR) << "SetTrack can't be called on a stopped RtpSender.";
    return false;
  }
  if (track && track->kind() != MediaStreamTrackInterface::kVideoKind) {
    RTC_LOG(LS_ERROR) << "SetTrack called on video RtpSender with "
                      << track->kind() << " track.";
    return false;
  }
  VideoTrackInterface* video_track = static_cast<VideoTrackInterface*>(track);

  // Detach from old track.
  if (track_) {
    track_->UnregisterObserver(this);
  }

  // Attach to new track.
  bool prev_can_send_track = can_send_track();
  // Keep a reference to the old track to keep it alive until we call
  // SetVideoSend.
  rtc::scoped_refptr<VideoTrackInterface> old_track = track_;
  track_ = video_track;
  if (track_) {
    cached_track_content_hint_ = track_->content_hint();
    track_->RegisterObserver(this);
  }

  // Update video channel.
  if (can_send_track()) {
    SetVideoSend();
  } else if (prev_can_send_track) {
    ClearVideoSend();
  }
  attachment_id_ = (track_ ? GenerateUniqueId() : 0);
  return true;
}

RtpParameters VideoRtpSender::GetParameters() {
  if (stopped_) {
    return RtpParameters();
  }
  if (!media_channel_) {
    RtpParameters result = init_parameters_;
    last_transaction_id_ = rtc::CreateRandomUuid();
    result.transaction_id = last_transaction_id_.value();
    return result;
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    RtpParameters result = media_channel_->GetRtpSendParameters(ssrc_);
    last_transaction_id_ = rtc::CreateRandomUuid();
    result.transaction_id = last_transaction_id_.value();
    return result;
  });
}

RTCError VideoRtpSender::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "VideoRtpSender::SetParameters");
  if (stopped_) {
    return RTCError(RTCErrorType::INVALID_STATE);
  }
  if (!last_transaction_id_) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_STATE,
        "Failed to set parameters since getParameters() has never been called"
        " on this sender");
  }
  if (last_transaction_id_ != parameters.transaction_id) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Failed to set parameters since the transaction_id doesn't match"
        " the last value returned from getParameters()");
  }

  if (UnimplementedRtpParameterHasValue(parameters)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::UNSUPPORTED_PARAMETER,
        "Attempted to set an unimplemented parameter of RtpParameters.");
  }
  if (!media_channel_) {
    auto result = cricket::ValidateRtpParameters(init_parameters_, parameters);
    if (result.ok()) {
      init_parameters_ = parameters;
    }
    return result;
  }
  return worker_thread_->Invoke<RTCError>(RTC_FROM_HERE, [&] {
    RTCError result = media_channel_->SetRtpSendParameters(ssrc_, parameters);
    last_transaction_id_.reset();
    return result;
  });
}

rtc::scoped_refptr<DtmfSenderInterface> VideoRtpSender::GetDtmfSender() const {
  RTC_LOG(LS_ERROR) << "Tried to get DTMF sender from video sender.";
  return nullptr;
}

void VideoRtpSender::SetFrameEncryptor(
    rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) {
  frame_encryptor_ = std::move(frame_encryptor);
  // Special Case: Set the frame encryptor to any value on any existing channel.
  if (media_channel_ && ssrc_ && !stopped_) {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      media_channel_->SetFrameEncryptor(ssrc_, frame_encryptor_);
    });
  }
}

rtc::scoped_refptr<FrameEncryptorInterface> VideoRtpSender::GetFrameEncryptor()
    const {
  return frame_encryptor_;
}

void VideoRtpSender::SetSsrc(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "VideoRtpSender::SetSsrc");
  if (stopped_ || ssrc == ssrc_) {
    return;
  }
  // If we are already sending with a particular SSRC, stop sending.
  if (can_send_track()) {
    ClearVideoSend();
  }
  ssrc_ = ssrc;
  if (can_send_track()) {
    SetVideoSend();
  }
  if (!init_parameters_.encodings.empty()) {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      RTC_DCHECK(media_channel_);
      // Get the current parameters, which are constructed from the SDP.
      // The number of layers in the SDP is currently authoritative to support
      // SDP munging for Plan-B simulcast with "a=ssrc-group:SIM <ssrc-id>..."
      // lines as described in RFC 5576.
      // All fields should be default constructed and the SSRC field set, which
      // we need to copy.
      RtpParameters current_parameters =
          media_channel_->GetRtpSendParameters(ssrc_);
      for (size_t i = 0; i < init_parameters_.encodings.size(); ++i) {
        init_parameters_.encodings[i].ssrc =
            current_parameters.encodings[i].ssrc;
        current_parameters.encodings[i] = init_parameters_.encodings[i];
      }
      current_parameters.degradation_preference =
          init_parameters_.degradation_preference;
      media_channel_->SetRtpSendParameters(ssrc_, current_parameters);
      init_parameters_.encodings.clear();
    });
  }
  MaybeAttachFrameEncryptorToMediaChannel(
      ssrc_, worker_thread_, frame_encryptor_, media_channel_, stopped_);
}

void VideoRtpSender::Stop() {
  TRACE_EVENT0("webrtc", "VideoRtpSender::Stop");
  // TODO(deadbeef): Need to do more here to fully stop sending packets.
  if (stopped_) {
    return;
  }
  if (track_) {
    track_->UnregisterObserver(this);
  }
  if (can_send_track()) {
    ClearVideoSend();
  }
  media_channel_ = nullptr;
  stopped_ = true;
}

void VideoRtpSender::SetMediaChannel(cricket::MediaChannel* media_channel) {
  RTC_DCHECK(media_channel == nullptr ||
             media_channel->media_type() == media_type());
  media_channel_ = static_cast<cricket::VideoMediaChannel*>(media_channel);
}

void VideoRtpSender::SetVideoSend() {
  RTC_DCHECK(!stopped_);
  RTC_DCHECK(can_send_track());
  if (!media_channel_) {
    RTC_LOG(LS_ERROR) << "SetVideoSend: No video channel exists.";
    return;
  }
  cricket::VideoOptions options;
  VideoTrackSourceInterface* source = track_->GetSource();
  if (source) {
    options.is_screencast = source->is_screencast();
    options.video_noise_reduction = source->needs_denoising();
  }
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
  bool success = worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetVideoSend(ssrc_, &options, track_);
  });
  RTC_DCHECK(success);
}

void VideoRtpSender::ClearVideoSend() {
  RTC_DCHECK(ssrc_ != 0);
  RTC_DCHECK(!stopped_);
  if (!media_channel_) {
    RTC_LOG(LS_WARNING) << "SetVideoSend: No video channel exists.";
    return;
  }
  // Allow SetVideoSend to fail since |enable| is false and |source| is null.
  // This the normal case when the underlying media channel has already been
  // deleted.
  worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetVideoSend(ssrc_, nullptr, nullptr);
  });
}

}  // namespace webrtc
