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
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_video_track.h"
#include "media/base/media_channel.h"
#include "pc/audio_track.h"
#include "pc/test/fake_video_track_source.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "pc/video_track.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;

namespace webrtc {

namespace {

RtpParameters CreateRtpParametersWithSsrcs(
    std::initializer_list<uint32_t> ssrcs) {
  RtpParameters params;
  for (uint32_t ssrc : ssrcs) {
    RtpEncodingParameters encoding_params;
    encoding_params.ssrc = ssrc;
    params.encodings.push_back(encoding_params);
  }
  return params;
}

scoped_refptr<MockRtpSenderInternal> CreateMockRtpSender(
    MediaType media_type,
    std::initializer_list<uint32_t> ssrcs,
    scoped_refptr<MediaStreamTrackInterface> track) {
  uint32_t first_ssrc;
  if (ssrcs.size()) {
    first_ssrc = *ssrcs.begin();
  } else {
    first_ssrc = 0;
  }
  auto sender = make_ref_counted<MockRtpSenderInternal>();
  EXPECT_CALL(*sender, track())
      .WillRepeatedly(::testing::Return(std::move(track)));
  EXPECT_CALL(*sender, ssrc()).WillRepeatedly(::testing::Return(first_ssrc));
  EXPECT_CALL(*sender, media_type())
      .WillRepeatedly(::testing::Return(media_type));
  EXPECT_CALL(*sender, GetParameters())
      .WillRepeatedly(::testing::Return(CreateRtpParametersWithSsrcs(ssrcs)));
  EXPECT_CALL(*sender, AttachmentId()).WillRepeatedly(::testing::Return(1));
  return sender;
}

scoped_refptr<MockRtpReceiverInternal> CreateMockRtpReceiver(
    MediaType media_type,
    std::initializer_list<uint32_t> ssrcs,
    scoped_refptr<MediaStreamTrackInterface> track) {
  auto receiver = make_ref_counted<MockRtpReceiverInternal>();
  EXPECT_CALL(*receiver, track())
      .WillRepeatedly(::testing::Return(std::move(track)));
  EXPECT_CALL(*receiver, media_type())
      .WillRepeatedly(::testing::Return(media_type));
  EXPECT_CALL(*receiver, GetParameters())
      .WillRepeatedly(::testing::Return(CreateRtpParametersWithSsrcs(ssrcs)));
  EXPECT_CALL(*receiver, AttachmentId()).WillRepeatedly(::testing::Return(1));
  return receiver;
}

scoped_refptr<VideoTrackInterface> CreateVideoTrack(const std::string& id) {
  return VideoTrack::Create(id, FakeVideoTrackSource::Create(false),
                            Thread::Current());
}

scoped_refptr<VideoTrackInterface> CreateMockVideoTrack(const std::string& id) {
  auto track = MockVideoTrack::Create();
  EXPECT_CALL(*track, kind())
      .WillRepeatedly(::testing::Return(VideoTrack::kVideoKind));
  return track;
}

class TrackMediaInfoMapTest : public ::testing::Test {
 public:
  TrackMediaInfoMapTest() : TrackMediaInfoMapTest(true) {}

  explicit TrackMediaInfoMapTest(bool use_real_video_track)
      : local_audio_track_(AudioTrack::Create("LocalAudioTrack", nullptr)),
        remote_audio_track_(AudioTrack::Create("RemoteAudioTrack", nullptr)),
        local_video_track_(use_real_video_track
                               ? CreateVideoTrack("LocalVideoTrack")
                               : CreateMockVideoTrack("LocalVideoTrack")),
        remote_video_track_(use_real_video_track
                                ? CreateVideoTrack("RemoteVideoTrack")
                                : CreateMockVideoTrack("LocalVideoTrack")) {}

  void AddRtpSenderWithSsrcs(std::initializer_list<uint32_t> ssrcs,
                             MediaStreamTrackInterface* local_track) {
    scoped_refptr<MockRtpSenderInternal> rtp_sender = CreateMockRtpSender(
        local_track->kind() == MediaStreamTrackInterface::kAudioKind
            ? MediaType::AUDIO
            : MediaType::VIDEO,
        ssrcs, scoped_refptr<MediaStreamTrackInterface>(local_track));
    rtp_senders_.push_back(rtp_sender);

    if (local_track->kind() == MediaStreamTrackInterface::kAudioKind) {
      VoiceSenderInfo voice_sender_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        voice_sender_info.local_stats.push_back(SsrcSenderInfo());
        voice_sender_info.local_stats[i++].ssrc = ssrc;
      }
      voice_media_info_.senders.push_back(voice_sender_info);
    } else {
      VideoSenderInfo video_sender_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        video_sender_info.local_stats.push_back(SsrcSenderInfo());
        video_sender_info.local_stats[i++].ssrc = ssrc;
      }
      video_media_info_.senders.push_back(video_sender_info);
      video_media_info_.aggregated_senders.push_back(video_sender_info);
    }
  }

