/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/conversational_speech/timing.h"

#include <fstream>
#include <iostream>

#include "rtc_base/string_encode.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

bool Turn::operator==(const Turn& b) const {
  return b.speaker_name == speaker_name &&
         b.audiotrack_file_name == audiotrack_file_name && b.offset == offset &&
         b.gain == gain;
}

std::vector<Turn> LoadTiming(const std::string& timing_filepath) {
  // Line parser.
  auto parse_line = [](const std::string& line) {
    std::vector<std::string> fields;
    rtc::split(line, ' ', &fields);
    RTC_CHECK_GE(fields.size(), 3);
    RTC_CHECK_LE(fields.size(), 4);
    int gain = 0;
    if (fields.size() == 4) {
      gain = std::atof(fields[3].c_str());
    }
    return Turn(fields[0], fields[1], std::atol(fields[2].c_str()), gain);
  };

  // Init.
  std::vector<Turn> timing;

  // Parse lines.
  std::string line;
  std::ifstream infile(timing_filepath);
  while (std::getline(infile, line)) {
    if (line.empty())
      continue;
    timing.push_back(parse_line(line));
  }
  infile.close();

  return timing;
}

void SaveTiming(const std::string& timing_filepath,
                rtc::ArrayView<const Turn> timing) {
  std::ofstream outfile(timing_filepath);
  RTC_CHECK(outfile.is_open());
  for (const Turn& turn : timing) {
    outfile << turn.speaker_name << " " << turn.audiotrack_file_name << " "
            << turn.offset << " " << turn.gain << std::endl;
  }
  outfile.close();
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
