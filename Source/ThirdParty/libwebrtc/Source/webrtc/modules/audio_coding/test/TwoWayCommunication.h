/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_TEST_TWOWAYCOMMUNICATION_H_
#define MODULES_AUDIO_CODING_TEST_TWOWAYCOMMUNICATION_H_

#include <memory>

#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/Channel.h"
#include "modules/audio_coding/test/PCMFile.h"

namespace webrtc {

class TwoWayCommunication {
 public:
  TwoWayCommunication();
  ~TwoWayCommunication();

  void Perform();

 private:
  void SetUpAutotest(AudioEncoderFactory* const encoder_factory,
                     const SdpAudioFormat& format1,
                     int payload_type1,
                     const SdpAudioFormat& format2,
                     int payload_type2);

  std::unique_ptr<AudioCodingModule> _acmA;
  std::unique_ptr<AudioCodingModule> _acmB;

  std::unique_ptr<AudioCodingModule> _acmRefA;
  std::unique_ptr<AudioCodingModule> _acmRefB;

  Channel* _channel_A2B;
  Channel* _channel_B2A;

  Channel* _channelRef_A2B;
  Channel* _channelRef_B2A;

  PCMFile _inFileA;
  PCMFile _inFileB;

  PCMFile _outFileA;
  PCMFile _outFileB;

  PCMFile _outFileRefA;
  PCMFile _outFileRefB;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_TEST_TWOWAYCOMMUNICATION_H_
