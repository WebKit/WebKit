/* Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/include/vp8_temporal_layers.h"

#include "absl/memory/memory.h"
#include "modules/video_coding/codecs/vp8/default_temporal_layers.h"
#include "modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

bool TemporalLayers::FrameConfig::operator==(const FrameConfig& o) const {
  return drop_frame == o.drop_frame &&
         last_buffer_flags == o.last_buffer_flags &&
         golden_buffer_flags == o.golden_buffer_flags &&
         arf_buffer_flags == o.arf_buffer_flags && layer_sync == o.layer_sync &&
         freeze_entropy == o.freeze_entropy &&
         encoder_layer_id == o.encoder_layer_id &&
         packetizer_temporal_idx == o.packetizer_temporal_idx;
}

std::unique_ptr<TemporalLayers> TemporalLayers::CreateTemporalLayers(
    TemporalLayersType type,
    int num_temporal_layers) {
  switch (type) {
    case TemporalLayersType::kFixedPattern:
      return absl::make_unique<DefaultTemporalLayers>(num_temporal_layers);
    case TemporalLayersType::kBitrateDynamic:
      // Conference mode temporal layering for screen content in base stream.
      return absl::make_unique<ScreenshareLayers>(num_temporal_layers,
                                                  Clock::GetRealTimeClock());
  }
}

}  // namespace webrtc
