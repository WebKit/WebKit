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

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/rtp_sender_interface.h"
#include "api/transport/rtp/rtp_source.h"
#include "media/base/media_channel.h"
#include "pc/audio_track.h"
#include "pc/test/fake_video_track_source.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "pc/video_track.h"
#include "rtc_base/ref_count.h"
#include "test/gtest.h"

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

rtc::scoped_refptr<MockRtpSenderInternal> CreateMockRtpSender(
    cricket::MediaType media_type,
    std::initializer_list<uint32_t> ssrcs,
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  uint32_t first_ssrc;
  if (ssrcs.size()) {
    first_ssrc = *ssrcs.begin();
  } else {
    first_ssrc = 0;
  }
  rtc::scoped_refptr<MockRtpSenderInternal> sender(
      new rtc::RefCountedObject<MockRtpSenderInternal>());
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

rtc::scoped_refptr<MockRtpReceiverInternal> CreateMockRtpReceiver(
    cricket::MediaType media_type,
    std::initializer_list<uint32_t> ssrcs,
    rtc::scoped_refptr<MediaStreamTrackInterface> track) {
  rtc::scoped_refptr<MockRtpReceiverInternal> receiver(
      new rtc::RefCountedObject<MockRtpReceiverInternal>());
  EXPECT_CALL(*receiver, track())
      .WillRepeatedly(::testing::Return(std::move(track)));
  EXPECT_CALL(*receiver, media_type())
      .WillRepeatedly(::testing::Return(media_type));
  EXPECT_CALL(*receiver, GetParameters())
      .WillRepeatedly(::testing::Return(CreateRtpParametersWithSsrcs(ssrcs)));
  EXPECT_CALL(*receiver, AttachmentId()).WillRepeatedly(::testing::Return(1));
  return receiver;
}

class TrackMediaInfoMapTest : public ::testing::Test {
 public:
  TrackMediaInfoMapTest() : TrackMediaInfoMapTest(true) {}

  explicit TrackMediaInfoMapTest(bool use_current_thread)
      : voice_media_info_(new cricket::VoiceMediaInfo()),
        video_media_info_(new cricket::VideoMediaInfo()),
        local_audio_track_(AudioTrack::Create("LocalAudioTrack", nullptr)),
        remote_audio_track_(AudioTrack::Create("RemoteAudioTrack", nullptr)),
        local_video_track_(VideoTrack::Create(
            "LocalVideoTrack",
            FakeVideoTrackSource::Create(false),
            use_current_thread ? rtc::Thread::Current() : nullptr)),
        remote_video_track_(VideoTrack::Create(
            "RemoteVideoTrack",
            FakeVideoTrackSource::Create(false),
            use_current_thread ? rtc::Thread::Current() : nullptr)) {}

  ~TrackMediaInfoMapTest() {
    // If we have a map the ownership has been passed to the map, only delete if
    // |CreateMap| has not been called.
    if (!map_) {
      delete voice_media_info_;
      delete video_media_info_;
    }
  }

  void AddRtpSenderWithSsrcs(std::initializer_list<uint32_t> ssrcs,
                             MediaStreamTrackInterface* local_track) {
    rtc::scoped_refptr<MockRtpSenderInternal> rtp_sender = CreateMockRtpSender(
        local_track->kind() == MediaStreamTrackInterface::kAudioKind
            ? cricket::MEDIA_TYPE_AUDIO
            : cricket::MEDIA_TYPE_VIDEO,
        ssrcs, local_track);
    rtp_senders_.push_back(rtp_sender);

    if (local_track->kind() == MediaStreamTrackInterface::kAudioKind) {
      cricket::VoiceSenderInfo voice_sender_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        voice_sender_info.local_stats.push_back(cricket::SsrcSenderInfo());
        voice_sender_info.local_stats[i++].ssrc = ssrc;
      }
      voice_media_info_->senders.push_back(voice_sender_info);
    } else {
      cricket::VideoSenderInfo video_sender_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        video_sender_info.local_stats.push_back(cricket::SsrcSenderInfo());
        video_sender_info.local_stats[i++].ssrc = ssrc;
      }
      video_media_info_->senders.push_back(video_sender_info);
      video_media_info_->aggregated_senders.push_back(video_sender_info);
    }
  }

  void AddRtpReceiverWithSsrcs(std::initializer_list<uint32_t> ssrcs,
                               MediaStreamTrackInterface* remote_track) {
    auto rtp_receiver = CreateMockRtpReceiver(
        remote_track->kind() == MediaStreamTrackInterface::kAudioKind
            ? cricket::MEDIA_TYPE_AUDIO
            : cricket::MEDIA_TYPE_VIDEO,
        ssrcs, remote_track);
    rtp_receivers_.push_back(rtp_receiver);

    if (remote_track->kind() == MediaStreamTrackInterface::kAudioKind) {
      cricket::VoiceReceiverInfo voice_receiver_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        voice_receiver_info.local_stats.push_back(cricket::SsrcReceiverInfo());
        voice_receiver_info.local_stats[i++].ssrc = ssrc;
      }
      voice_media_info_->receivers.push_back(voice_receiver_info);
    } else {
      cricket::VideoReceiverInfo video_receiver_info;
      size_t i = 0;
      for (uint32_t ssrc : ssrcs) {
        video_receiver_info.local_stats.push_back(cricket::SsrcReceiverInfo());
        video_receiver_info.local_stats[i++].ssrc = ssrc;
      }
      video_media_info_->receivers.push_back(video_receiver_info);
    }
  }

  void CreateMap() {
    RTC_DCHECK(!map_);
    map_.reset(new TrackMediaInfoMap(
        std::unique_ptr<cricket::VoiceMediaInfo>(voice_media_info_),
        std::unique_ptr<cricket::VideoMediaInfo>(video_media_info_),
        rtp_senders_, rtp_receivers_));
  }

 protected:
  cricket::VoiceMediaInfo* voice_media_info_;
  cricket::VideoMediaInfo* video_media_info_;
  std::vector<rtc::scoped_refptr<RtpSenderInternal>> rtp_senders_;
  std::vector<rtc::scoped_refptr<RtpReceiverInternal>> rtp_receivers_;
  std::unique_ptr<TrackMediaInfoMap> map_;
  rtc::scoped_refptr<AudioTrack> local_audio_track_;
  rtc::scoped_refptr<AudioTrack> remote_audio_track_;
  rtc::scoped_refptr<VideoTrack> local_video_track_;
  rtc::scoped_refptr<VideoTrack> remote_video_track_;
};

}  // namespace

