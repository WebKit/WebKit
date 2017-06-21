/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_ADM_HELPERS_H_
#define WEBRTC_MEDIA_ENGINE_ADM_HELPERS_H_

#include "webrtc/common_types.h"

namespace webrtc {

class AudioDeviceModule;

namespace adm_helpers {

void SetRecordingDevice(AudioDeviceModule* adm);
void SetPlayoutDevice(AudioDeviceModule* adm);

}  // namespace adm_helpers
}  // namespace webrtc

#endif  // WEBRTC_MEDIA_ENGINE_ADM_HELPERS_H_
