/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/config.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

const std::string& Config::audiotracks_path() const {
  return audiotracks_path_;
}

const std::string& Config::timing_filepath() const {
  return timing_filepath_;
}

const std::string& Config::output_path() const {
  return output_path_;
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
