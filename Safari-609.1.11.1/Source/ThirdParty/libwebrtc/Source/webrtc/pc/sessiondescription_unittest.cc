/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/sessiondescription.h"
#include "rtc_base/gunit.h"

namespace cricket {

TEST(MediaContentDescriptionTest, ExtmapAllowMixedDefaultValue) {
  VideoContentDescription video_desc;
  EXPECT_EQ(MediaContentDescription::kNo, video_desc.extmap_allow_mixed_enum());
}

TEST(MediaContentDescriptionTest, SetExtmapAllowMixed) {
  VideoContentDescription video_desc;
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kNo);
  EXPECT_EQ(MediaContentDescription::kNo, video_desc.extmap_allow_mixed_enum());
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_desc.extmap_allow_mixed_enum());
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kSession);
  EXPECT_EQ(MediaContentDescription::kSession,
            video_desc.extmap_allow_mixed_enum());

  // Not allowed to downgrade from kSession to kMedia.
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  EXPECT_EQ(MediaContentDescription::kSession,
            video_desc.extmap_allow_mixed_enum());

  // Always okay to set not supported.
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kNo);
  EXPECT_EQ(MediaContentDescription::kNo, video_desc.extmap_allow_mixed_enum());
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_desc.extmap_allow_mixed_enum());
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kNo);
  EXPECT_EQ(MediaContentDescription::kNo, video_desc.extmap_allow_mixed_enum());
}

TEST(MediaContentDescriptionTest, MixedOneTwoByteHeaderSupported) {
  VideoContentDescription video_desc;
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kNo);
  EXPECT_FALSE(video_desc.extmap_allow_mixed());
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  EXPECT_TRUE(video_desc.extmap_allow_mixed());
  video_desc.set_extmap_allow_mixed_enum(MediaContentDescription::kSession);
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
  MediaContentDescription* video_desc = new VideoContentDescription();
  session_desc.AddContent("video", MediaProtocolType::kRtp, video_desc);

  // Setting true on session level propagates to media level.
  session_desc.set_extmap_allow_mixed(true);
  EXPECT_EQ(MediaContentDescription::kSession,
            video_desc->extmap_allow_mixed_enum());

  // Don't downgrade from session level to media level
  video_desc->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  EXPECT_EQ(MediaContentDescription::kSession,
            video_desc->extmap_allow_mixed_enum());

  // Setting false on session level propagates to media level if the current
  // state is kSession.
  session_desc.set_extmap_allow_mixed(false);
  EXPECT_EQ(MediaContentDescription::kNo,
            video_desc->extmap_allow_mixed_enum());

  // Now possible to set at media level.
  video_desc->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_desc->extmap_allow_mixed_enum());

  // Setting false on session level does not override on media level if current
  // state is kMedia.
  session_desc.set_extmap_allow_mixed(false);
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_desc->extmap_allow_mixed_enum());

  // Setting true on session level overrides setting on media level.
  session_desc.set_extmap_allow_mixed(true);
  EXPECT_EQ(MediaContentDescription::kSession,
            video_desc->extmap_allow_mixed_enum());
}

TEST(SessionDescriptionTest, AddContentTransfersExtmapAllowMixedSetting) {
  SessionDescription session_desc;
  session_desc.set_extmap_allow_mixed(false);
  MediaContentDescription* audio_desc = new AudioContentDescription();
  audio_desc->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);

  // If session setting is false, media level setting is preserved when new
  // content is added.
  session_desc.AddContent("audio", MediaProtocolType::kRtp, audio_desc);
  EXPECT_EQ(MediaContentDescription::kMedia,
            audio_desc->extmap_allow_mixed_enum());

  // If session setting is true, it's transferred to media level when new
  // content is added.
  session_desc.set_extmap_allow_mixed(true);
  MediaContentDescription* video_desc = new VideoContentDescription();
  session_desc.AddContent("video", MediaProtocolType::kRtp, video_desc);
  EXPECT_EQ(MediaContentDescription::kSession,
            video_desc->extmap_allow_mixed_enum());

  // Session level setting overrides media level when new content is added.
  MediaContentDescription* data_desc = new DataContentDescription;
  data_desc->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  session_desc.AddContent("data", MediaProtocolType::kRtp, data_desc);
  EXPECT_EQ(MediaContentDescription::kSession,
            data_desc->extmap_allow_mixed_enum());
}

}  // namespace cricket
