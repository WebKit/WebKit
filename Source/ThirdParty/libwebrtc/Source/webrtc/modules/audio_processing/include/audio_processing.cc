/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/include/audio_processing.h"

#include "webrtc/base/checks.h"

namespace webrtc {

Beamforming::Beamforming()
    : enabled(false),
      array_geometry(),
      target_direction(
          SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f)) {}
Beamforming::Beamforming(bool enabled, const std::vector<Point>& array_geometry)
    : Beamforming(enabled,
                  array_geometry,
                  SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f)) {}

Beamforming::Beamforming(bool enabled,
                         const std::vector<Point>& array_geometry,
                         SphericalPointf target_direction)
    : enabled(enabled),
      array_geometry(array_geometry),
      target_direction(target_direction) {}

Beamforming::~Beamforming() {}
}  // namespace webrtc