  void AddRtpReceiverWithSsrcs(std::initializer_list<uint32_t> ssrcs,
                               MediaStreamTrackInterface* remote_track) {
    auto rtp_receiver = CreateMockRtpReceiver(
        remote_track->kind() == MediaStreamTrackInterface::kAudioKind
            ? MediaType::AUDIO
            : MediaType::VIDEO,
        ssrcs, scoped_refptr<MediaStreamTrackInterface>(remote_track));
    rtp_receivers_.push_back(rtp_receiver);

    if (remote_track->kind() == MediaStreamTrackInterface::kAudioKind) {
      VoiceReceiverInfo voice_receiver_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        voice_receiver_info.local_stats.push_back(SsrcReceiverInfo());
        voice_receiver_info.local_stats[i++].ssrc = ssrc;
      }
      voice_media_info_.receivers.push_back(voice_receiver_info);
    } else {
      VideoReceiverInfo video_receiver_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        video_receiver_info.local_stats.push_back(SsrcReceiverInfo());
        video_receiver_info.local_stats[i++].ssrc = ssrc;
      }
      video_media_info_.receivers.push_back(video_receiver_info);
    }
  }

  // Copies the current state of `voice_media_info_` and `video_media_info_`
  // into the map.
  TrackMediaInfoMap InitializeMap() {
    std::vector<TrackMediaInfoMap::RtpSenderSignalInfo> sender_infos;
    for (const auto& sender : rtp_senders_) {
      sender_infos.push_back({
          .ssrc = sender->ssrc(),
          .attachment_id = sender->AttachmentId(),
          .media_type = sender->media_type(),
      });
    }
    std::vector<TrackMediaInfoMap::RtpReceiverSignalInfo> receiver_infos;
    std::vector<RtpParameters> receiver_params;
    for (const auto& receiver : rtp_receivers_) {
      receiver_infos.push_back({
          .track_id = receiver->track() ? receiver->track()->id() : "",
          .attachment_id = receiver->AttachmentId(),
          .media_type = receiver->media_type(),
      });
      receiver_params.push_back(receiver->GetParameters());
    }
    std::optional<VoiceMediaInfo> voice_media_info;
    if (!voice_media_info_.senders.empty() ||
        !voice_media_info_.receivers.empty()) {
      voice_media_info = voice_media_info_;
    }
    std::optional<VideoMediaInfo> video_media_info;
    if (!video_media_info_.aggregated_senders.empty() ||
        !video_media_info_.receivers.empty()) {
      video_media_info = video_media_info_;
    }
    return TrackMediaInfoMap(std::move(voice_media_info),
                             std::move(video_media_info), sender_infos,
                             receiver_infos, receiver_params);
  }

 private:
  AutoThread main_thread_;
  VoiceMediaInfo voice_media_info_;
  VideoMediaInfo video_media_info_;

 protected:
  std::vector<scoped_refptr<RtpSenderInternal>> rtp_senders_;
  std::vector<scoped_refptr<RtpReceiverInternal>> rtp_receivers_;
  scoped_refptr<AudioTrack> local_audio_track_;
  scoped_refptr<AudioTrack> remote_audio_track_;
  scoped_refptr<VideoTrackInterface> local_video_track_;
  scoped_refptr<VideoTrackInterface> remote_video_track_;
};

}  // namespace

TEST_F(TrackMediaInfoMapTest, SingleSenderReceiverPerTrackWithOneSsrc) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_.get());
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_.get());
  AddRtpSenderWithSsrcs({3}, local_video_track_.get());
  AddRtpReceiverWithSsrcs({4}, remote_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  // RTP audio sender -> attachment_id
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[0]->AttachmentId());
  // RTP audio receiver -> track_id
  EXPECT_EQ(map.GetReceiverTrackIdBySsrc(2, MediaType::AUDIO),
            remote_audio_track_->id());
  // RTP video sender -> local video track
  EXPECT_EQ(map.GetAttachmentIdBySsrc(3, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[1]->AttachmentId());
  // RTP video receiver -> remote video track
  EXPECT_EQ(map.GetReceiverTrackIdBySsrc(4, MediaType::VIDEO),
            remote_video_track_->id());
}

TEST_F(TrackMediaInfoMapTest,
       SingleSenderReceiverPerTrackWithAudioAndVideoUseSameSsrc) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_.get());
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_.get());
  AddRtpSenderWithSsrcs({1}, local_video_track_.get());
  AddRtpReceiverWithSsrcs({2}, remote_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  // RTP audio sender -> attachment_id
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[0]->AttachmentId());
  // RTP audio receiver -> track_id
  EXPECT_EQ(map.GetReceiverTrackIdBySsrc(2, MediaType::AUDIO),
            remote_audio_track_->id());
  // RTP video sender -> local video track
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[1]->AttachmentId());
  // RTP video receiver -> remote video track
  EXPECT_EQ(map.GetReceiverTrackIdBySsrc(2, MediaType::VIDEO),
            remote_video_track_->id());
}

