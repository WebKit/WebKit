/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtpreceiver.h"

#include <utility>
#include <vector>

#include "api/mediastreamtrackproxy.h"
#include "api/videosourceproxy.h"
#include "pc/audiotrack.h"
#include "pc/videotrack.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

// This function is only expected to be called on the signalling thread.
int GenerateUniqueId() {
  static int g_unique_id = 0;

  return ++g_unique_id;
}

}  // namespace

AudioRtpReceiver::AudioRtpReceiver(
    rtc::Thread* worker_thread,
    const std::string& receiver_id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
    uint32_t ssrc,
    cricket::VoiceMediaChannel* media_channel)
    : worker_thread_(worker_thread),
      id_(receiver_id),
      ssrc_(ssrc),
      track_(AudioTrackProxy::Create(
          rtc::Thread::Current(),
          AudioTrack::Create(
              receiver_id,
              RemoteAudioSource::Create(worker_thread, media_channel, ssrc)))),
      cached_track_enabled_(track_->enabled()),
      attachment_id_(GenerateUniqueId()) {
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(track_->GetSource()->remote());
  track_->RegisterObserver(this);
  track_->GetSource()->RegisterAudioObserver(this);
  SetStreams(streams);
  SetMediaChannel(media_channel);
  Reconfigure();
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

bool AudioRtpReceiver::SetOutputVolume(double volume) {
  RTC_DCHECK_GE(volume, 0.0);
  RTC_DCHECK_LE(volume, 10.0);
  RTC_DCHECK(media_channel_);
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetOutputVolume(ssrc_, volume);
  });
}

void AudioRtpReceiver::OnSetVolume(double volume) {
  RTC_DCHECK_GE(volume, 0);
  RTC_DCHECK_LE(volume, 10);
  cached_volume_ = volume;
  if (!media_channel_) {
    RTC_LOG(LS_ERROR)
        << "AudioRtpReceiver::OnSetVolume: No audio channel exists.";
    return;
  }
  // When the track is disabled, the volume of the source, which is the
  // corresponding WebRtc Voice Engine channel will be 0. So we do not allow
  // setting the volume to the source when the track is disabled.
  if (!stopped_ && track_->enabled()) {
    if (!SetOutputVolume(cached_volume_)) {
      RTC_NOTREACHED();
    }
  }
}

RtpParameters AudioRtpReceiver::GetParameters() const {
  if (!media_channel_ || stopped_) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    return media_channel_->GetRtpReceiveParameters(ssrc_);
  });
}

bool AudioRtpReceiver::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "AudioRtpReceiver::SetParameters");
  if (!media_channel_ || stopped_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetRtpReceiveParameters(ssrc_, parameters);
  });
}

void AudioRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  if (media_channel_) {
    // Allow that SetOutputVolume fail. This is the normal case when the
    // underlying media channel has already been deleted.
    SetOutputVolume(0.0);
  }
  stopped_ = true;
}

void AudioRtpReceiver::SetStreams(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  // Remove remote track from any streams that are going away.
  for (auto existing_stream : streams_) {
    bool removed = true;
    for (auto stream : streams) {
      if (existing_stream->label() == stream->label()) {
        RTC_DCHECK_EQ(existing_stream.get(), stream.get());
        removed = false;
        break;
      }
    }
    if (removed) {
      existing_stream->RemoveTrack(track_);
    }
  }
  // Add remote track to any streams that are new.
  for (auto stream : streams) {
    bool added = true;
    for (auto existing_stream : streams_) {
      if (stream->label() == existing_stream->label()) {
        RTC_DCHECK_EQ(stream.get(), existing_stream.get());
        added = false;
        break;
      }
    }
    if (added) {
      stream->AddTrack(track_);
    }
  }
  streams_ = streams;
}

std::vector<RtpSource> AudioRtpReceiver::GetSources() const {
  return worker_thread_->Invoke<std::vector<RtpSource>>(
      RTC_FROM_HERE, [&] { return media_channel_->GetSources(ssrc_); });
}

void AudioRtpReceiver::Reconfigure() {
  RTC_DCHECK(!stopped_);
  if (!media_channel_) {
    RTC_LOG(LS_ERROR)
        << "AudioRtpReceiver::Reconfigure: No audio channel exists.";
    return;
  }
  if (!SetOutputVolume(track_->enabled() ? cached_volume_ : 0)) {
    RTC_NOTREACHED();
  }
}

