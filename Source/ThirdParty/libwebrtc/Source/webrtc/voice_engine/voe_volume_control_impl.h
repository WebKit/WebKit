/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_IMPL_H

#include "webrtc/voice_engine/include/voe_volume_control.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEVolumeControlImpl : public VoEVolumeControl {
 public:
  int SetSpeakerVolume(unsigned int volume) override;

  int GetSpeakerVolume(unsigned int& volume) override;

  int SetMicVolume(unsigned int volume) override;

  int GetMicVolume(unsigned int& volume) override;

  int SetInputMute(int channel, bool enable) override;

  int GetInputMute(int channel, bool& enabled) override;

  int GetSpeechInputLevel(unsigned int& level) override;

  int GetSpeechOutputLevel(int channel, unsigned int& level) override;

  int GetSpeechInputLevelFullRange(unsigned int& level) override;

  int GetSpeechOutputLevelFullRange(int channel, unsigned int& level) override;

  int SetChannelOutputVolumeScaling(int channel, float scaling) override;

  int GetChannelOutputVolumeScaling(int channel, float& scaling) override;

  int SetOutputVolumePan(int channel, float left, float right) override;

  int GetOutputVolumePan(int channel, float& left, float& right) override;

 protected:
  VoEVolumeControlImpl(voe::SharedData* shared);
  ~VoEVolumeControlImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_IMPL_H