TEST_F(TrackMediaInfoMapTest, SingleMultiSsrcSenderPerTrack) {
  AddRtpSenderWithSsrcs({1, 2}, local_audio_track_.get());
  AddRtpSenderWithSsrcs({3, 4}, local_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  // RTP audio senders -> attachment_id
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[0]->AttachmentId());
  // RTP video senders -> local video track
  EXPECT_EQ(map.GetAttachmentIdBySsrc(3, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[1]->AttachmentId());
}

TEST_F(TrackMediaInfoMapTest, MultipleOneSsrcSendersPerTrack) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_.get());
  AddRtpSenderWithSsrcs({2}, local_audio_track_.get());
  AddRtpSenderWithSsrcs({3}, local_video_track_.get());
  AddRtpSenderWithSsrcs({4}, local_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  // RTP audio senders -> attachment_ids
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[0]->AttachmentId());
  EXPECT_EQ(map.GetAttachmentIdBySsrc(2, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[1]->AttachmentId());
  // RTP video senders -> local video track
  EXPECT_EQ(map.GetAttachmentIdBySsrc(3, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[2]->AttachmentId());
  EXPECT_EQ(map.GetAttachmentIdBySsrc(4, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[3]->AttachmentId());
}

TEST_F(TrackMediaInfoMapTest, MultipleMultiSsrcSendersPerTrack) {
  AddRtpSenderWithSsrcs({1, 2}, local_audio_track_.get());
  AddRtpSenderWithSsrcs({3, 4}, local_audio_track_.get());
  AddRtpSenderWithSsrcs({5, 6}, local_video_track_.get());
  AddRtpSenderWithSsrcs({7, 8}, local_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  // RTP audio senders -> attachment_ids (Primary SSRCs only)
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[0]->AttachmentId());
  EXPECT_EQ(map.GetAttachmentIdBySsrc(3, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[1]->AttachmentId());
  // RTP video senders -> local video track
  EXPECT_EQ(map.GetAttachmentIdBySsrc(5, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[2]->AttachmentId());
  EXPECT_EQ(map.GetAttachmentIdBySsrc(7, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[3]->AttachmentId());
}

// SSRCs can be reused for send and receive in loopback.
TEST_F(TrackMediaInfoMapTest, SingleSenderReceiverPerTrackWithSsrcNotUnique) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_.get());
  AddRtpReceiverWithSsrcs({1}, remote_audio_track_.get());
  AddRtpSenderWithSsrcs({2}, local_video_track_.get());
  AddRtpReceiverWithSsrcs({2}, remote_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  // RTP audio senders -> attachment_ids
  EXPECT_EQ(map.GetAttachmentIdBySsrc(1, MediaType::AUDIO, /*is_sender=*/true),
            rtp_senders_[0]->AttachmentId());
  // RTP audio receiver -> track_id
  EXPECT_EQ(map.GetReceiverTrackIdBySsrc(1, MediaType::AUDIO),
            remote_audio_track_->id());
  // RTP video senders -> local video track
  EXPECT_EQ(map.GetAttachmentIdBySsrc(2, MediaType::VIDEO, /*is_sender=*/true),
            rtp_senders_[1]->AttachmentId());
  // RTP video receiver -> remote video track
  EXPECT_EQ(map.GetReceiverTrackIdBySsrc(2, MediaType::VIDEO),
            remote_video_track_->id());
}

TEST_F(TrackMediaInfoMapTest, SsrcLookupFunction) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_.get());
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_.get());
  AddRtpSenderWithSsrcs({3}, local_video_track_.get());
  AddRtpReceiverWithSsrcs({4}, remote_video_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  EXPECT_TRUE(map.GetVoiceSenderInfoBySsrc(1));
  EXPECT_TRUE(map.GetVideoSenderInfoBySsrc(3));
  EXPECT_FALSE(map.GetVoiceSenderInfoBySsrc(2));
  EXPECT_FALSE(map.GetVoiceSenderInfoBySsrc(1024));
}

TEST_F(TrackMediaInfoMapTest, GetAttachmentIdBySsrc) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_.get());
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  EXPECT_EQ(rtp_senders_[0]->AttachmentId(),
            map.GetAttachmentIdBySsrc(1, MediaType::AUDIO,
                                      /*is_sender=*/true));
  EXPECT_EQ(rtp_receivers_[0]->AttachmentId(),
            map.GetAttachmentIdBySsrc(2, MediaType::AUDIO,
                                      /*is_sender=*/false));
  EXPECT_EQ(std::nullopt, map.GetAttachmentIdBySsrc(3, MediaType::AUDIO,
                                                    /*is_sender=*/true));
}

TEST_F(TrackMediaInfoMapTest, GetReceiverTrackIdBySsrc) {
  AddRtpReceiverWithSsrcs({1}, remote_audio_track_.get());
  TrackMediaInfoMap map = InitializeMap();
  EXPECT_EQ(remote_audio_track_->id(),
            map.GetReceiverTrackIdBySsrc(1, MediaType::AUDIO));
  EXPECT_EQ(std::nullopt, map.GetReceiverTrackIdBySsrc(2, MediaType::AUDIO));
}

}  // namespace webrtc
