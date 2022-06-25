/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/svc_config.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
const size_t kMinVp9SvcBitrateKbps = 30;

const size_t kMaxNumLayersForScreenSharing = 3;
const float kMaxScreenSharingLayerFramerateFps[] = {5.0, 10.0, 30.0};
const size_t kMinScreenSharingLayerBitrateKbps[] = {30, 200, 500};
const size_t kTargetScreenSharingLayerBitrateKbps[] = {150, 350, 950};
const size_t kMaxScreenSharingLayerBitrateKbps[] = {250, 500, 950};

}  // namespace

std::vector<SpatialLayer> ConfigureSvcScreenSharing(size_t input_width,
                                                    size_t input_height,
                                                    float max_framerate_fps,
                                                    size_t num_spatial_layers) {
  num_spatial_layers =
      std::min(num_spatial_layers, kMaxNumLayersForScreenSharing);
  std::vector<SpatialLayer> spatial_layers;

  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    SpatialLayer spatial_layer = {0};
    spatial_layer.width = input_width;
    spatial_layer.height = input_height;
    spatial_layer.maxFramerate =
        std::min(kMaxScreenSharingLayerFramerateFps[sl_idx], max_framerate_fps);
    spatial_layer.numberOfTemporalLayers = 1;
    spatial_layer.minBitrate =
        static_cast<int>(kMinScreenSharingLayerBitrateKbps[sl_idx]);
    spatial_layer.maxBitrate =
        static_cast<int>(kMaxScreenSharingLayerBitrateKbps[sl_idx]);
    spatial_layer.targetBitrate =
        static_cast<int>(kTargetScreenSharingLayerBitrateKbps[sl_idx]);
    spatial_layer.active = true;
    spatial_layers.push_back(spatial_layer);
  }

  return spatial_layers;
}

std::vector<SpatialLayer> ConfigureSvcNormalVideo(size_t input_width,
                                                  size_t input_height,
                                                  float max_framerate_fps,
                                                  size_t first_active_layer,
                                                  size_t num_spatial_layers,
                                                  size_t num_temporal_layers) {
  RTC_DCHECK_LT(first_active_layer, num_spatial_layers);
  std::vector<SpatialLayer> spatial_layers;

  // Limit number of layers for given resolution.
  const size_t num_layers_fit_horz = static_cast<size_t>(std::floor(
      1 + std::max(0.0f,
                   std::log2(1.0f * input_width / kMinVp9SpatialLayerWidth))));
  const size_t num_layers_fit_vert = static_cast<size_t>(
      std::floor(1 + std::max(0.0f, std::log2(1.0f * input_height /
                                              kMinVp9SpatialLayerHeight))));
  const size_t limited_num_spatial_layers =
      std::min(num_layers_fit_horz, num_layers_fit_vert);
  if (limited_num_spatial_layers < num_spatial_layers) {
    RTC_LOG(LS_WARNING) << "Reducing number of spatial layers from "
                        << num_spatial_layers << " to "
                        << limited_num_spatial_layers
                        << " due to low input resolution.";
    num_spatial_layers = limited_num_spatial_layers;
  }
  // First active layer must be configured.
  num_spatial_layers = std::max(num_spatial_layers, first_active_layer + 1);

  // Ensure top layer is even enough.
  int required_divisiblity = 1 << (num_spatial_layers - first_active_layer - 1);
  input_width = input_width - input_width % required_divisiblity;
  input_height = input_height - input_height % required_divisiblity;

  for (size_t sl_idx = first_active_layer; sl_idx < num_spatial_layers;
       ++sl_idx) {
    SpatialLayer spatial_layer = {0};
    spatial_layer.width = input_width >> (num_spatial_layers - sl_idx - 1);
    spatial_layer.height = input_height >> (num_spatial_layers - sl_idx - 1);
    spatial_layer.maxFramerate = max_framerate_fps;
    spatial_layer.numberOfTemporalLayers = num_temporal_layers;
    spatial_layer.active = true;

    // minBitrate and maxBitrate formulas were derived from
    // subjective-quality data to determing bit rates below which video
    // quality is unacceptable and above which additional bits do not provide
    // benefit. The formulas express rate in units of kbps.

    // TODO(ssilkin): Add to the comment PSNR/SSIM we get at encoding certain
    // video to min/max bitrate specified by those formulas.
    const size_t num_pixels = spatial_layer.width * spatial_layer.height;
    int min_bitrate =
        static_cast<int>((600. * std::sqrt(num_pixels) - 95000.) / 1000.);
    min_bitrate = std::max(min_bitrate, 0);
    spatial_layer.minBitrate =
        std::max(static_cast<size_t>(min_bitrate), kMinVp9SvcBitrateKbps);
    spatial_layer.maxBitrate =
        static_cast<int>((1.6 * num_pixels + 50 * 1000) / 1000);
    spatial_layer.targetBitrate =
        (spatial_layer.minBitrate + spatial_layer.maxBitrate) / 2;
    spatial_layers.push_back(spatial_layer);
  }

  // A workaround for sitiation when single HD layer is left with minBitrate
  // about 500kbps. This would mean that there will always be at least 500kbps
  // allocated to video regardless of how low is the actual BWE.
  // Also, boost maxBitrate for the first layer to account for lost ability to
  // predict from previous layers.
  if (first_active_layer > 0) {
    spatial_layers[0].minBitrate = kMinVp9SvcBitrateKbps;
    // TODO(ilnik): tune this value or come up with a different formula to
    // ensure that all singlecast configurations look good and not too much
    // bitrate is added.
    spatial_layers[0].maxBitrate *= 1.1;
  }

  return spatial_layers;
}

std::vector<SpatialLayer> GetSvcConfig(size_t input_width,
                                       size_t input_height,
                                       float max_framerate_fps,
                                       size_t first_active_layer,
                                       size_t num_spatial_layers,
                                       size_t num_temporal_layers,
                                       bool is_screen_sharing) {
  RTC_DCHECK_GT(input_width, 0);
  RTC_DCHECK_GT(input_height, 0);
  RTC_DCHECK_GT(num_spatial_layers, 0);
  RTC_DCHECK_GT(num_temporal_layers, 0);

  if (is_screen_sharing) {
    return ConfigureSvcScreenSharing(input_width, input_height,
                                     max_framerate_fps, num_spatial_layers);
  } else {
    return ConfigureSvcNormalVideo(input_width, input_height, max_framerate_fps,
                                   first_active_layer, num_spatial_layers,
                                   num_temporal_layers);
  }
}

}  // namespace webrtc
