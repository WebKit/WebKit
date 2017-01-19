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
 *  platform specific settings and include files.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H
#define WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/voice_engine_configurations.h"

// ----------------------------------------------------------------------------
//  Enumerators
// ----------------------------------------------------------------------------

namespace webrtc {

// Internal buffer size required for mono audio, based on the highest sample
// rate voice engine supports (10 ms of audio at 192 kHz).
static const size_t kMaxMonoDataSizeSamples = 1920;

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

// DTMF
enum { kMinDtmfEventCode = 0 };         // DTMF digit "0"
enum { kMaxDtmfEventCode = 15 };        // DTMF digit "D"
enum { kMinTelephoneEventCode = 0 };    // RFC4733 (Section 2.3.1)
enum { kMaxTelephoneEventCode = 255 };  // RFC4733 (Section 2.3.1)
enum { kMinTelephoneEventDuration = 100 };
enum { kMaxTelephoneEventDuration = 60000 };       // Actual limit is 2^16
enum { kMinTelephoneEventAttenuation = 0 };        // 0 dBm0
enum { kMaxTelephoneEventAttenuation = 36 };       // -36 dBm0
enum { kMinTelephoneEventSeparationMs = 100 };     // Min delta time between two
                                                   // telephone events
enum { kVoiceEngineMaxIpPacketSizeBytes = 1500 };  // assumes Ethernet

enum { kVoiceEngineMaxModuleVersionSize = 960 };

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

// Codec
// Min init target rate for iSAC-wb
enum { kVoiceEngineMinIsacInitTargetRateBpsWb = 10000 };
// Max init target rate for iSAC-wb
enum { kVoiceEngineMaxIsacInitTargetRateBpsWb = 32000 };
// Min init target rate for iSAC-swb
enum { kVoiceEngineMinIsacInitTargetRateBpsSwb = 10000 };
// Max init target rate for iSAC-swb
enum { kVoiceEngineMaxIsacInitTargetRateBpsSwb = 56000 };
// Lowest max rate for iSAC-wb
enum { kVoiceEngineMinIsacMaxRateBpsWb = 32000 };
// Highest max rate for iSAC-wb
enum { kVoiceEngineMaxIsacMaxRateBpsWb = 53400 };
// Lowest max rate for iSAC-swb
enum { kVoiceEngineMinIsacMaxRateBpsSwb = 32000 };
// Highest max rate for iSAC-swb
enum { kVoiceEngineMaxIsacMaxRateBpsSwb = 107000 };
// Lowest max payload size for iSAC-wb
enum { kVoiceEngineMinIsacMaxPayloadSizeBytesWb = 120 };
// Highest max payload size for iSAC-wb
enum { kVoiceEngineMaxIsacMaxPayloadSizeBytesWb = 400 };
// Lowest max payload size for iSAC-swb
enum { kVoiceEngineMinIsacMaxPayloadSizeBytesSwb = 120 };
// Highest max payload size for iSAC-swb
enum { kVoiceEngineMaxIsacMaxPayloadSizeBytesSwb = 600 };

// VideoSync
// Lowest minimum playout delay
enum { kVoiceEngineMinMinPlayoutDelayMs = 0 };
// Highest minimum playout delay
enum { kVoiceEngineMaxMinPlayoutDelayMs = 10000 };

// Network
// Min packet-timeout time for received RTP packets
enum { kVoiceEngineMinPacketTimeoutSec = 1 };
// Max packet-timeout time for received RTP packets
enum { kVoiceEngineMaxPacketTimeoutSec = 150 };
// Min sample time for dead-or-alive detection
enum { kVoiceEngineMinSampleTimeSec = 1 };
// Max sample time for dead-or-alive detection
enum { kVoiceEngineMaxSampleTimeSec = 150 };

// RTP/RTCP
// Min 4-bit ID for RTP extension (see section 4.2 in RFC 5285)
enum { kVoiceEngineMinRtpExtensionId = 1 };
// Max 4-bit ID for RTP extension
enum { kVoiceEngineMaxRtpExtensionId = 14 };

}  // namespace webrtc

