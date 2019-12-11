/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_WAV_BASED_SIMULATOR_H_
#define MODULES_AUDIO_PROCESSING_TEST_WAV_BASED_SIMULATOR_H_

#include <vector>

#include "modules/audio_processing/test/audio_processing_simulator.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {
namespace test {

// Used to perform an audio processing simulation from wav files.
class WavBasedSimulator final : public AudioProcessingSimulator {
 public:
  WavBasedSimulator(const SimulationSettings& settings,
                    std::unique_ptr<AudioProcessingBuilder> ap_builder);
  ~WavBasedSimulator() override;

  // Processes the WAV input.
  void Process() override;

 private:
  enum SimulationEventType {
    kProcessStream,
    kProcessReverseStream,
  };

  void Initialize();
  bool HandleProcessStreamCall();
  bool HandleProcessReverseStreamCall();
  void PrepareProcessStreamCall();
  void PrepareReverseProcessStreamCall();
  static std::vector<SimulationEventType> GetDefaultEventChain();
  static std::vector<SimulationEventType> GetCustomEventChain(
      const std::string& filename);

  std::vector<SimulationEventType> call_chain_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WavBasedSimulator);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_WAV_BASED_SIMULATOR_H_
