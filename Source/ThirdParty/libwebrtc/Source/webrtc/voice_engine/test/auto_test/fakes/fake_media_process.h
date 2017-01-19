/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VOICE_ENGINE_MAIN_TEST_AUTO_TEST_FAKE_MEDIA_PROCESS_H_
#define VOICE_ENGINE_MAIN_TEST_AUTO_TEST_FAKE_MEDIA_PROCESS_H_

#include <math.h>

class FakeMediaProcess : public webrtc::VoEMediaProcess {
 public:
  FakeMediaProcess() : frequency(0) {}
  virtual void Process(int channel,
                       const webrtc::ProcessingTypes type,
                       int16_t audio_10ms[],
                       size_t length,
                       int sampling_freq_hz,
                       bool stereo) {
    for (size_t i = 0; i < length; i++) {
      if (!stereo) {
        audio_10ms[i] = static_cast<int16_t>(audio_10ms[i] *
            sin(2.0 * 3.14 * frequency * 400.0 / sampling_freq_hz));
      } else {
        // Interleaved stereo.
        audio_10ms[2 * i] = static_cast<int16_t> (
            audio_10ms[2 * i] * sin(2.0 * 3.14 *
                frequency * 400.0 / sampling_freq_hz));
        audio_10ms[2 * i + 1] = static_cast<int16_t> (
            audio_10ms[2 * i + 1] * sin(2.0 * 3.14 *
                frequency * 400.0 / sampling_freq_hz));
      }
      frequency++;
    }
  }

 private:
  int frequency;
};

#endif  // VOICE_ENGINE_MAIN_TEST_AUTO_TEST_FAKE_MEDIA_PROCESS_H_
