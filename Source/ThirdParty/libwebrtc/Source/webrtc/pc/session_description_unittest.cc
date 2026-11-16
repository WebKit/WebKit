/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/session_description.h"

#include <memory>
#include <utility>

#include "test/gtest.h"

namespace webrtc {

TEST(MediaContentDescriptionTest, ExtmapAllowMixedDefaultValue) {
  VideoContentDescription video_desc;
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kMedia,
            video_desc.extmap_allow_mixed_level());
}

TEST(MediaContentDescriptionTest, SetExtmapAllowMixed) {
  VideoContentDescription video_desc;
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kNone);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kNone,
            video_desc.extmap_allow_mixed_level());
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kMedia,
            video_desc.extmap_allow_mixed_level());
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kSession);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kSession,
            video_desc.extmap_allow_mixed_level());

  // Not allowed to downgrade from kSession to kMedia.
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kSession,
            video_desc.extmap_allow_mixed_level());

  // Always okay to set not supported.
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kNone);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kNone,
            video_desc.extmap_allow_mixed_level());
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kMedia,
            video_desc.extmap_allow_mixed_level());
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kNone);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kNone,
            video_desc.extmap_allow_mixed_level());
}

TEST(MediaContentDescriptionTest, MixedOneTwoByteHeaderSupported) {
  VideoContentDescription video_desc;
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kNone);
  EXPECT_FALSE(video_desc.extmap_allow_mixed());
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);
  EXPECT_TRUE(video_desc.extmap_allow_mixed());
  video_desc.set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kSession);
  EXPECT_TRUE(video_desc.extmap_allow_mixed());
}

TEST(SessionDescriptionTest, SetExtmapAllowMixed) {
  SessionDescription session_desc;
  session_desc.set_extmap_allow_mixed(true);
  EXPECT_TRUE(session_desc.extmap_allow_mixed());
  session_desc.set_extmap_allow_mixed(false);
  EXPECT_FALSE(session_desc.extmap_allow_mixed());
}

TEST(SessionDescriptionTest, SetExtmapAllowMixedPropagatesToMediaLevel) {
  SessionDescription session_desc;
  session_desc.AddContent("video", MediaProtocolType::kRtp,
                          std::make_unique<VideoContentDescription>());
  MediaContentDescription* video_desc =
      session_desc.GetContentDescriptionByName("video");

  // Setting true on session level propagates to media level.
  session_desc.set_extmap_allow_mixed(true);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kSession,
            video_desc->extmap_allow_mixed_level());

  // Don't downgrade from session level to media level
  video_desc->set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kSession,
            video_desc->extmap_allow_mixed_level());

  // Setting false on session level propagates to media level if the current
  // state is kSession.
  session_desc.set_extmap_allow_mixed(false);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kNone,
            video_desc->extmap_allow_mixed_level());

  // Now possible to set at media level.
  video_desc->set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kMedia,
            video_desc->extmap_allow_mixed_level());

  // Setting false on session level does not override on media level if current
  // state is kMedia.
  session_desc.set_extmap_allow_mixed(false);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kMedia,
            video_desc->extmap_allow_mixed_level());

  // Setting true on session level overrides setting on media level.
  session_desc.set_extmap_allow_mixed(true);
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kSession,
            video_desc->extmap_allow_mixed_level());
}

TEST(SessionDescriptionTest, AddContentTransfersExtmapAllowMixedSetting) {
  SessionDescription session_desc;
  session_desc.set_extmap_allow_mixed(false);
  std::unique_ptr<MediaContentDescription> audio_desc =
      std::make_unique<AudioContentDescription>();
  audio_desc->set_extmap_allow_mixed_level(
      MediaContentDescription::AttributeLevel::kMedia);

  // If session setting is false, media level setting is preserved when new
  // content is added.
  session_desc.AddContent("audio", MediaProtocolType::kRtp,
                          std::move(audio_desc));
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kMedia,
            session_desc.GetContentDescriptionByName("audio")
                ->extmap_allow_mixed_level());

  // If session setting is true, it's transferred to media level when new
  // content is added.
  session_desc.set_extmap_allow_mixed(true);
  std::unique_ptr<MediaContentDescription> video_desc =
      std::make_unique<VideoContentDescription>();
  session_desc.AddContent("video", MediaProtocolType::kRtp,
                          std::move(video_desc));
  EXPECT_EQ(MediaContentDescription::AttributeLevel::kSession,
            session_desc.GetContentDescriptionByName("video")
                ->extmap_allow_mixed_level());
}

}  // namespace webrtc
