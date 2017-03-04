/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_VARIABILITY_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_VARIABILITY_H_

namespace webrtc {

struct EchoPathVariability {
  EchoPathVariability(bool gain_change, bool delay_change);

  bool AudioPathChanged() const { return gain_change || delay_change; }
  bool gain_change;
  bool delay_change;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_VARIABILITY_H_
