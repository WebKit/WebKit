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

#include <vector>

#include "api/mediastreaminterface.h"
#include "pc/localaudiosource.h"
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

}  // namespace

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
                               StatsCollector* stats)
    : AudioRtpSender(worker_thread, nullptr, {rtc::CreateRandomUuid()}, stats) {
}

AudioRtpSender::AudioRtpSender(rtc::Thread* worker_thread,
                               rtc::scoped_refptr<AudioTrackInterface> track,
                               const std::vector<std::string>& stream_labels,
                               StatsCollector* stats)
    : worker_thread_(worker_thread),
      id_(track ? track->id() : rtc::CreateRandomUuid()),
      stream_ids_(stream_labels),
      stats_(stats),
      track_(track),
      dtmf_sender_proxy_(DtmfSenderProxy::Create(
          rtc::Thread::Current(),
          DtmfSender::Create(track_, rtc::Thread::Current(), this))),
      cached_track_enabled_(track ? track->enabled() : false),
      sink_adapter_(new LocalAudioSinkAdapter()),
      attachment_id_(track ? GenerateUniqueId() : 0) {
  RTC_DCHECK(worker_thread);
  // TODO(bugs.webrtc.org/7932): Remove once zero or multiple streams are
  // supported.
  RTC_DCHECK_EQ(stream_labels.size(), 1u);
  if (track_) {
    track_->RegisterObserver(this);
    track_->AddSink(sink_adapter_.get());
  }
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
    RTC_LOG(LS_ERROR) << "CanInsertDtmf: No audio channel exists.";
    return false;
  }
  if (!ssrc_) {
    RTC_LOG(LS_ERROR) << "CanInsertDtmf: Sender does not have SSRC.";
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
  attachment_id_ = GenerateUniqueId();
  return true;
}

RtpParameters AudioRtpSender::GetParameters() const {
  if (!media_channel_ || stopped_) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    return media_channel_->GetRtpSendParameters(ssrc_);
  });
}

bool AudioRtpSender::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "AudioRtpSender::SetParameters");
  if (!media_channel_ || stopped_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetRtpSendParameters(ssrc_, parameters);
  });
}

rtc::scoped_refptr<DtmfSenderInterface> AudioRtpSender::GetDtmfSender() const {
  return dtmf_sender_proxy_;
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
  stopped_ = true;
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
    // TODO(xians): Remove this static_cast since we should be able to connect
    // a remote audio track to a peer connection.
    options = static_cast<LocalAudioSource*>(track_->GetSource())->options();
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

VideoRtpSender::VideoRtpSender(rtc::Thread* worker_thread)
    : VideoRtpSender(worker_thread, nullptr, {rtc::CreateRandomUuid()}) {}

VideoRtpSender::VideoRtpSender(rtc::Thread* worker_thread,
                               rtc::scoped_refptr<VideoTrackInterface> track,
                               const std::vector<std::string>& stream_labels)
    : worker_thread_(worker_thread),
      id_(track ? track->id() : rtc::CreateRandomUuid()),
      stream_ids_(stream_labels),
      track_(track),
      cached_track_enabled_(track ? track->enabled() : false),
      cached_track_content_hint_(
          track ? track->content_hint()
                : VideoTrackInterface::ContentHint::kNone),
      attachment_id_(track ? GenerateUniqueId() : 0) {
  RTC_DCHECK(worker_thread);
  // TODO(bugs.webrtc.org/7932): Remove once zero or multiple streams are
  // supported.
  RTC_DCHECK_EQ(stream_labels.size(), 1u);
  if (track_) {
    track_->RegisterObserver(this);
  }
}

VideoRtpSender::~VideoRtpSender() {
  Stop();
}

void VideoRtpSender::OnChanged() {
  TRACE_EVENT0("webrtc", "VideoRtpSender::OnChanged");
  RTC_DCHECK(!stopped_);
  if (cached_track_enabled_ != track_->enabled() ||
      cached_track_content_hint_ != track_->content_hint()) {
    cached_track_enabled_ = track_->enabled();
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
    cached_track_enabled_ = track_->enabled();
    cached_track_content_hint_ = track_->content_hint();
    track_->RegisterObserver(this);
  }

  // Update video channel.
  if (can_send_track()) {
    SetVideoSend();
  } else if (prev_can_send_track) {
    ClearVideoSend();
  }
  attachment_id_ = GenerateUniqueId();
  return true;
}

RtpParameters VideoRtpSender::GetParameters() const {
  if (!media_channel_ || stopped_) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    return media_channel_->GetRtpSendParameters(ssrc_);
  });
}

bool VideoRtpSender::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "VideoRtpSender::SetParameters");
  if (!media_channel_ || stopped_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetRtpSendParameters(ssrc_, parameters);
  });
}

rtc::scoped_refptr<DtmfSenderInterface> VideoRtpSender::GetDtmfSender() const {
  RTC_LOG(LS_ERROR) << "Tried to get DTMF sender from video sender.";
  return nullptr;
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
  stopped_ = true;
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
      options.is_screencast = true;
      break;
  }
  // |track_->enabled()| hops to the signaling thread, so call it before we hop
  // to the worker thread or else it will deadlock.
  bool track_enabled = track_->enabled();
  bool success = worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetVideoSend(ssrc_, track_enabled, &options, track_);
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
    return media_channel_->SetVideoSend(ssrc_, false, nullptr, nullptr);
  });
}

}  // namespace webrtc
