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

#include "api/mediastreamproxy.h"
#include "api/mediastreamtrackproxy.h"
#include "api/videosourceproxy.h"
#include "pc/audiotrack.h"
#include "pc/mediastream.h"
#include "pc/videotrack.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

// This function is only expected to be called on the signalling thread.
int GenerateUniqueId() {
  static int g_unique_id = 0;

  return ++g_unique_id;
}

std::vector<rtc::scoped_refptr<MediaStreamInterface>> CreateStreamsFromIds(
    std::vector<std::string> stream_ids) {
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams(
      stream_ids.size());
  for (size_t i = 0; i < stream_ids.size(); ++i) {
    streams[i] = MediaStreamProxy::Create(
        rtc::Thread::Current(), MediaStream::Create(std::move(stream_ids[i])));
  }
  return streams;
}

// Attempt to attach the frame decryptor to the current media channel on the
// correct worker thread only if both the media channel exists and a ssrc has
// been allocated to the stream.
void MaybeAttachFrameDecryptorToMediaChannel(
    const absl::optional<uint32_t>& ssrc,
    rtc::Thread* worker_thread,
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor,
    cricket::MediaChannel* media_channel,
    bool stopped) {
  if (media_channel && frame_decryptor && ssrc.has_value() && !stopped) {
    worker_thread->Invoke<void>(RTC_FROM_HERE, [&] {
      media_channel->SetFrameDecryptor(*ssrc, frame_decryptor);
    });
  }
}

}  // namespace

AudioRtpReceiver::AudioRtpReceiver(rtc::Thread* worker_thread,
                                   std::string receiver_id,
                                   std::vector<std::string> stream_ids)
    : AudioRtpReceiver(worker_thread,
                       receiver_id,
                       CreateStreamsFromIds(std::move(stream_ids))) {}

AudioRtpReceiver::AudioRtpReceiver(
    rtc::Thread* worker_thread,
    const std::string& receiver_id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams)
    : worker_thread_(worker_thread),
      id_(receiver_id),
      source_(new rtc::RefCountedObject<RemoteAudioSource>(worker_thread)),
      track_(AudioTrackProxy::Create(rtc::Thread::Current(),
                                     AudioTrack::Create(receiver_id, source_))),
      cached_track_enabled_(track_->enabled()),
      attachment_id_(GenerateUniqueId()) {
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(track_->GetSource()->remote());
  track_->RegisterObserver(this);
  track_->GetSource()->RegisterAudioObserver(this);
  SetStreams(streams);
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
  RTC_DCHECK(ssrc_);
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetOutputVolume(*ssrc_, volume);
  });
}

void AudioRtpReceiver::OnSetVolume(double volume) {
  RTC_DCHECK_GE(volume, 0);
  RTC_DCHECK_LE(volume, 10);
  cached_volume_ = volume;
  if (!media_channel_ || !ssrc_) {
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

std::vector<std::string> AudioRtpReceiver::stream_ids() const {
  std::vector<std::string> stream_ids(streams_.size());
  for (size_t i = 0; i < streams_.size(); ++i)
    stream_ids[i] = streams_[i]->id();
  return stream_ids;
}

RtpParameters AudioRtpReceiver::GetParameters() const {
  if (!media_channel_ || !ssrc_ || stopped_) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    return media_channel_->GetRtpReceiveParameters(*ssrc_);
  });
}

bool AudioRtpReceiver::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "AudioRtpReceiver::SetParameters");
  if (!media_channel_ || !ssrc_ || stopped_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetRtpReceiveParameters(*ssrc_, parameters);
  });
}

void AudioRtpReceiver::SetFrameDecryptor(
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) {
  frame_decryptor_ = std::move(frame_decryptor);
  // Special Case: Set the frame decryptor to any value on any existing channel.
  if (media_channel_ && ssrc_.has_value() && !stopped_) {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      media_channel_->SetFrameDecryptor(*ssrc_, frame_decryptor_);
    });
  }
}

