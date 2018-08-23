/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/runtime_enabled_features.h"

#include "rtc_base/flags.h"

namespace flags {
DEFINE_bool(enable_dual_stream_mode, false, "Enables dual video stream mode.");
}

namespace webrtc {
namespace runtime_enabled_features {

bool IsFeatureEnabled(std::string feature_name) {
  if (feature_name == kDualStreamModeFeatureName)
    return flags::FLAG_enable_dual_stream_mode;
  return false;
}

}  // namespace runtime_enabled_features
}  // namespace webrtc