TEST_F(TrackMediaInfoMapTest, SingleSenderReceiverPerTrackWithOneSsrc) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_);
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_);
  AddRtpSenderWithSsrcs({3}, local_video_track_);
  AddRtpReceiverWithSsrcs({4}, remote_video_track_);
  CreateMap();

  // Local audio track <-> RTP audio sender
  ASSERT_TRUE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_EQ(
      *map_->GetVoiceSenderInfos(*local_audio_track_),
      std::vector<cricket::VoiceSenderInfo*>({&voice_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[0]),
            local_audio_track_.get());

  // Remote audio track <-> RTP audio receiver
  EXPECT_EQ(map_->GetVoiceReceiverInfo(*remote_audio_track_),
            &voice_media_info_->receivers[0]);
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->receivers[0]),
            remote_audio_track_.get());

  // Local video track <-> RTP video sender
  ASSERT_TRUE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_EQ(
      *map_->GetVideoSenderInfos(*local_video_track_),
      std::vector<cricket::VideoSenderInfo*>({&video_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[0]),
            local_video_track_.get());

  // Remote video track <-> RTP video receiver
  EXPECT_EQ(map_->GetVideoReceiverInfo(*remote_video_track_),
            &video_media_info_->receivers[0]);
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->receivers[0]),
            remote_video_track_.get());
}

TEST_F(TrackMediaInfoMapTest, SingleSenderReceiverPerTrackWithMissingSsrc) {
  AddRtpSenderWithSsrcs({}, local_audio_track_);
  AddRtpSenderWithSsrcs({}, local_video_track_);
  AddRtpReceiverWithSsrcs({}, remote_audio_track_);
  AddRtpReceiverWithSsrcs({}, remote_video_track_);
  CreateMap();

  EXPECT_FALSE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_FALSE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_FALSE(map_->GetVoiceReceiverInfo(*remote_audio_track_));
  EXPECT_FALSE(map_->GetVideoReceiverInfo(*remote_video_track_));
}

