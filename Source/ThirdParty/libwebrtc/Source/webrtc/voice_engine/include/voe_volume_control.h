/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Speaker volume controls.
//  - Microphone volume control.
//  - Non-linear speech level control.
//  - Mute functions.
//  - Additional stereo scaling methods.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoEVolumeControl* volume  = VoEVolumeControl::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  volume->SetInputMute(ch, true);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  volume->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_H
#define WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_H

#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoEVolumeControl {
 public:
  // Factory for the VoEVolumeControl sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoEVolumeControl* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoEVolumeControl sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Sets the speaker |volume| level. Valid range is [0,255].
  virtual int SetSpeakerVolume(unsigned int volume) = 0;

  // Gets the speaker |volume| level.
  virtual int GetSpeakerVolume(unsigned int& volume) = 0;

  // Sets the microphone volume level. Valid range is [0,255].
  virtual int SetMicVolume(unsigned int volume) = 0;

  // Gets the microphone volume level.
  virtual int GetMicVolume(unsigned int& volume) = 0;

  // Mutes the microphone input signal completely without affecting
  // the audio device volume.
  virtual int SetInputMute(int channel, bool enable) = 0;

  // Gets the current microphone input mute state.
  virtual int GetInputMute(int channel, bool& enabled) = 0;

  // Gets the microphone speech |level|, mapped non-linearly to the range
  // [0,9].
  virtual int GetSpeechInputLevel(unsigned int& level) = 0;

  // Gets the speaker speech |level|, mapped non-linearly to the range
  // [0,9].
  virtual int GetSpeechOutputLevel(int channel, unsigned int& level) = 0;

  // Gets the microphone speech |level|, mapped linearly to the range
  // [0,32768].
  virtual int GetSpeechInputLevelFullRange(unsigned int& level) = 0;

  // Gets the speaker speech |level|, mapped linearly to the range [0,32768].
  virtual int GetSpeechOutputLevelFullRange(int channel,
                                            unsigned int& level) = 0;

  // Sets a volume |scaling| applied to the outgoing signal of a specific
  // channel. Valid scale range is [0.0, 10.0].
  virtual int SetChannelOutputVolumeScaling(int channel, float scaling) = 0;

  // Gets the current volume scaling for a specified |channel|.
  virtual int GetChannelOutputVolumeScaling(int channel, float& scaling) = 0;

  // Scales volume of the |left| and |right| channels independently.
  // Valid scale range is [0.0, 1.0].
  virtual int SetOutputVolumePan(int channel, float left, float right) = 0;

  // Gets the current left and right scaling factors.
  virtual int GetOutputVolumePan(int channel, float& left, float& right) = 0;

 protected:
  VoEVolumeControl(){};
  virtual ~VoEVolumeControl(){};
};

}  // namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_H
