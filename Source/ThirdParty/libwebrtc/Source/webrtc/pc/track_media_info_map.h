/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TRACK_MEDIA_INFO_MAP_H_
#define PC_TRACK_MEDIA_INFO_MAP_H_

#include <cstdint>
#include <optional>
#include <string>

#include "api/array_view.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "media/base/media_channel.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {
class RtpSenderInternal;
class RtpReceiverInternal;

// Audio/video tracks and sender/receiver statistical information are associated
// with each other based on attachments to RTP senders/receivers. This class
// maps that relationship so that "infos" can be obtained from SSRCs and tracks
// can be obtained from "infos".
class TrackMediaInfoMap {
 public:
  struct RtpSenderSignalInfo {
    uint32_t ssrc = 0;
    int attachment_id = 0;
    MediaType media_type = MediaType::AUDIO;
  };

  struct RtpReceiverSignalInfo {
    std::string track_id;
    int attachment_id = 0;
    MediaType media_type = MediaType::AUDIO;
  };

  TrackMediaInfoMap(std::optional<VoiceMediaInfo> voice_media_info,
                    std::optional<VideoMediaInfo> video_media_info,
                    ArrayView<const RtpSenderSignalInfo> senders,
                    ArrayView<const RtpReceiverSignalInfo> receivers,
                    ArrayView<const RtpParameters> receiver_parameters);

  const std::optional<VoiceMediaInfo>& voice_media_info() const {
    return voice_media_info_;
  }
  const std::optional<VideoMediaInfo>& video_media_info() const {
    return video_media_info_;
  }

  const VoiceSenderInfo* GetVoiceSenderInfoBySsrc(uint32_t ssrc) const;
  const VoiceReceiverInfo* GetVoiceReceiverInfoBySsrc(uint32_t ssrc) const;
  const VideoSenderInfo* GetVideoSenderInfoBySsrc(uint32_t ssrc) const;
  const VideoReceiverInfo* GetVideoReceiverInfoBySsrc(uint32_t ssrc) const;

  std::optional<int> GetAttachmentIdBySsrc(uint32_t ssrc,
                                           MediaType media_type,
                                           bool is_sender) const;
  std::optional<std::string> GetReceiverTrackIdBySsrc(
      uint32_t ssrc,
      MediaType media_type) const;

 private:
  const std::optional<VoiceMediaInfo> voice_media_info_;
  const std::optional<VideoMediaInfo> video_media_info_;

  // Maps SSRC to Attachment ID/Track ID, split by media type to handle SSRC
  // reuse (e.g. same SSRC for Audio and Video on different processing chains)
  // and by direction to handle loopback (same SSRC for Sender and Receiver).
  const flat_map<uint32_t, int> audio_sender_attachment_id_by_ssrc_;
  const flat_map<uint32_t, int> video_sender_attachment_id_by_ssrc_;
  const flat_map<uint32_t, int> audio_receiver_attachment_id_by_ssrc_;
  const flat_map<uint32_t, int> video_receiver_attachment_id_by_ssrc_;
  const flat_map<uint32_t, std::string> audio_receiver_track_id_by_ssrc_;
  const flat_map<uint32_t, std::string> video_receiver_track_id_by_ssrc_;
  // These maps map SSRCs to the corresponding voice or video info objects.
  const flat_map<uint32_t, const VoiceSenderInfo*> voice_info_by_sender_ssrc_;
  const flat_map<uint32_t, const VideoSenderInfo*> video_info_by_sender_ssrc_;
};

}  // namespace webrtc

#endif  // PC_TRACK_MEDIA_INFO_MAP_H_
