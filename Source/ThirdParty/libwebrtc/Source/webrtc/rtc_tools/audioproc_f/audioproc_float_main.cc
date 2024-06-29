/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/audio/audio_processing.h"
#include "api/test/audioproc_float.h"

int main(int argc, char* argv[]) {
  return webrtc::test::AudioprocFloat(
      std::make_unique<webrtc::AudioProcessingBuilder>(), argc, argv);
}
