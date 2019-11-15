/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_APM_HELPERS_H_
#define MEDIA_ENGINE_APM_HELPERS_H_

#include <cstdint>

namespace webrtc {

class AudioProcessing;

enum EcModes {
  kEcConference,  // Conferencing default (aggressive AEC).
  kEcAecm,        // AEC mobile.
};

namespace apm_helpers {

void Init(AudioProcessing* apm);
void SetEcStatus(AudioProcessing* apm, bool enable, EcModes mode);
void SetEcMetricsStatus(AudioProcessing* apm, bool enable);
void SetAecmMode(AudioProcessing* apm, bool enable_cng);
void SetNsStatus(AudioProcessing* apm, bool enable);

}  // namespace apm_helpers
}  // namespace webrtc

#endif  // MEDIA_ENGINE_APM_HELPERS_H_
