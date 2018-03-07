/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/trackmediainfomap.h"

#include <utility>

namespace webrtc {

namespace {

template <typename K, typename V>
V FindValueOrNull(const std::map<K, V>& map, const K& key) {
  auto it = map.find(key);
  return (it != map.end()) ? it->second : nullptr;
}

template <typename K, typename V>
const V* FindAddressOrNull(const std::map<K, V>& map, const K& key) {
  auto it = map.find(key);
  return (it != map.end()) ? &it->second : nullptr;
}

void GetAudioAndVideoTrackBySsrc(
    const std::vector<rtc::scoped_refptr<RtpSenderInterface>>& rtp_senders,
    const std::vector<rtc::scoped_refptr<RtpReceiverInterface>>& rtp_receivers,
    std::map<uint32_t, AudioTrackInterface*>* local_audio_track_by_ssrc,
    std::map<uint32_t, VideoTrackInterface*>* local_video_track_by_ssrc,
    std::map<uint32_t, AudioTrackInterface*>* remote_audio_track_by_ssrc,
    std::map<uint32_t, VideoTrackInterface*>* remote_video_track_by_ssrc,
    AudioTrackInterface** unsignaled_audio_track,
    VideoTrackInterface** unsignaled_video_track) {
  RTC_DCHECK(local_audio_track_by_ssrc->empty());
  RTC_DCHECK(local_video_track_by_ssrc->empty());
  RTC_DCHECK(remote_audio_track_by_ssrc->empty());
  RTC_DCHECK(remote_video_track_by_ssrc->empty());
  // TODO(hbos): RTP senders/receivers uses a proxy to the signaling thread, and
  // our sender/receiver implementations invokes on the worker thread. (This
  // means one thread jump if on signaling thread and two thread jumps if on any
  // other threads). Is there a way to avoid thread jump(s) on a per
  // sender/receiver, per method basis?
  for (const rtc::scoped_refptr<RtpSenderInterface>& rtp_sender : rtp_senders) {
    cricket::MediaType media_type = rtp_sender->media_type();
    MediaStreamTrackInterface* track = rtp_sender->track();
    if (!track) {
      continue;
    }
    RTC_DCHECK_EQ(track->kind(), media_type == cricket::MEDIA_TYPE_AUDIO
                                     ? MediaStreamTrackInterface::kAudioKind
                                     : MediaStreamTrackInterface::kVideoKind);
    // TODO(deadbeef): |ssrc| should be removed in favor of |GetParameters|.
    uint32_t ssrc = rtp_sender->ssrc();
    if (ssrc != 0) {
      if (media_type == cricket::MEDIA_TYPE_AUDIO) {
        RTC_DCHECK(local_audio_track_by_ssrc->find(ssrc) ==
                   local_audio_track_by_ssrc->end());
        (*local_audio_track_by_ssrc)[ssrc] =
            static_cast<AudioTrackInterface*>(track);
      } else {
        RTC_DCHECK(local_video_track_by_ssrc->find(ssrc) ==
                   local_video_track_by_ssrc->end());
        (*local_video_track_by_ssrc)[ssrc] =
            static_cast<VideoTrackInterface*>(track);
      }
    }
  }
  for (const rtc::scoped_refptr<RtpReceiverInterface>& rtp_receiver :
       rtp_receivers) {
    cricket::MediaType media_type = rtp_receiver->media_type();
    MediaStreamTrackInterface* track = rtp_receiver->track();
    RTC_DCHECK(track);
    RTC_DCHECK_EQ(track->kind(), media_type == cricket::MEDIA_TYPE_AUDIO
                                     ? MediaStreamTrackInterface::kAudioKind
                                     : MediaStreamTrackInterface::kVideoKind);
    RtpParameters params = rtp_receiver->GetParameters();
    for (const RtpEncodingParameters& encoding : params.encodings) {
      if (!encoding.ssrc) {
        if (media_type == cricket::MEDIA_TYPE_AUDIO) {
          *unsignaled_audio_track = static_cast<AudioTrackInterface*>(track);
        } else {
          RTC_DCHECK(media_type == cricket::MEDIA_TYPE_VIDEO);
          *unsignaled_video_track = static_cast<VideoTrackInterface*>(track);
        }
        continue;
      }
      if (media_type == cricket::MEDIA_TYPE_AUDIO) {
        RTC_DCHECK(remote_audio_track_by_ssrc->find(*encoding.ssrc) ==
                   remote_audio_track_by_ssrc->end());
        (*remote_audio_track_by_ssrc)[*encoding.ssrc] =
            static_cast<AudioTrackInterface*>(track);
      } else {
        RTC_DCHECK(remote_video_track_by_ssrc->find(*encoding.ssrc) ==
                   remote_video_track_by_ssrc->end());
        (*remote_video_track_by_ssrc)[*encoding.ssrc] =
            static_cast<VideoTrackInterface*>(track);
      }
    }
  }
}

}  // namespace

TrackMediaInfoMap::TrackMediaInfoMap(
    std::unique_ptr<cricket::VoiceMediaInfo> voice_media_info,
    std::unique_ptr<cricket::VideoMediaInfo> video_media_info,
    const std::vector<rtc::scoped_refptr<RtpSenderInterface>>& rtp_senders,
    const std::vector<rtc::scoped_refptr<RtpReceiverInterface>>& rtp_receivers)
    : voice_media_info_(std::move(voice_media_info)),
      video_media_info_(std::move(video_media_info)) {
  std::map<uint32_t, AudioTrackInterface*> local_audio_track_by_ssrc;
  std::map<uint32_t, VideoTrackInterface*> local_video_track_by_ssrc;
  std::map<uint32_t, AudioTrackInterface*> remote_audio_track_by_ssrc;
  std::map<uint32_t, VideoTrackInterface*> remote_video_track_by_ssrc;
  AudioTrackInterface* unsignaled_audio_track = nullptr;
  VideoTrackInterface* unsignaled_video_track = nullptr;
  GetAudioAndVideoTrackBySsrc(
      rtp_senders, rtp_receivers, &local_audio_track_by_ssrc,
      &local_video_track_by_ssrc, &remote_audio_track_by_ssrc,
      &remote_video_track_by_ssrc, &unsignaled_audio_track,
      &unsignaled_video_track);
  if (voice_media_info_) {
    for (auto& sender_info : voice_media_info_->senders) {
      AudioTrackInterface* associated_track =
          FindValueOrNull(local_audio_track_by_ssrc, sender_info.ssrc());
      if (associated_track) {
        // One sender is associated with at most one track.
        // One track may be associated with multiple senders.
        audio_track_by_sender_info_[&sender_info] = associated_track;
        voice_infos_by_local_track_[associated_track].push_back(&sender_info);
      }
    }
    for (auto& receiver_info : voice_media_info_->receivers) {
      AudioTrackInterface* associated_track =
          FindValueOrNull(remote_audio_track_by_ssrc, receiver_info.ssrc());
      if (associated_track) {
        // One receiver is associated with at most one track, which is uniquely
        // associated with that receiver.
        audio_track_by_receiver_info_[&receiver_info] = associated_track;
        RTC_DCHECK(voice_info_by_remote_track_.find(associated_track) ==
                   voice_info_by_remote_track_.end());
        voice_info_by_remote_track_[associated_track] = &receiver_info;
      } else if (unsignaled_audio_track) {
        audio_track_by_receiver_info_[&receiver_info] = unsignaled_audio_track;
        voice_info_by_remote_track_[unsignaled_audio_track] = &receiver_info;
      }
    }
  }
  if (video_media_info_) {
    for (auto& sender_info : video_media_info_->senders) {
      VideoTrackInterface* associated_track =
          FindValueOrNull(local_video_track_by_ssrc, sender_info.ssrc());
      if (associated_track) {
        // One sender is associated with at most one track.
        // One track may be associated with multiple senders.
        video_track_by_sender_info_[&sender_info] = associated_track;
        video_infos_by_local_track_[associated_track].push_back(&sender_info);
      }
    }
    for (auto& receiver_info : video_media_info_->receivers) {
      VideoTrackInterface* associated_track =
          FindValueOrNull(remote_video_track_by_ssrc, receiver_info.ssrc());
      if (associated_track) {
        // One receiver is associated with at most one track, which is uniquely
        // associated with that receiver.
        video_track_by_receiver_info_[&receiver_info] = associated_track;
        RTC_DCHECK(video_info_by_remote_track_.find(associated_track) ==
                   video_info_by_remote_track_.end());
        video_info_by_remote_track_[associated_track] = &receiver_info;
      } else if (unsignaled_video_track) {
        video_track_by_receiver_info_[&receiver_info] = unsignaled_video_track;
        video_info_by_remote_track_[unsignaled_video_track] = &receiver_info;
      }
    }
  }
}

const std::vector<cricket::VoiceSenderInfo*>*
TrackMediaInfoMap::GetVoiceSenderInfos(
    const AudioTrackInterface& local_audio_track) const {
  return FindAddressOrNull(voice_infos_by_local_track_, &local_audio_track);
}

const cricket::VoiceReceiverInfo* TrackMediaInfoMap::GetVoiceReceiverInfo(
    const AudioTrackInterface& remote_audio_track) const {
  return FindValueOrNull(voice_info_by_remote_track_, &remote_audio_track);
}

const std::vector<cricket::VideoSenderInfo*>*
TrackMediaInfoMap::GetVideoSenderInfos(
    const VideoTrackInterface& local_video_track) const {
  return FindAddressOrNull(video_infos_by_local_track_, &local_video_track);
}

const cricket::VideoReceiverInfo* TrackMediaInfoMap::GetVideoReceiverInfo(
    const VideoTrackInterface& remote_video_track) const {
  return FindValueOrNull(video_info_by_remote_track_, &remote_video_track);
}

rtc::scoped_refptr<AudioTrackInterface> TrackMediaInfoMap::GetAudioTrack(
    const cricket::VoiceSenderInfo& voice_sender_info) const {
  return FindValueOrNull(audio_track_by_sender_info_, &voice_sender_info);
}

rtc::scoped_refptr<AudioTrackInterface> TrackMediaInfoMap::GetAudioTrack(
    const cricket::VoiceReceiverInfo& voice_receiver_info) const {
  return FindValueOrNull(audio_track_by_receiver_info_, &voice_receiver_info);
}

rtc::scoped_refptr<VideoTrackInterface> TrackMediaInfoMap::GetVideoTrack(
    const cricket::VideoSenderInfo& video_sender_info) const {
  return FindValueOrNull(video_track_by_sender_info_, &video_sender_info);
}

rtc::scoped_refptr<VideoTrackInterface> TrackMediaInfoMap::GetVideoTrack(
    const cricket::VideoReceiverInfo& video_receiver_info) const {
  return FindValueOrNull(video_track_by_receiver_info_, &video_receiver_info);
}

}  // namespace webrtc
