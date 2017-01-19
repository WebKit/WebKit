/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_HARDWARE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_HARDWARE_IMPL_H

#include "webrtc/voice_engine/include/voe_hardware.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEHardwareImpl : public VoEHardware {
 public:
  int GetNumOfRecordingDevices(int& devices) override;

  int GetNumOfPlayoutDevices(int& devices) override;

  int GetRecordingDeviceName(int index,
                             char strNameUTF8[128],
                             char strGuidUTF8[128]) override;

  int GetPlayoutDeviceName(int index,
                           char strNameUTF8[128],
                           char strGuidUTF8[128]) override;

  int SetRecordingDevice(int index,
                         StereoChannel recordingChannel = kStereoBoth) override;

  int SetPlayoutDevice(int index) override;

  int SetAudioDeviceLayer(AudioLayers audioLayer) override;

  int GetAudioDeviceLayer(AudioLayers& audioLayer) override;

  int SetRecordingSampleRate(unsigned int samples_per_sec) override;
  int RecordingSampleRate(unsigned int* samples_per_sec) const override;
  int SetPlayoutSampleRate(unsigned int samples_per_sec) override;
  int PlayoutSampleRate(unsigned int* samples_per_sec) const override;

  bool BuiltInAECIsAvailable() const override;
  int EnableBuiltInAEC(bool enable) override;
  bool BuiltInAGCIsAvailable() const override;
  int EnableBuiltInAGC(bool enable) override;
  bool BuiltInNSIsAvailable() const override;
  int EnableBuiltInNS(bool enable) override;

 protected:
  VoEHardwareImpl(voe::SharedData* shared);
  ~VoEHardwareImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_HARDWARE_IMPL_H
