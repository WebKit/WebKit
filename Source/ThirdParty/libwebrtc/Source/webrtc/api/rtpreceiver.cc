/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtpreceiver.h"

#include "webrtc/api/mediastreamtrackproxy.h"
#include "webrtc/api/audiotrack.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/api/videotrack.h"
#include "webrtc/base/trace_event.h"

namespace webrtc {

AudioRtpReceiver::AudioRtpReceiver(MediaStreamInterface* stream,
                                   const std::string& track_id,
                                   uint32_t ssrc,
                                   cricket::VoiceChannel* channel)
    : id_(track_id),
      ssrc_(ssrc),
      channel_(channel),
      track_(AudioTrackProxy::Create(
          rtc::Thread::Current(),
          AudioTrack::Create(track_id,
                             RemoteAudioSource::Create(ssrc, channel)))),
      cached_track_enabled_(track_->enabled()) {
  RTC_DCHECK(track_->GetSource()->remote());
  track_->RegisterObserver(this);
  track_->GetSource()->RegisterAudioObserver(this);
  Reconfigure();
  stream->AddTrack(track_);
  if (channel_) {
    channel_->SignalFirstPacketReceived.connect(
        this, &AudioRtpReceiver::OnFirstPacketReceived);
  }
}

AudioRtpReceiver::~AudioRtpReceiver() {
  track_->GetSource()->UnregisterAudioObserver(this);
  track_->UnregisterObserver(this);
  Stop();
}

void AudioRtpReceiver::OnChanged() {
  if (cached_track_enabled_ != track_->enabled()) {
    cached_track_enabled_ = track_->enabled();
    Reconfigure();
  }
}

void AudioRtpReceiver::OnSetVolume(double volume) {
  RTC_DCHECK(volume >= 0 && volume <= 10);
  cached_volume_ = volume;
  if (!channel_) {
    LOG(LS_ERROR) << "AudioRtpReceiver::OnSetVolume: No audio channel exists.";
    return;
  }
  // When the track is disabled, the volume of the source, which is the
  // corresponding WebRtc Voice Engine channel will be 0. So we do not allow
  // setting the volume to the source when the track is disabled.
  if (!stopped_ && track_->enabled()) {
    if (!channel_->SetOutputVolume(ssrc_, cached_volume_)) {
      RTC_DCHECK(false);
    }
  }
}

RtpParameters AudioRtpReceiver::GetParameters() const {
  if (!channel_ || stopped_) {
    return RtpParameters();
  }
  return channel_->GetRtpReceiveParameters(ssrc_);
}

bool AudioRtpReceiver::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "AudioRtpReceiver::SetParameters");
  if (!channel_ || stopped_) {
    return false;
  }
  return channel_->SetRtpReceiveParameters(ssrc_, parameters);
}

void AudioRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  if (channel_) {
    // Allow that SetOutputVolume fail. This is the normal case when the
    // underlying media channel has already been deleted.
    channel_->SetOutputVolume(ssrc_, 0);
  }
  stopped_ = true;
}

void AudioRtpReceiver::Reconfigure() {
  RTC_DCHECK(!stopped_);
  if (!channel_) {
    LOG(LS_ERROR) << "AudioRtpReceiver::Reconfigure: No audio channel exists.";
    return;
  }
  if (!channel_->SetOutputVolume(ssrc_,
                                 track_->enabled() ? cached_volume_ : 0)) {
    RTC_DCHECK(false);
  }
}

void AudioRtpReceiver::SetObserver(RtpReceiverObserverInterface* observer) {
  observer_ = observer;
  // Deliver any notifications the observer may have missed by being set late.
  if (received_first_packet_) {
    observer_->OnFirstPacketReceived(media_type());
  }
}

void AudioRtpReceiver::SetChannel(cricket::VoiceChannel* channel) {
  if (channel_) {
    channel_->SignalFirstPacketReceived.disconnect(this);
  }
  channel_ = channel;
  if (channel_) {
    channel_->SignalFirstPacketReceived.connect(
        this, &AudioRtpReceiver::OnFirstPacketReceived);
  }
}

void AudioRtpReceiver::OnFirstPacketReceived(cricket::BaseChannel* channel) {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

VideoRtpReceiver::VideoRtpReceiver(MediaStreamInterface* stream,
                                   const std::string& track_id,
                                   rtc::Thread* worker_thread,
                                   uint32_t ssrc,
                                   cricket::VideoChannel* channel)
    : id_(track_id),
      ssrc_(ssrc),
      channel_(channel),
      source_(new RefCountedObject<VideoTrackSource>(&broadcaster_,
                                                     true /* remote */)),
      track_(VideoTrackProxy::Create(
          rtc::Thread::Current(),
          worker_thread,
          VideoTrack::Create(
              track_id,
              VideoTrackSourceProxy::Create(rtc::Thread::Current(),
                                            worker_thread,
                                            source_)))) {
  source_->SetState(MediaSourceInterface::kLive);
  if (!channel_) {
    LOG(LS_ERROR)
        << "VideoRtpReceiver::VideoRtpReceiver: No video channel exists.";
  } else {
    if (!channel_->SetSink(ssrc_, &broadcaster_)) {
      RTC_DCHECK(false);
    }
  }
  stream->AddTrack(track_);
  if (channel_) {
    channel_->SignalFirstPacketReceived.connect(
        this, &VideoRtpReceiver::OnFirstPacketReceived);
  }
}

VideoRtpReceiver::~VideoRtpReceiver() {
  // Since cricket::VideoRenderer is not reference counted,
  // we need to remove it from the channel before we are deleted.
  Stop();
}

RtpParameters VideoRtpReceiver::GetParameters() const {
  if (!channel_ || stopped_) {
    return RtpParameters();
  }
  return channel_->GetRtpReceiveParameters(ssrc_);
}

bool VideoRtpReceiver::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "VideoRtpReceiver::SetParameters");
  if (!channel_ || stopped_) {
    return false;
  }
  return channel_->SetRtpReceiveParameters(ssrc_, parameters);
}

void VideoRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  source_->SetState(MediaSourceInterface::kEnded);
  source_->OnSourceDestroyed();
  if (!channel_) {
    LOG(LS_WARNING) << "VideoRtpReceiver::Stop: No video channel exists.";
  } else {
    // Allow that SetSink fail. This is the normal case when the underlying
    // media channel has already been deleted.
    channel_->SetSink(ssrc_, nullptr);
  }
  stopped_ = true;
}

void VideoRtpReceiver::SetObserver(RtpReceiverObserverInterface* observer) {
  observer_ = observer;
  // Deliver any notifications the observer may have missed by being set late.
  if (received_first_packet_) {
    observer_->OnFirstPacketReceived(media_type());
  }
}

void VideoRtpReceiver::SetChannel(cricket::VideoChannel* channel) {
  if (channel_) {
    channel_->SignalFirstPacketReceived.disconnect(this);
    channel_->SetSink(ssrc_, nullptr);
  }
  channel_ = channel;
  if (channel_) {
    if (!channel_->SetSink(ssrc_, &broadcaster_)) {
      RTC_DCHECK(false);
    }
    channel_->SignalFirstPacketReceived.connect(
        this, &VideoRtpReceiver::OnFirstPacketReceived);
  }
}

void VideoRtpReceiver::OnFirstPacketReceived(cricket::BaseChannel* channel) {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

}  // namespace webrtc