TEST_F(TrackMediaInfoMapTest,
       SingleSenderReceiverPerTrackWithAudioAndVideoUseSameSsrc) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_);
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_);
  AddRtpSenderWithSsrcs({1}, local_video_track_);
  AddRtpReceiverWithSsrcs({2}, remote_video_track_);
  CreateMap();

  // Local audio track <-> RTP audio sender
  ASSERT_TRUE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_EQ(
      *map_->GetVoiceSenderInfos(*local_audio_track_),
      std::vector<cricket::VoiceSenderInfo*>({&voice_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[0]),
            local_audio_track_.get());

  // Remote audio track <-> RTP audio receiver
  EXPECT_EQ(map_->GetVoiceReceiverInfo(*remote_audio_track_),
            &voice_media_info_->receivers[0]);
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->receivers[0]),
            remote_audio_track_.get());

  // Local video track <-> RTP video sender
  ASSERT_TRUE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_EQ(
      *map_->GetVideoSenderInfos(*local_video_track_),
      std::vector<cricket::VideoSenderInfo*>({&video_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[0]),
            local_video_track_.get());

  // Remote video track <-> RTP video receiver
  EXPECT_EQ(map_->GetVideoReceiverInfo(*remote_video_track_),
            &video_media_info_->receivers[0]);
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->receivers[0]),
            remote_video_track_.get());
}

TEST_F(TrackMediaInfoMapTest, SingleMultiSsrcSenderPerTrack) {
  AddRtpSenderWithSsrcs({1, 2}, local_audio_track_);
  AddRtpSenderWithSsrcs({3, 4}, local_video_track_);
  CreateMap();

  // Local audio track <-> RTP audio senders
  ASSERT_TRUE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_EQ(
      *map_->GetVoiceSenderInfos(*local_audio_track_),
      std::vector<cricket::VoiceSenderInfo*>({&voice_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[0]),
            local_audio_track_.get());

  // Local video track <-> RTP video senders
  ASSERT_TRUE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_EQ(
      *map_->GetVideoSenderInfos(*local_video_track_),
      std::vector<cricket::VideoSenderInfo*>({&video_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[0]),
            local_video_track_.get());
}

TEST_F(TrackMediaInfoMapTest, MultipleOneSsrcSendersPerTrack) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_);
  AddRtpSenderWithSsrcs({2}, local_audio_track_);
  AddRtpSenderWithSsrcs({3}, local_video_track_);
  AddRtpSenderWithSsrcs({4}, local_video_track_);
  CreateMap();

  // Local audio track <-> RTP audio senders
  ASSERT_TRUE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_EQ(
      *map_->GetVoiceSenderInfos(*local_audio_track_),
      std::vector<cricket::VoiceSenderInfo*>(
          {&voice_media_info_->senders[0], &voice_media_info_->senders[1]}));
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[0]),
            local_audio_track_.get());
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[1]),
            local_audio_track_.get());

  // Local video track <-> RTP video senders
  ASSERT_TRUE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_EQ(
      *map_->GetVideoSenderInfos(*local_video_track_),
      std::vector<cricket::VideoSenderInfo*>(
          {&video_media_info_->senders[0], &video_media_info_->senders[1]}));
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[0]),
            local_video_track_.get());
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[1]),
            local_video_track_.get());
}

TEST_F(TrackMediaInfoMapTest, MultipleMultiSsrcSendersPerTrack) {
  AddRtpSenderWithSsrcs({1, 2}, local_audio_track_);
  AddRtpSenderWithSsrcs({3, 4}, local_audio_track_);
  AddRtpSenderWithSsrcs({5, 6}, local_video_track_);
  AddRtpSenderWithSsrcs({7, 8}, local_video_track_);
  CreateMap();

  // Local audio track <-> RTP audio senders
  ASSERT_TRUE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_EQ(
      *map_->GetVoiceSenderInfos(*local_audio_track_),
      std::vector<cricket::VoiceSenderInfo*>(
          {&voice_media_info_->senders[0], &voice_media_info_->senders[1]}));
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[0]),
            local_audio_track_.get());
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[1]),
            local_audio_track_.get());

  // Local video track <-> RTP video senders
  ASSERT_TRUE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_EQ(
      *map_->GetVideoSenderInfos(*local_video_track_),
      std::vector<cricket::VideoSenderInfo*>(
          {&video_media_info_->senders[0], &video_media_info_->senders[1]}));
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[0]),
            local_video_track_.get());
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[1]),
            local_video_track_.get());
}

