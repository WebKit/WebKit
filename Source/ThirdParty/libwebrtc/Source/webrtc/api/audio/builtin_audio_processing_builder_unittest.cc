/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/builtin_audio_processing_builder.h"

#include "api/audio/audio_processing.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/scoped_refptr.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::NotNull;

TEST(BuiltinAudioProcessingBuilderTest, CreatesWithDefaults) {
  EXPECT_THAT(BuiltinAudioProcessingBuilder().Build(CreateEnvironment()),
              NotNull());
}

TEST(BuiltinAudioProcessingBuilderTest, CreatesWithConfig) {
  const Environment env = CreateEnvironment();
  AudioProcessing::Config config;
  // Change a field to make config different to default one.
  config.gain_controller1.enabled = !config.gain_controller1.enabled;

  scoped_refptr<AudioProcessing> ap1 =
      BuiltinAudioProcessingBuilder(config).Build(env);
  ASSERT_THAT(ap1, NotNull());
  EXPECT_EQ(ap1->GetConfig().gain_controller1.enabled,
            config.gain_controller1.enabled);

  scoped_refptr<AudioProcessing> ap2 =
      BuiltinAudioProcessingBuilder().SetConfig(config).Build(env);
  ASSERT_THAT(ap2, NotNull());
  EXPECT_EQ(ap2->GetConfig().gain_controller1.enabled,
            config.gain_controller1.enabled);
}

}  // namespace webrtc
