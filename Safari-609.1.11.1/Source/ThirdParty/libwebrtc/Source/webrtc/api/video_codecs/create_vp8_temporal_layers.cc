/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/create_vp8_temporal_layers.h"
#include "absl/memory/memory.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/default_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

std::unique_ptr<Vp8TemporalLayers> CreateVp8TemporalLayers(
    Vp8TemporalLayersType type,
    int num_temporal_layers) {
  switch (type) {
    case Vp8TemporalLayersType::kFixedPattern:
      return absl::make_unique<DefaultTemporalLayers>(num_temporal_layers);
    case Vp8TemporalLayersType::kBitrateDynamic:
      // Conference mode temporal layering for screen content in base stream.
      return absl::make_unique<ScreenshareLayers>(num_temporal_layers,
                                                  Clock::GetRealTimeClock());
  }
}

}  // namespace webrtc
