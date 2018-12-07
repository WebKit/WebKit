/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/turnserver/read_auth_file.h"
#include "rtc_base/stringencode.h"

namespace webrtc_examples {

std::map<std::string, std::string> ReadAuthFile(std::istream* s) {
  std::map<std::string, std::string> name_to_key;
  for (std::string line; std::getline(*s, line);) {
    const size_t sep = line.find('=');
    if (sep == std::string::npos)
      continue;
    char buf[32];
    size_t len = rtc::hex_decode(buf, sizeof(buf), line.data() + sep + 1,
                                 line.size() - sep - 1);
    if (len > 0) {
      name_to_key.emplace(line.substr(0, sep), std::string(buf, len));
    }
  }
  return name_to_key;
}

}  // namespace webrtc_examples