void AudioRtpReceiver::SetObserver(RtpReceiverObserverInterface* observer) {
  observer_ = observer;
  // Deliver any notifications the observer may have missed by being set late.
  if (received_first_packet_ && observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
}

void AudioRtpReceiver::SetMediaChannel(
    cricket::VoiceMediaChannel* media_channel) {
  media_channel_ = media_channel;
}

void AudioRtpReceiver::NotifyFirstPacketReceived() {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

VideoRtpReceiver::VideoRtpReceiver(
    rtc::Thread* worker_thread,
    const std::string& receiver_id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams,
    uint32_t ssrc,
    cricket::VideoMediaChannel* media_channel)
    : worker_thread_(worker_thread),
      id_(receiver_id),
      ssrc_(ssrc),
      source_(new RefCountedObject<VideoTrackSource>(&broadcaster_,
                                                     true /* remote */)),
      track_(VideoTrackProxy::Create(
          rtc::Thread::Current(),
          worker_thread,
          VideoTrack::Create(
              receiver_id,
              VideoTrackSourceProxy::Create(rtc::Thread::Current(),
                                            worker_thread,
                                            source_),
              worker_thread))),
      attachment_id_(GenerateUniqueId()) {
  RTC_DCHECK(worker_thread_);
  SetStreams(streams);
  source_->SetState(MediaSourceInterface::kLive);
  SetMediaChannel(media_channel);
}

VideoRtpReceiver::~VideoRtpReceiver() {
  // Since cricket::VideoRenderer is not reference counted,
  // we need to remove it from the channel before we are deleted.
  Stop();
}

bool VideoRtpReceiver::SetSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK(media_channel_);
  return worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, [&] { return media_channel_->SetSink(ssrc_, sink); });
}

RtpParameters VideoRtpReceiver::GetParameters() const {
  if (!media_channel_ || stopped_) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    return media_channel_->GetRtpReceiveParameters(ssrc_);
  });
}

bool VideoRtpReceiver::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "VideoRtpReceiver::SetParameters");
  if (!media_channel_ || stopped_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetRtpReceiveParameters(ssrc_, parameters);
  });
}

void VideoRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  source_->SetState(MediaSourceInterface::kEnded);
  source_->OnSourceDestroyed();
  if (!media_channel_) {
    RTC_LOG(LS_WARNING) << "VideoRtpReceiver::Stop: No video channel exists.";
  } else {
    // Allow that SetSink fail. This is the normal case when the underlying
    // media channel has already been deleted.
    SetSink(nullptr);
  }
  stopped_ = true;
}

void VideoRtpReceiver::SetStreams(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  // Remove remote track from any streams that are going away.
  for (auto existing_stream : streams_) {
    bool removed = true;
    for (auto stream : streams) {
      if (existing_stream->label() == stream->label()) {
        RTC_DCHECK_EQ(existing_stream.get(), stream.get());
        removed = false;
        break;
      }
    }
    if (removed) {
      existing_stream->RemoveTrack(track_);
    }
  }
  // Add remote track to any streams that are new.
  for (auto stream : streams) {
    bool added = true;
    for (auto existing_stream : streams_) {
      if (stream->label() == existing_stream->label()) {
        RTC_DCHECK_EQ(stream.get(), existing_stream.get());
        added = false;
        break;
      }
    }
    if (added) {
      stream->AddTrack(track_);
    }
  }
  streams_ = streams;
}

void VideoRtpReceiver::SetObserver(RtpReceiverObserverInterface* observer) {
  observer_ = observer;
  // Deliver any notifications the observer may have missed by being set late.
  if (received_first_packet_ && observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
}

void VideoRtpReceiver::SetMediaChannel(
    cricket::VideoMediaChannel* media_channel) {
  if (media_channel_) {
    SetSink(nullptr);
  }
  media_channel_ = media_channel;
  if (media_channel_) {
    if (!SetSink(&broadcaster_)) {
      RTC_NOTREACHED();
    }
  }
}

void VideoRtpReceiver::NotifyFirstPacketReceived() {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

}  // namespace webrtc