rtc::scoped_refptr<FrameDecryptorInterface>
AudioRtpReceiver::GetFrameDecryptor() const {
  return frame_decryptor_;
}

void AudioRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  if (media_channel_ && ssrc_) {
    // Allow that SetOutputVolume fail. This is the normal case when the
    // underlying media channel has already been deleted.
    SetOutputVolume(0.0);
  }
  stopped_ = true;
}

void AudioRtpReceiver::SetupMediaChannel(uint32_t ssrc) {
  if (!media_channel_) {
    RTC_LOG(LS_ERROR)
        << "AudioRtpReceiver::SetupMediaChannel: No audio channel exists.";
    return;
  }
  if (ssrc_ == ssrc) {
    return;
  }
  if (ssrc_) {
    source_->Stop(media_channel_, *ssrc_);
  }
  ssrc_ = ssrc;
  source_->Start(media_channel_, *ssrc_);
  Reconfigure();
}

void AudioRtpReceiver::set_stream_ids(std::vector<std::string> stream_ids) {
  SetStreams(CreateStreamsFromIds(std::move(stream_ids)));
}

void AudioRtpReceiver::SetStreams(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  // Remove remote track from any streams that are going away.
  for (auto existing_stream : streams_) {
    bool removed = true;
    for (auto stream : streams) {
      if (existing_stream->id() == stream->id()) {
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
      if (stream->id() == existing_stream->id()) {
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
  if (!media_channel_ || !ssrc_ || stopped_) {
    return {};
  }
  return worker_thread_->Invoke<std::vector<RtpSource>>(
      RTC_FROM_HERE, [&] { return media_channel_->GetSources(*ssrc_); });
}

void AudioRtpReceiver::Reconfigure() {
  RTC_DCHECK(!stopped_);
  if (!media_channel_ || !ssrc_) {
    RTC_LOG(LS_ERROR)
        << "AudioRtpReceiver::Reconfigure: No audio channel exists.";
    return;
  }
  if (!SetOutputVolume(track_->enabled() ? cached_volume_ : 0)) {
    RTC_NOTREACHED();
  }
  // Reattach the frame decryptor if we were reconfigured.
  MaybeAttachFrameDecryptorToMediaChannel(
      ssrc_, worker_thread_, frame_decryptor_, media_channel_, stopped_);
}

void AudioRtpReceiver::SetObserver(RtpReceiverObserverInterface* observer) {
  observer_ = observer;
  // Deliver any notifications the observer may have missed by being set late.
  if (received_first_packet_ && observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
}

void AudioRtpReceiver::SetMediaChannel(cricket::MediaChannel* media_channel) {
  RTC_DCHECK(media_channel == nullptr ||
             media_channel->media_type() == media_type());
  media_channel_ = static_cast<cricket::VoiceMediaChannel*>(media_channel);
}

void AudioRtpReceiver::NotifyFirstPacketReceived() {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

VideoRtpReceiver::VideoRtpReceiver(rtc::Thread* worker_thread,
                                   std::string receiver_id,
                                   std::vector<std::string> stream_ids)
    : VideoRtpReceiver(worker_thread,
                       receiver_id,
                       CreateStreamsFromIds(std::move(stream_ids))) {}

VideoRtpReceiver::VideoRtpReceiver(
    rtc::Thread* worker_thread,
    const std::string& receiver_id,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams)
    : worker_thread_(worker_thread),
      id_(receiver_id),
      source_(new RefCountedObject<VideoRtpTrackSource>()),
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
}

VideoRtpReceiver::~VideoRtpReceiver() {
  // Since cricket::VideoRenderer is not reference counted,
  // we need to remove it from the channel before we are deleted.
  Stop();
}

std::vector<std::string> VideoRtpReceiver::stream_ids() const {
  std::vector<std::string> stream_ids(streams_.size());
  for (size_t i = 0; i < streams_.size(); ++i)
    stream_ids[i] = streams_[i]->id();
  return stream_ids;
}

bool VideoRtpReceiver::SetSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK(media_channel_);
  RTC_DCHECK(ssrc_);
  return worker_thread_->Invoke<bool>(
      RTC_FROM_HERE, [&] { return media_channel_->SetSink(*ssrc_, sink); });
}

RtpParameters VideoRtpReceiver::GetParameters() const {
  if (!media_channel_ || !ssrc_ || stopped_) {
    return RtpParameters();
  }
  return worker_thread_->Invoke<RtpParameters>(RTC_FROM_HERE, [&] {
    return media_channel_->GetRtpReceiveParameters(*ssrc_);
  });
}

bool VideoRtpReceiver::SetParameters(const RtpParameters& parameters) {
  TRACE_EVENT0("webrtc", "VideoRtpReceiver::SetParameters");
  if (!media_channel_ || !ssrc_ || stopped_) {
    return false;
  }
  return worker_thread_->Invoke<bool>(RTC_FROM_HERE, [&] {
    return media_channel_->SetRtpReceiveParameters(*ssrc_, parameters);
  });
}

void VideoRtpReceiver::SetFrameDecryptor(
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) {
  frame_decryptor_ = std::move(frame_decryptor);
  // Special Case: Set the frame decryptor to any value on any existing channel.
  if (media_channel_ && ssrc_.has_value() && !stopped_) {
    worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
      media_channel_->SetFrameDecryptor(*ssrc_, frame_decryptor_);
    });
  }
}

rtc::scoped_refptr<FrameDecryptorInterface>
VideoRtpReceiver::GetFrameDecryptor() const {
  return frame_decryptor_;
}

void VideoRtpReceiver::Stop() {
  // TODO(deadbeef): Need to do more here to fully stop receiving packets.
  if (stopped_) {
    return;
  }
  source_->SetState(MediaSourceInterface::kEnded);
  if (!media_channel_ || !ssrc_) {
    RTC_LOG(LS_WARNING) << "VideoRtpReceiver::Stop: No video channel exists.";
  } else {
    // Allow that SetSink fail. This is the normal case when the underlying
    // media channel has already been deleted.
    SetSink(nullptr);
  }
  stopped_ = true;
}

void VideoRtpReceiver::SetupMediaChannel(uint32_t ssrc) {
  if (!media_channel_) {
    RTC_LOG(LS_ERROR)
        << "VideoRtpReceiver::SetupMediaChannel: No video channel exists.";
  }
  if (ssrc_ == ssrc) {
    return;
  }
  if (ssrc_) {
    SetSink(nullptr);
  }
  ssrc_ = ssrc;
  SetSink(source_->sink());
  // Attach any existing frame decryptor to the media channel.
  MaybeAttachFrameDecryptorToMediaChannel(
      ssrc_, worker_thread_, frame_decryptor_, media_channel_, stopped_);
}

void VideoRtpReceiver::set_stream_ids(std::vector<std::string> stream_ids) {
  SetStreams(CreateStreamsFromIds(std::move(stream_ids)));
}

void VideoRtpReceiver::SetStreams(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  // Remove remote track from any streams that are going away.
  for (auto existing_stream : streams_) {
    bool removed = true;
    for (auto stream : streams) {
      if (existing_stream->id() == stream->id()) {
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
      if (stream->id() == existing_stream->id()) {
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

void VideoRtpReceiver::SetMediaChannel(cricket::MediaChannel* media_channel) {
  RTC_DCHECK(media_channel == nullptr ||
             media_channel->media_type() == media_type());
  media_channel_ = static_cast<cricket::VideoMediaChannel*>(media_channel);
}

void VideoRtpReceiver::NotifyFirstPacketReceived() {
  if (observer_) {
    observer_->OnFirstPacketReceived(media_type());
  }
  received_first_packet_ = true;
}

std::vector<RtpSource> VideoRtpReceiver::GetSources() const {
  if (!media_channel_ || !ssrc_ || stopped_) {
    return {};
  }
  return worker_thread_->Invoke<std::vector<RtpSource>>(
      RTC_FROM_HERE, [&] { return media_channel_->GetSources(*ssrc_); });
}

}  // namespace webrtc
