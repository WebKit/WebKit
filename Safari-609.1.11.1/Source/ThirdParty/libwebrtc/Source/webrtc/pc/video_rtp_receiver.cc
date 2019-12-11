/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_receiver.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "api/media_stream_proxy.h"
#include "api/media_stream_track_proxy.h"
#include "api/video_track_source_proxy.h"
#include "pc/jitter_buffer_delay.h"
#include "pc/jitter_buffer_delay_proxy.h"
#include "pc/media_stream.h"
#include "pc/video_track.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

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
      attachment_id_(GenerateUniqueId()),
      delay_(JitterBufferDelayProxy::Create(
          rtc::Thread::Current(),
          worker_thread,
          new rtc::RefCountedObject<JitterBufferDelay>(worker_thread))) {
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
  delay_->OnStop();
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

  delay_->OnStart(media_channel_, ssrc);
}

void VideoRtpReceiver::set_stream_ids(std::vector<std::string> stream_ids) {
  SetStreams(CreateStreamsFromIds(std::move(stream_ids)));
}

void VideoRtpReceiver::SetStreams(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  // Remove remote track from any streams that are going away.
  for (const auto& existing_stream : streams_) {
    bool removed = true;
    for (const auto& stream : streams) {
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
  for (const auto& stream : streams) {
    bool added = true;
    for (const auto& existing_stream : streams_) {
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

void VideoRtpReceiver::SetJitterBufferMinimumDelay(
    absl::optional<double> delay_seconds) {
  delay_->Set(delay_seconds);
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
