/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "modules/audio_processing/include/audio_generator_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

// TODO(bugs.webrtc.org/8882) Stub.
// Add unit tests for both file audio and generated audio.

TEST(FileAudioGeneratorTest, CreationDeletion) {
  const std::string audio_filename =
      test::ResourcePath("voice_engine/audio_tiny48", "wav");
  auto audio_generator = AudioGeneratorFactory::Create(audio_filename);
}

}  // namespace test
}  // namespace webrtc
