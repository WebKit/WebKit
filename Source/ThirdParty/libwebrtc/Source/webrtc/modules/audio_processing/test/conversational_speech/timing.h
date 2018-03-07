/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_TIMING_H_
#define MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_TIMING_H_

#include <string>
#include <vector>

#include "api/array_view.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

struct Turn{
  Turn(std::string new_speaker_name, std::string new_audiotrack_file_name,
       int new_offset)
      : speaker_name(new_speaker_name),
        audiotrack_file_name(new_audiotrack_file_name),
        offset(new_offset) {}
  bool operator==(const Turn &b) const;
  std::string speaker_name;
  std::string audiotrack_file_name;
  int offset;
};

// Loads a list of turns from a file.
std::vector<Turn> LoadTiming(const std::string& timing_filepath);

// Writes a list of turns into a file.
void SaveTiming(const std::string& timing_filepath,
                rtc::ArrayView<const Turn> timing);

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_TIMING_H_