// ----------------------------------------------------------------------------
//  Macros
// ----------------------------------------------------------------------------

#define NOT_SUPPORTED(stat)                 \
  LOG_F(LS_ERROR) << "not supported";       \
  stat.SetLastError(VE_FUNC_NOT_SUPPORTED); \
  return -1;

#if (!defined(NDEBUG) && defined(_WIN32) && (_MSC_VER >= 1400))
#include <windows.h>
#include <stdio.h>
#define DEBUG_PRINT(...)       \
  {                            \
    char msg[256];             \
    sprintf(msg, __VA_ARGS__); \
    OutputDebugStringA(msg);   \
  }
#else
// special fix for visual 2003
#define DEBUG_PRINT(exp) ((void)0)
#endif  // !defined(NDEBUG) && defined(_WIN32)

#define CHECK_CHANNEL(channel)     \
  if (CheckChannel(channel) == -1) \
    return -1;

// ----------------------------------------------------------------------------
//  Inline functions
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
//  Platform settings
// ----------------------------------------------------------------------------

// *** WINDOWS ***

#if defined(_WIN32)

#include <windows.h>

#pragma comment(lib, "winmm.lib")

#ifndef WEBRTC_EXTERNAL_TRANSPORT
#pragma comment(lib, "ws2_32.lib")
#endif

// ----------------------------------------------------------------------------
//  Defines
// ----------------------------------------------------------------------------

// Default device for Windows PC
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE \
  AudioDeviceModule::kDefaultCommunicationDevice

#endif  // #if (defined(_WIN32)

// *** LINUX ***

#ifdef WEBRTC_LINUX

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#ifndef QNX
#include <linux/net.h>
#ifndef ANDROID
#include <sys/soundcard.h>
#endif  // ANDROID
#endif  // QNX
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DWORD unsigned long int
#define WINAPI
#define LPVOID void *
#define FALSE 0
#define TRUE 1
#define UINT unsigned int
#define UCHAR unsigned char
#define TCHAR char
#ifdef QNX
#define _stricmp stricmp
#else
#define _stricmp strcasecmp
#endif
#define GetLastError() errno
#define WSAGetLastError() errno
#define LPCTSTR const char *
#define LPCSTR const char *
#define wsprintf sprintf
#define TEXT(a) a
#define _ftprintf fprintf
#define _tcslen strlen
#define FAR
#define __cdecl
#define LPSOCKADDR struct sockaddr *

// Default device for Linux and Android
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE 0

#endif  // #ifdef WEBRTC_LINUX

// *** WEBRTC_MAC ***
// including iPhone

#ifdef WEBRTC_MAC

#include <AudioUnit/AudioUnit.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#if !defined(WEBRTC_IOS)
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/DefaultAudioOutput.h>
#include <AudioToolbox/AudioConverter.h>
#include <CoreAudio/HostTime.h>
#endif

#define DWORD unsigned long int
#define WINAPI
#define LPVOID void *
#define FALSE 0
#define TRUE 1
#define SOCKADDR_IN struct sockaddr_in
#define UINT unsigned int
#define UCHAR unsigned char
#define TCHAR char
#define _stricmp strcasecmp
#define GetLastError() errno
#define WSAGetLastError() errno
#define LPCTSTR const char *
#define wsprintf sprintf
#define TEXT(a) a
#define _ftprintf fprintf
#define _tcslen strlen
#define FAR
#define __cdecl
#define LPSOCKADDR struct sockaddr *
#define LPCSTR const char *
#define ULONG unsigned long

// Default device for Mac and iPhone
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE 0
#endif  // #ifdef WEBRTC_MAC

#endif  // WEBRTC_VOICE_ENGINE_VOICE_ENGINE_DEFINES_H
