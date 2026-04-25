/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/track_media_info_map.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "api/array_view.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "media/base/media_channel.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {

namespace {

template <typename K, typename V>
V FindValueOrNull(const flat_map<K, V>& map, const K& key) {
  auto it = map.find(key);
  return (it != map.end()) ? it->second : nullptr;
}

template <typename K, typename V>
const V* FindAddressOrNull(const flat_map<K, V>& map, const K& key) {
  auto it = map.find(key);
  return (it != map.end()) ? &it->second : nullptr;
}

flat_map<uint32_t, int> GetSenderAttachmentIds(
    ArrayView<const TrackMediaInfoMap::RtpSenderSignalInfo> senders,
    MediaType media_type) {
  flat_map<uint32_t, int> result;
  for (const auto& sender : senders) {
    if (sender.media_type != media_type || sender.ssrc == 0) {
      continue;
    }
    result[sender.ssrc] = sender.attachment_id;
  }
  return result;
}

flat_map<uint32_t, int> GetReceiverAttachmentIds(
    ArrayView<const TrackMediaInfoMap::RtpReceiverSignalInfo> receivers,
    ArrayView<const RtpParameters> receiver_parameters,
    MediaType media_type) {
  RTC_DCHECK_EQ(receivers.size(), receiver_parameters.size());
  flat_map<uint32_t, int> result;
  for (size_t i = 0; i < receivers.size(); ++i) {
    if (receivers[i].media_type != media_type) {
      continue;
    }
    for (const RtpEncodingParameters& encoding :
         receiver_parameters[i].encodings) {
      if (encoding.ssrc) {
        result[*encoding.ssrc] = receivers[i].attachment_id;
      }
    }
  }
  return result;
}

flat_map<uint32_t, std::string> GetReceiverTrackIds(
    ArrayView<const TrackMediaInfoMap::RtpReceiverSignalInfo> receivers,
    ArrayView<const RtpParameters> receiver_parameters,
    MediaType media_type) {
  RTC_DCHECK_EQ(receivers.size(), receiver_parameters.size());
  flat_map<uint32_t, std::string> result;
  for (size_t i = 0; i < receivers.size(); ++i) {
    if (receivers[i].media_type != media_type) {
      continue;
    }
    for (const RtpEncodingParameters& encoding :
         receiver_parameters[i].encodings) {
      if (encoding.ssrc) {
        result[*encoding.ssrc] = receivers[i].track_id;
      }
    }
  }
  return result;
}

flat_map<uint32_t, const VoiceSenderInfo*> GetVoiceSenderInfos(
    const std::optional<VoiceMediaInfo>& voice_media_info) {
  flat_map<uint32_t, const VoiceSenderInfo*> result;
  if (!voice_media_info.has_value()) {
    return result;
  }
  for (auto& sender_info : voice_media_info->senders) {
    if (sender_info.ssrc() == 0) {
      continue;
    }
    RTC_CHECK(result.find(sender_info.ssrc()) == result.end())
        << "Duplicate voice sender SSRC: " << sender_info.ssrc();
    result[sender_info.ssrc()] = &sender_info;
  }
  return result;
}

flat_map<uint32_t, const VideoSenderInfo*> GetVideoSenderInfos(
    const std::optional<VideoMediaInfo>& video_media_info) {
  flat_map<uint32_t, const VideoSenderInfo*> result;
  if (!video_media_info.has_value()) {
    return result;
  }
  for (auto& sender_info : video_media_info->aggregated_senders) {
    if (sender_info.ssrc() == 0) {
      continue;
    }
    RTC_DCHECK(result.find(sender_info.ssrc()) == result.end())
        << "Duplicate video sender SSRC: " << sender_info.ssrc();
    result[sender_info.ssrc()] = &sender_info;
  }
  return result;
}

}  // namespace

TrackMediaInfoMap::TrackMediaInfoMap(
    std::optional<VoiceMediaInfo> voice_media_info,
    std::optional<VideoMediaInfo> video_media_info,
    ArrayView<const RtpSenderSignalInfo> senders,
    ArrayView<const RtpReceiverSignalInfo> receivers,
    ArrayView<const RtpParameters> receiver_parameters)
    : voice_media_info_(std::move(voice_media_info)),
      video_media_info_(std::move(video_media_info)),
      audio_sender_attachment_id_by_ssrc_(
          GetSenderAttachmentIds(senders, MediaType::AUDIO)),
      video_sender_attachment_id_by_ssrc_(
          GetSenderAttachmentIds(senders, MediaType::VIDEO)),
      audio_receiver_attachment_id_by_ssrc_(
          GetReceiverAttachmentIds(receivers,
                                   receiver_parameters,
                                   MediaType::AUDIO)),
      video_receiver_attachment_id_by_ssrc_(
          GetReceiverAttachmentIds(receivers,
                                   receiver_parameters,
                                   MediaType::VIDEO)),
      audio_receiver_track_id_by_ssrc_(GetReceiverTrackIds(receivers,
                                                           receiver_parameters,
                                                           MediaType::AUDIO)),
      video_receiver_track_id_by_ssrc_(GetReceiverTrackIds(receivers,
                                                           receiver_parameters,
                                                           MediaType::VIDEO)),
      voice_info_by_sender_ssrc_(GetVoiceSenderInfos(voice_media_info_)),
      video_info_by_sender_ssrc_(GetVideoSenderInfos(video_media_info_)) {
  RTC_DCHECK_EQ(receivers.size(), receiver_parameters.size());
}

const VoiceSenderInfo* TrackMediaInfoMap::GetVoiceSenderInfoBySsrc(
    uint32_t ssrc) const {
  return FindValueOrNull(voice_info_by_sender_ssrc_, ssrc);
}

const VideoSenderInfo* TrackMediaInfoMap::GetVideoSenderInfoBySsrc(
    uint32_t ssrc) const {
  return FindValueOrNull(video_info_by_sender_ssrc_, ssrc);
}

std::optional<int> TrackMediaInfoMap::GetAttachmentIdBySsrc(
    uint32_t ssrc,
    MediaType media_type,
    bool is_sender) const {
  if (media_type == MediaType::AUDIO) {
    if (is_sender) {
      auto it = audio_sender_attachment_id_by_ssrc_.find(ssrc);
      if (it != audio_sender_attachment_id_by_ssrc_.end()) {
        return it->second;
      }
    } else {
      auto it = audio_receiver_attachment_id_by_ssrc_.find(ssrc);
      if (it != audio_receiver_attachment_id_by_ssrc_.end()) {
        return it->second;
      }
    }
  } else {
    if (is_sender) {
      auto it = video_sender_attachment_id_by_ssrc_.find(ssrc);
      if (it != video_sender_attachment_id_by_ssrc_.end()) {
        return it->second;
      }
    } else {
      auto it = video_receiver_attachment_id_by_ssrc_.find(ssrc);
      if (it != video_receiver_attachment_id_by_ssrc_.end()) {
        return it->second;
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> TrackMediaInfoMap::GetReceiverTrackIdBySsrc(
    uint32_t ssrc,
    MediaType media_type) const {
  if (media_type == MediaType::AUDIO) {
    auto it = audio_receiver_track_id_by_ssrc_.find(ssrc);
    if (it != audio_receiver_track_id_by_ssrc_.end()) {
      return it->second;
    }
  } else {
    auto it = video_receiver_track_id_by_ssrc_.find(ssrc);
    if (it != video_receiver_track_id_by_ssrc_.end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

}  // namespace webrtc
