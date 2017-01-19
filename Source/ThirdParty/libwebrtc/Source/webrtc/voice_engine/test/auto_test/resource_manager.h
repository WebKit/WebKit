/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_RESOURCE_MANAGER_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_RESOURCE_MANAGER_H_

#include <string>

class ResourceManager {
 public:
  ResourceManager();

  // Returns the full path to a long audio file.
  // Returns the empty string on failure.
  const std::string& long_audio_file_path() const {
    return long_audio_file_path_;
  }

 private:
  std::string long_audio_file_path_;
};

#endif // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_RESOURCE_MANAGER_H_