// SSRCs can be reused for send and receive in loopback.
TEST_F(TrackMediaInfoMapTest, SingleSenderReceiverPerTrackWithSsrcNotUnique) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_);
  AddRtpReceiverWithSsrcs({1}, remote_audio_track_);
  AddRtpSenderWithSsrcs({2}, local_video_track_);
  AddRtpReceiverWithSsrcs({2}, remote_video_track_);
  CreateMap();

  // Local audio track <-> RTP audio senders
  ASSERT_TRUE(map_->GetVoiceSenderInfos(*local_audio_track_));
  EXPECT_EQ(
      *map_->GetVoiceSenderInfos(*local_audio_track_),
      std::vector<cricket::VoiceSenderInfo*>({&voice_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->senders[0]),
            local_audio_track_.get());

  // Remote audio track <-> RTP audio receiver
  EXPECT_EQ(map_->GetVoiceReceiverInfo(*remote_audio_track_),
            &voice_media_info_->receivers[0]);
  EXPECT_EQ(map_->GetAudioTrack(voice_media_info_->receivers[0]),
            remote_audio_track_.get());

  // Local video track <-> RTP video senders
  ASSERT_TRUE(map_->GetVideoSenderInfos(*local_video_track_));
  EXPECT_EQ(
      *map_->GetVideoSenderInfos(*local_video_track_),
      std::vector<cricket::VideoSenderInfo*>({&video_media_info_->senders[0]}));
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->senders[0]),
            local_video_track_.get());

  // Remote video track <-> RTP video receiver
  EXPECT_EQ(map_->GetVideoReceiverInfo(*remote_video_track_),
            &video_media_info_->receivers[0]);
  EXPECT_EQ(map_->GetVideoTrack(video_media_info_->receivers[0]),
            remote_video_track_.get());
}

TEST_F(TrackMediaInfoMapTest, SsrcLookupFunction) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_);
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_);
  AddRtpSenderWithSsrcs({3}, local_video_track_);
  AddRtpReceiverWithSsrcs({4}, remote_video_track_);
  CreateMap();
  EXPECT_TRUE(map_->GetVoiceSenderInfoBySsrc(1));
  EXPECT_TRUE(map_->GetVoiceReceiverInfoBySsrc(2));
  EXPECT_TRUE(map_->GetVideoSenderInfoBySsrc(3));
  EXPECT_TRUE(map_->GetVideoReceiverInfoBySsrc(4));
  EXPECT_FALSE(map_->GetVoiceSenderInfoBySsrc(2));
  EXPECT_FALSE(map_->GetVoiceSenderInfoBySsrc(1024));
}

TEST_F(TrackMediaInfoMapTest, GetAttachmentIdByTrack) {
  AddRtpSenderWithSsrcs({1}, local_audio_track_);
  CreateMap();
  EXPECT_EQ(rtp_senders_[0]->AttachmentId(),
            map_->GetAttachmentIdByTrack(local_audio_track_));
  EXPECT_EQ(absl::nullopt, map_->GetAttachmentIdByTrack(local_video_track_));
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

class TrackMediaInfoMapDeathTest : public TrackMediaInfoMapTest {
 public:
  TrackMediaInfoMapDeathTest() : TrackMediaInfoMapTest(false) {}
};

TEST_F(TrackMediaInfoMapDeathTest, MultipleOneSsrcReceiversPerTrack) {
  AddRtpReceiverWithSsrcs({1}, remote_audio_track_);
  AddRtpReceiverWithSsrcs({2}, remote_audio_track_);
  AddRtpReceiverWithSsrcs({3}, remote_video_track_);
  AddRtpReceiverWithSsrcs({4}, remote_video_track_);
  EXPECT_DEATH(CreateMap(), "");
}

TEST_F(TrackMediaInfoMapDeathTest, MultipleMultiSsrcReceiversPerTrack) {
  AddRtpReceiverWithSsrcs({1, 2}, remote_audio_track_);
  AddRtpReceiverWithSsrcs({3, 4}, remote_audio_track_);
  AddRtpReceiverWithSsrcs({5, 6}, remote_video_track_);
  AddRtpReceiverWithSsrcs({7, 8}, remote_video_track_);
  EXPECT_DEATH(CreateMap(), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
