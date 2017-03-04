/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  This file contains common constants for VoiceEngine, as well as
 *  platform specific settings.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H
#define WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// VolumeControl
enum { kMinVolumeLevel = 0 };
enum { kMaxVolumeLevel = 255 };
// Min scale factor for per-channel volume scaling
const float kMinOutputVolumeScaling = 0.0f;
// Max scale factor for per-channel volume scaling
const float kMaxOutputVolumeScaling = 10.0f;
// Min scale factor for output volume panning
const float kMinOutputVolumePanning = 0.0f;
// Max scale factor for output volume panning
const float kMaxOutputVolumePanning = 1.0f;

enum { kVoiceEngineMaxIpPacketSizeBytes = 1500 };  // assumes Ethernet

// Audio processing
const NoiseSuppression::Level kDefaultNsMode = NoiseSuppression::kModerate;
const GainControl::Mode kDefaultAgcMode =
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
    GainControl::kAdaptiveDigital;
#else
    GainControl::kAdaptiveAnalog;
#endif
const bool kDefaultAgcState =
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
    false;
#else
    true;
#endif
const GainControl::Mode kDefaultRxAgcMode = GainControl::kAdaptiveDigital;

// VideoSync
// Lowest minimum playout delay
enum { kVoiceEngineMinMinPlayoutDelayMs = 0 };
// Highest minimum playout delay
enum { kVoiceEngineMaxMinPlayoutDelayMs = 10000 };

// RTP/RTCP
// Min 4-bit ID for RTP extension (see section 4.2 in RFC 5285)
enum { kVoiceEngineMinRtpExtensionId = 1 };
// Max 4-bit ID for RTP extension
enum { kVoiceEngineMaxRtpExtensionId = 14 };

}  // namespace webrtc

#define NOT_SUPPORTED(stat)                 \
  LOG_F(LS_ERROR) << "not supported";       \
  stat.SetLastError(VE_FUNC_NOT_SUPPORTED); \
  return -1;

namespace webrtc {

inline int VoEId(int veId, int chId) {
  if (chId == -1) {
    const int dummyChannel(99);
    return (int)((veId << 16) + dummyChannel);
  }
  return (int)((veId << 16) + chId);
}

inline int VoEModuleId(int veId, int chId) {
  return (int)((veId << 16) + chId);
}

// Convert module ID to internal VoE channel ID
inline int VoEChannelId(int moduleId) {
  return (int)(moduleId & 0xffff);
}

}  // namespace webrtc

#if defined(_WIN32)
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE \
  AudioDeviceModule::kDefaultCommunicationDevice
#else
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE 0
#endif  // #if (defined(_WIN32)

#endif  // WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H
