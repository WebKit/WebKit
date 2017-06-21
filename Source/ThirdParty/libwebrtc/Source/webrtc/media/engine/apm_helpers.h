/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_APM_HELPERS_H_
#define WEBRTC_MEDIA_ENGINE_APM_HELPERS_H_

namespace webrtc {

class AudioProcessing;
class AudioDeviceModule;

enum EcModes {
  kEcConference,     // Conferencing default (aggressive AEC).
  kEcAecm,           // AEC mobile.
};

struct AgcConfig {
  unsigned short targetLeveldBOv;
  unsigned short digitalCompressionGaindB;
  bool limiterEnable;
};

namespace apm_helpers {

AgcConfig GetAgcConfig(AudioProcessing* apm);
void SetAgcConfig(AudioProcessing* apm,
                  const AgcConfig& config);
void SetAgcStatus(AudioProcessing* apm,
                  AudioDeviceModule* adm,
                  bool enable);
void SetEcStatus(AudioProcessing* apm,
                 bool enable,
                 EcModes mode);
void SetEcMetricsStatus(AudioProcessing* apm, bool enable);
void SetAecmMode(AudioProcessing* apm, bool enable_cng);
void SetNsStatus(AudioProcessing* apm, bool enable);
void SetTypingDetectionStatus(AudioProcessing* apm, bool enable);

}  // namespace apm_helpers
}  // namespace webrtc

#endif  // WEBRTC_MEDIA_ENGINE_APM_HELPERS_H_
