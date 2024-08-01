/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CONFIG_SIMULCAST_H_
#define VIDEO_CONFIG_SIMULCAST_H_

#include <stddef.h>

#include <vector>

#include "api/array_view.h"
#include "api/field_trials_view.h"
#include "api/units/data_rate.h"
#include "api/video/resolution.h"
#include "video/config/video_encoder_config.h"

namespace cricket {

// Gets the total maximum bitrate for the `streams`.
webrtc::DataRate GetTotalMaxBitrate(
    const std::vector<webrtc::VideoStream>& streams);

// Adds any bitrate of `max_bitrate` that is above the total maximum bitrate for
// the `layers` to the highest quality layer.
void BoostMaxSimulcastLayer(webrtc::DataRate max_bitrate,
                            std::vector<webrtc::VideoStream>* layers);

// Returns number of simulcast streams. The value depends on the resolution and
// is restricted to the range from `min_num_layers` to `max_num_layers`,
// inclusive.
size_t LimitSimulcastLayerCount(size_t min_num_layers,
                                size_t max_num_layers,
                                int width,
                                int height,
                                const webrtc::FieldTrialsView& trials,
                                webrtc::VideoCodecType codec);

// Gets simulcast settings.
std::vector<webrtc::VideoStream> GetSimulcastConfig(
    rtc::ArrayView<const webrtc::Resolution> resolutions,
    bool is_screenshare_with_conference_mode,
    bool temporal_layers_supported,
    const webrtc::FieldTrialsView& trials,
    webrtc::VideoCodecType codec);

}  // namespace cricket

#endif  // VIDEO_CONFIG_SIMULCAST_H_
