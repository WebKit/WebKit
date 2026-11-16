/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/simulcast_stream.h"

#include <optional>

#include "api/video_codecs/scalability_mode.h"
#include "rtc_base/checks.h"

namespace webrtc {

unsigned char SimulcastStream::GetNumberOfTemporalLayers() const {
  return numberOfTemporalLayers;
}
void SimulcastStream::SetNumberOfTemporalLayers(unsigned char n) {
  RTC_DCHECK_GE(n, 1);
  RTC_DCHECK_LE(n, 3);
  numberOfTemporalLayers = n;
}

std::optional<ScalabilityMode> SimulcastStream::GetScalabilityMode() const {
  static const ScalabilityMode scalability_modes[3] = {
      ScalabilityMode::kL1T1,
      ScalabilityMode::kL1T2,
      ScalabilityMode::kL1T3,
  };
  if (numberOfTemporalLayers < 1 || numberOfTemporalLayers > 3) {
    return std::nullopt;
  }
  return scalability_modes[numberOfTemporalLayers - 1];
}

bool SimulcastStream::operator==(const SimulcastStream& other) const {
  return (width == other.width && height == other.height &&
          maxFramerate == other.maxFramerate &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          maxBitrate == other.maxBitrate &&
          targetBitrate == other.targetBitrate &&
          minBitrate == other.minBitrate && qpMax == other.qpMax &&
          active == other.active);
}

}  // namespace webrtc
