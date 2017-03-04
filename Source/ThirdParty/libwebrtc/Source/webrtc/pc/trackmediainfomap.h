/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_TRACKMEDIAINFOMAP_H_
#define WEBRTC_PC_TRACKMEDIAINFOMAP_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/rtpreceiverinterface.h"
#include "webrtc/api/rtpsenderinterface.h"
#include "webrtc/base/refcount.h"
#include "webrtc/media/base/mediachannel.h"

namespace webrtc {

// Audio/video tracks and sender/receiver statistical information are associated
// with each other based on attachments to RTP senders/receivers. This class
// maps that relationship, in both directions, so that stats about a track can
// be retrieved on a per-attachment basis.
//
// An RTP sender/receiver sends or receives media for a set of SSRCs. The media
// comes from an audio/video track that is attached to it.
// |[Voice/Video][Sender/Receiver]Info| has statistical information for a set of
// SSRCs. Looking at the RTP senders and receivers uncovers the track <-> info
// relationships, which this class does.
class TrackMediaInfoMap {
 public:
  TrackMediaInfoMap(
      std::unique_ptr<cricket::VoiceMediaInfo> voice_media_info,
      std::unique_ptr<cricket::VideoMediaInfo> video_media_info,
      const std::vector<rtc::scoped_refptr<RtpSenderInterface>>& rtp_senders,
      const std::vector<rtc::scoped_refptr<RtpReceiverInterface>>&
          rtp_receivers);

  const cricket::VoiceMediaInfo* voice_media_info() const {
    return voice_media_info_.get();
  }
  const cricket::VideoMediaInfo* video_media_info() const {
    return video_media_info_.get();
  }

  const std::vector<cricket::VoiceSenderInfo*>* GetVoiceSenderInfos(
      const AudioTrackInterface& local_audio_track) const;
  const cricket::VoiceReceiverInfo* GetVoiceReceiverInfo(
      const AudioTrackInterface& remote_audio_track) const;
  const std::vector<cricket::VideoSenderInfo*>* GetVideoSenderInfos(
      const VideoTrackInterface& local_video_track) const;
  const cricket::VideoReceiverInfo* GetVideoReceiverInfo(
      const VideoTrackInterface& remote_video_track) const;

  rtc::scoped_refptr<AudioTrackInterface> GetAudioTrack(
      const cricket::VoiceSenderInfo& voice_sender_info) const;
  rtc::scoped_refptr<AudioTrackInterface> GetAudioTrack(
      const cricket::VoiceReceiverInfo& voice_receiver_info) const;
  rtc::scoped_refptr<VideoTrackInterface> GetVideoTrack(
      const cricket::VideoSenderInfo& video_sender_info) const;
  rtc::scoped_refptr<VideoTrackInterface> GetVideoTrack(
      const cricket::VideoReceiverInfo& video_receiver_info) const;

 private:
  std::unique_ptr<cricket::VoiceMediaInfo> voice_media_info_;
  std::unique_ptr<cricket::VideoMediaInfo> video_media_info_;
  // These maps map tracks (identified by a pointer) to their corresponding info
  // object of the correct kind. One track can map to multiple info objects.
  std::map<const AudioTrackInterface*, std::vector<cricket::VoiceSenderInfo*>>
      voice_infos_by_local_track_;
  std::map<const AudioTrackInterface*, cricket::VoiceReceiverInfo*>
      voice_info_by_remote_track_;
  std::map<const VideoTrackInterface*, std::vector<cricket::VideoSenderInfo*>>
      video_infos_by_local_track_;
  std::map<const VideoTrackInterface*, cricket::VideoReceiverInfo*>
      video_info_by_remote_track_;
  // These maps map info objects to their corresponding tracks. They are always
  // the inverse of the maps above. One info object always maps to only one
  // track.
  std::map<const cricket::VoiceSenderInfo*,
           rtc::scoped_refptr<AudioTrackInterface>>
      audio_track_by_sender_info_;
  std::map<const cricket::VoiceReceiverInfo*,
           rtc::scoped_refptr<AudioTrackInterface>>
      audio_track_by_receiver_info_;
  std::map<const cricket::VideoSenderInfo*,
           rtc::scoped_refptr<VideoTrackInterface>>
      video_track_by_sender_info_;
  std::map<const cricket::VideoReceiverInfo*,
           rtc::scoped_refptr<VideoTrackInterface>>
      video_track_by_receiver_info_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_TRACKMEDIAINFOMAP_H_
