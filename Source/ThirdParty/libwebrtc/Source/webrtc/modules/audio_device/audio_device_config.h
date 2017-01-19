/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_CONFIG_H_
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_CONFIG_H_

// Enumerators
//
enum { kAdmMaxIdleTimeProcess = 1000 };
enum { GET_MIC_VOLUME_INTERVAL_MS = 1000 };

// Platform specifics
//
#if defined(_WIN32)
#if (_MSC_VER >= 1400)
// Windows Core Audio is the default audio layer in Windows.
// Only supported for VS 2005 and higher.
#define WEBRTC_WINDOWS_CORE_AUDIO_BUILD
#endif
#endif

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_CONFIG_H_

