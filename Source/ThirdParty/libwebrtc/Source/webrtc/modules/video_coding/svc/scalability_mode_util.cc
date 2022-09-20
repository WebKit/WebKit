/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/svc/scalability_mode_util.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/video_codecs/scalability_mode.h"
#include "rtc_base/checks.h"

namespace webrtc {

absl::optional<ScalabilityMode> ScalabilityModeFromString(
    absl::string_view mode_string) {
  if (mode_string == "L1T1")
    return ScalabilityMode::kL1T1;
  if (mode_string == "L1T2")
    return ScalabilityMode::kL1T2;
  if (mode_string == "L1T2h")
    return ScalabilityMode::kL1T2h;
  if (mode_string == "L1T3")
    return ScalabilityMode::kL1T3;
  if (mode_string == "L1T3h")
    return ScalabilityMode::kL1T3h;

  if (mode_string == "L2T1")
    return ScalabilityMode::kL2T1;
  if (mode_string == "L2T1h")
    return ScalabilityMode::kL2T1h;
  if (mode_string == "L2T1_KEY")
    return ScalabilityMode::kL2T1_KEY;

  if (mode_string == "L2T2")
    return ScalabilityMode::kL2T2;
  if (mode_string == "L2T2h")
    return ScalabilityMode::kL2T2h;
  if (mode_string == "L2T2_KEY")
    return ScalabilityMode::kL2T2_KEY;
  if (mode_string == "L2T2_KEY_SHIFT")
    return ScalabilityMode::kL2T2_KEY_SHIFT;
  if (mode_string == "L2T3")
    return ScalabilityMode::kL2T3;
  if (mode_string == "L2T3h")
    return ScalabilityMode::kL2T3h;
  if (mode_string == "L2T3_KEY")
    return ScalabilityMode::kL2T3_KEY;

  if (mode_string == "L3T1")
    return ScalabilityMode::kL3T1;
  if (mode_string == "L3T1h")
    return ScalabilityMode::kL3T1h;
  if (mode_string == "L3T1_KEY")
    return ScalabilityMode::kL3T1_KEY;

  if (mode_string == "L3T2")
    return ScalabilityMode::kL3T2;
  if (mode_string == "L3T2h")
    return ScalabilityMode::kL3T2h;
  if (mode_string == "L3T2_KEY")
    return ScalabilityMode::kL3T2_KEY;

  if (mode_string == "L3T3")
    return ScalabilityMode::kL3T3;
  if (mode_string == "L3T3h")
    return ScalabilityMode::kL3T3h;
  if (mode_string == "L3T3_KEY")
    return ScalabilityMode::kL3T3_KEY;

  if (mode_string == "S2T1")
    return ScalabilityMode::kS2T1;
  if (mode_string == "S2T3")
    return ScalabilityMode::kS2T3;
  if (mode_string == "S3T3")
    return ScalabilityMode::kS3T3;

  return absl::nullopt;
}

InterLayerPredMode ScalabilityModeToInterLayerPredMode(
    ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
    case ScalabilityMode::kL1T2:
    case ScalabilityMode::kL1T2h:
    case ScalabilityMode::kL1T3:
    case ScalabilityMode::kL1T3h:
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL2T1_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL2T3_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL3T1_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL3T2_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3h:
      return InterLayerPredMode::kOn;
    case ScalabilityMode::kL3T3_KEY:
      return InterLayerPredMode::kOnKeyPic;
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS2T3:
    case ScalabilityMode::kS3T3:
      return InterLayerPredMode::kOff;
  }
  RTC_CHECK_NOTREACHED();
}

int ScalabilityModeToNumSpatialLayers(ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
    case ScalabilityMode::kL1T2:
    case ScalabilityMode::kL1T2h:
    case ScalabilityMode::kL1T3:
    case ScalabilityMode::kL1T3h:
      return 1;
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1h:
    case ScalabilityMode::kL2T1_KEY:
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2h:
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3h:
    case ScalabilityMode::kL2T3_KEY:
      return 2;
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1h:
    case ScalabilityMode::kL3T1_KEY:
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2h:
    case ScalabilityMode::kL3T2_KEY:
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3h:
    case ScalabilityMode::kL3T3_KEY:
      return 3;
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS2T3:
      return 2;
    case ScalabilityMode::kS3T3:
      return 3;
  }
  RTC_CHECK_NOTREACHED();
}

int ScalabilityModeToNumTemporalLayers(ScalabilityMode scalability_mode) {
  switch (scalability_mode) {
    case ScalabilityMode::kL1T1:
      return 1;
    case ScalabilityMode::kL1T2:
    case ScalabilityMode::kL1T2h:
      return 2;
    case ScalabilityMode::kL1T3:
    case ScalabilityMode::kL1T3h:
      return 3;
    case ScalabilityMode::kL2T1:
    case ScalabilityMode::kL2T1h:
    case ScalabilityMode::kL2T1_KEY:
      return 1;
    case ScalabilityMode::kL2T2:
    case ScalabilityMode::kL2T2h:
    case ScalabilityMode::kL2T2_KEY:
    case ScalabilityMode::kL2T2_KEY_SHIFT:
      return 2;
    case ScalabilityMode::kL2T3:
    case ScalabilityMode::kL2T3h:
    case ScalabilityMode::kL2T3_KEY:
      return 3;
    case ScalabilityMode::kL3T1:
    case ScalabilityMode::kL3T1h:
    case ScalabilityMode::kL3T1_KEY:
      return 1;
    case ScalabilityMode::kL3T2:
    case ScalabilityMode::kL3T2h:
    case ScalabilityMode::kL3T2_KEY:
      return 2;
    case ScalabilityMode::kL3T3:
    case ScalabilityMode::kL3T3h:
    case ScalabilityMode::kL3T3_KEY:
      return 3;
    case ScalabilityMode::kS2T1:
      return 1;
    case ScalabilityMode::kS2T3:
    case ScalabilityMode::kS3T3:
      return 3;
  }
  RTC_CHECK_NOTREACHED();
}

}  // namespace webrtc
