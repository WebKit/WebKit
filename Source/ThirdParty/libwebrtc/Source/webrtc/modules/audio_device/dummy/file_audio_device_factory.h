/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_FACTORY_H
#define WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_FACTORY_H

#include "webrtc/common_types.h"

namespace webrtc {

class FileAudioDevice;

// This class is used by audio_device_impl.cc when WebRTC is compiled with
// WEBRTC_DUMMY_FILE_DEVICES. The application must include this file and set the
// filenames to use before the audio device module is initialized. This is
// intended for test tools which use the audio device module.
class FileAudioDeviceFactory {
 public:
  static FileAudioDevice* CreateFileAudioDevice(const int32_t id);

  // The input file must be a readable 48k stereo raw file. The output
  // file must be writable. The strings will be copied.
  static void SetFilenamesToUse(const char* inputAudioFilename,
                                const char* outputAudioFilename);

 private:
  static const uint32_t MAX_FILENAME_LEN = 512;
  static bool _isConfigured;
  static char _inputAudioFilename[MAX_FILENAME_LEN];
  static char _outputAudioFilename[MAX_FILENAME_LEN];
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_FACTORY_H
