/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <algorithm>
#include <string>

#include "media/base/mediaconstants.h"
#include "media/base/streamparams.h"
#include "media/engine/constants.h"
#include "media/engine/simulcast.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace cricket {

namespace {

constexpr char kUseBaseHeavyVP8TL3RateAllocationFieldTrial[] =
    "WebRTC-UseBaseHeavyVP8TL3RateAllocation";

// Limits for legacy conference screensharing mode. Currently used for the
// lower of the two simulcast streams.
constexpr int kScreenshareDefaultTl0BitrateKbps = 200;
constexpr int kScreenshareDefaultTl1BitrateKbps = 1000;

// Min/max bitrate for the higher one of the two simulcast stream used for
// screen content.
constexpr int kScreenshareHighStreamMinBitrateBps = 600000;
constexpr int kScreenshareHighStreamMaxBitrateBps = 1250000;
static const char* kSimulcastScreenshareFieldTrialName =
    "WebRTC-SimulcastScreenshare";

}  // namespace

struct SimulcastFormat {
  int width;
  int height;
  // The maximum number of simulcast layers can be used for
  // resolutions at |widthxheigh|.
  size_t max_layers;
  // The maximum bitrate for encoding stream at |widthxheight|, when we are
  // not sending the next higher spatial stream.
  int max_bitrate_kbps;
  // The target bitrate for encoding stream at |widthxheight|, when this layer
  // is not the highest layer (i.e., when we are sending another higher spatial
  // stream).
  int target_bitrate_kbps;
  // The minimum bitrate needed for encoding stream at |widthxheight|.
  int min_bitrate_kbps;
};

// These tables describe from which resolution we can use how many
// simulcast layers at what bitrates (maximum, target, and minimum).
// Important!! Keep this table from high resolution to low resolution.
// clang-format off
const SimulcastFormat kSimulcastFormats[] = {
  {1920, 1080, 3, 5000, 4000, 800},
  {1280, 720, 3,  2500, 2500, 600},
  {960, 540, 3, 900, 900, 450},
  {640, 360, 2, 700, 500, 150},
  {480, 270, 2, 450, 350, 150},
  {320, 180, 1, 200, 150, 30},
  {0, 0, 1, 200, 150, 30}
};
// clang-format on

const int kMaxScreenshareSimulcastLayers = 2;

// Multiway: Number of temporal layers for each simulcast stream.
int DefaultNumberOfTemporalLayers(int simulcast_id, bool screenshare) {
  RTC_CHECK_GE(simulcast_id, 0);
  RTC_CHECK_LT(simulcast_id, webrtc::kMaxSimulcastStreams);

  const int kDefaultNumTemporalLayers = 3;
  const int kDefaultNumScreenshareTemporalLayers = 2;
  int default_num_temporal_layers = screenshare
                                        ? kDefaultNumScreenshareTemporalLayers
                                        : kDefaultNumTemporalLayers;

  const std::string group_name =
      screenshare ? webrtc::field_trial::FindFullName(
                        "WebRTC-VP8ScreenshareTemporalLayers")
                  : webrtc::field_trial::FindFullName(
                        "WebRTC-VP8ConferenceTemporalLayers");
  if (group_name.empty())
    return default_num_temporal_layers;

  int num_temporal_layers = default_num_temporal_layers;
  if (sscanf(group_name.c_str(), "%d", &num_temporal_layers) == 1 &&
      num_temporal_layers > 0 &&
      num_temporal_layers <= webrtc::kMaxTemporalStreams) {
    return num_temporal_layers;
  }

  RTC_LOG(LS_WARNING) << "Attempt to set number of temporal layers to "
                         "incorrect value: "
                      << group_name;

  return default_num_temporal_layers;
}

int FindSimulcastFormatIndex(int width, int height) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  for (uint32_t i = 0; i < arraysize(kSimulcastFormats); ++i) {
    if (width * height >=
        kSimulcastFormats[i].width * kSimulcastFormats[i].height) {
      return i;
    }
  }
  RTC_NOTREACHED();
  return -1;
}

int FindSimulcastFormatIndex(int width, int height, size_t max_layers) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  RTC_DCHECK_GT(max_layers, 0);
  for (uint32_t i = 0; i < arraysize(kSimulcastFormats); ++i) {
    if (width * height >=
            kSimulcastFormats[i].width * kSimulcastFormats[i].height &&
        max_layers == kSimulcastFormats[i].max_layers) {
      return i;
    }
  }
  RTC_NOTREACHED();
  return -1;
}

// Simulcast stream width and height must both be dividable by
// |2 ^ (simulcast_layers - 1)|.
int NormalizeSimulcastSize(int size, size_t simulcast_layers) {
  const int base2_exponent = static_cast<int>(simulcast_layers) - 1;
  return ((size >> base2_exponent) << base2_exponent);
}

size_t FindSimulcastMaxLayers(int width, int height) {
  int index = FindSimulcastFormatIndex(width, height);
  return kSimulcastFormats[index].max_layers;
}

int FindSimulcastMaxBitrateBps(int width, int height) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  return kSimulcastFormats[format_index].max_bitrate_kbps * 1000;
}

int FindSimulcastTargetBitrateBps(int width, int height) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  return kSimulcastFormats[format_index].target_bitrate_kbps * 1000;
}

int FindSimulcastMinBitrateBps(int width, int height) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  return kSimulcastFormats[format_index].min_bitrate_kbps * 1000;
}

void SlotSimulcastMaxResolution(size_t max_layers, int* width, int* height) {
  int index = FindSimulcastFormatIndex(*width, *height, max_layers);
  *width = kSimulcastFormats[index].width;
  *height = kSimulcastFormats[index].height;
  RTC_LOG(LS_INFO) << "SlotSimulcastMaxResolution to width:" << *width
                   << " height:" << *height;
}

void BoostMaxSimulcastLayer(int max_bitrate_bps,
                            std::vector<webrtc::VideoStream>* layers) {
  if (layers->empty())
    return;

  // Spend additional bits to boost the max layer.
  int bitrate_left_bps = max_bitrate_bps - GetTotalMaxBitrateBps(*layers);
  if (bitrate_left_bps > 0) {
    layers->back().max_bitrate_bps += bitrate_left_bps;
  }
}

int GetTotalMaxBitrateBps(const std::vector<webrtc::VideoStream>& layers) {
  if (layers.empty())
    return 0;

  int total_max_bitrate_bps = 0;
  for (size_t s = 0; s < layers.size() - 1; ++s) {
    total_max_bitrate_bps += layers[s].target_bitrate_bps;
  }
  total_max_bitrate_bps += layers.back().max_bitrate_bps;
  return total_max_bitrate_bps;
}

std::vector<webrtc::VideoStream> GetSimulcastConfig(
    size_t max_layers,
    int width,
    int height,
    int /*max_bitrate_bps*/,
    double bitrate_priority,
    int max_qp,
    int /*max_framerate*/,
    bool is_screenshare,
    bool temporal_layers_supported) {
  if (is_screenshare) {
    return GetScreenshareLayers(max_layers, width, height, bitrate_priority,
                                max_qp, ScreenshareSimulcastFieldTrialEnabled(),
                                temporal_layers_supported);
  } else {
    return GetNormalSimulcastLayers(max_layers, width, height, bitrate_priority,
                                    max_qp, temporal_layers_supported);
  }
}

std::vector<webrtc::VideoStream> GetNormalSimulcastLayers(
    size_t max_layers,
    int width,
    int height,
    double bitrate_priority,
    int max_qp,
    bool temporal_layers_supported) {
  // TODO(bugs.webrtc.org/8785): Currently if the resolution isn't large enough
  // (defined in kSimulcastFormats) we scale down the number of simulcast
  // layers. Consider changing this so that the application can have more
  // control over exactly how many simulcast layers are used.
  size_t num_simulcast_layers = FindSimulcastMaxLayers(width, height);
  if (webrtc::field_trial::IsEnabled("WebRTC-SimulcastMaxLayers")) {
    num_simulcast_layers = max_layers;
  }
  if (num_simulcast_layers > max_layers) {
    // TODO(bugs.webrtc.org/8486): This scales down the resolution if the
    // number of simulcast layers created by the application isn't sufficient
    // (defined in kSimulcastFormats). For example if the input frame's
    // resolution is HD, but there are only 2 simulcast layers, the
    // resolution gets scaled down to VGA. Consider taking this logic out to
    // allow the application more control over the resolutions.
    SlotSimulcastMaxResolution(max_layers, &width, &height);
    num_simulcast_layers = max_layers;
  }
  std::vector<webrtc::VideoStream> layers(num_simulcast_layers);

  // Format width and height has to be divisible by |2 ^ num_simulcast_layers -
  // 1|.
  width = NormalizeSimulcastSize(width, num_simulcast_layers);
  height = NormalizeSimulcastSize(height, num_simulcast_layers);
  // Add simulcast streams, from highest resolution (|s| = num_simulcast_layers
  // -1) to lowest resolution at |s| = 0.
  for (size_t s = num_simulcast_layers - 1;; --s) {
    layers[s].width = width;
    layers[s].height = height;
    // TODO(pbos): Fill actual temporal-layer bitrate thresholds.
    layers[s].max_qp = max_qp;
    layers[s].num_temporal_layers =
        temporal_layers_supported ? DefaultNumberOfTemporalLayers(s, false) : 0;
    layers[s].max_bitrate_bps = FindSimulcastMaxBitrateBps(width, height);
    layers[s].target_bitrate_bps = FindSimulcastTargetBitrateBps(width, height);
    int num_temporal_layers = DefaultNumberOfTemporalLayers(s, false);
    if (s == 0) {
      // If alternative temporal rate allocation is selected, adjust the
      // bitrate of the lowest simulcast stream so that absolute bitrate for
      // the base temporal layer matches the bitrate for the base temporal
      // layer with the default 3 simulcast streams. Otherwise we risk a
      // higher threshold for receiving a feed at all.
      float rate_factor = 1.0;
      if (num_temporal_layers == 3) {
        if (webrtc::field_trial::IsEnabled(
                kUseBaseHeavyVP8TL3RateAllocationFieldTrial)) {
          // Base heavy allocation increases TL0 bitrate from 40% to 60%.
          rate_factor = 0.4 / 0.6;
        }
      } else {
        rate_factor =
            webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(3, 0) /
            webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                num_temporal_layers, 0);
      }

      layers[s].max_bitrate_bps =
          static_cast<int>(layers[s].max_bitrate_bps * rate_factor);
      layers[s].target_bitrate_bps =
          static_cast<int>(layers[s].target_bitrate_bps * rate_factor);
    }
    layers[s].min_bitrate_bps = FindSimulcastMinBitrateBps(width, height);
    layers[s].max_framerate = kDefaultVideoMaxFramerate;

    width /= 2;
    height /= 2;

    if (s == 0) {
      break;
    }
  }
  // Currently the relative bitrate priority of the sender is controlled by
  // the value of the lowest VideoStream.
  // TODO(bugs.webrtc.org/8630): The web specification describes being able to
  // control relative bitrate for each individual simulcast layer, but this
  // is currently just implemented per rtp sender.
  layers[0].bitrate_priority = bitrate_priority;
  return layers;
}

std::vector<webrtc::VideoStream> GetScreenshareLayers(
    size_t max_layers,
    int width,
    int height,
    double bitrate_priority,
    int max_qp,
    bool screenshare_simulcast_enabled,
    bool temporal_layers_supported) {
  auto max_screenshare_layers =
      screenshare_simulcast_enabled ? kMaxScreenshareSimulcastLayers : 1;
  size_t num_simulcast_layers =
      std::min<int>(max_layers, max_screenshare_layers);

  std::vector<webrtc::VideoStream> layers(num_simulcast_layers);
  // For legacy screenshare in conference mode, tl0 and tl1 bitrates are
  // piggybacked on the VideoCodec struct as target and max bitrates,
  // respectively. See eg. webrtc::LibvpxVp8Encoder::SetRates().
  layers[0].width = width;
  layers[0].height = height;
  layers[0].max_qp = max_qp;
  layers[0].max_framerate = 5;
  layers[0].min_bitrate_bps = kMinVideoBitrateBps;
  layers[0].target_bitrate_bps = kScreenshareDefaultTl0BitrateKbps * 1000;
  layers[0].max_bitrate_bps = kScreenshareDefaultTl1BitrateKbps * 1000;
  layers[0].num_temporal_layers = temporal_layers_supported ? 2 : 0;

  // With simulcast enabled, add another spatial layer. This one will have a
  // more normal layout, with the regular 3 temporal layer pattern and no fps
  // restrictions. The base simulcast layer will still use legacy setup.
  if (num_simulcast_layers == kMaxScreenshareSimulcastLayers) {
    // Add optional upper simulcast layer.
    const int num_temporal_layers = DefaultNumberOfTemporalLayers(1, true);
    int max_bitrate_bps;
    bool using_boosted_bitrate = false;
    if (!temporal_layers_supported) {
      // Set the max bitrate to where the base layer would have been if temporal
      // layers were enabled.
      max_bitrate_bps = static_cast<int>(
          kScreenshareHighStreamMaxBitrateBps *
          webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
              num_temporal_layers, 0));
    } else if (DefaultNumberOfTemporalLayers(1, true) != 3 ||
               webrtc::field_trial::IsEnabled(
                   kUseBaseHeavyVP8TL3RateAllocationFieldTrial)) {
      // Experimental temporal layer mode used, use increased max bitrate.
      max_bitrate_bps = kScreenshareHighStreamMaxBitrateBps;
      using_boosted_bitrate = true;
    } else {
      // Keep current bitrates with default 3tl/8 frame settings.
      // Lowest temporal layers of a 3 layer setup will have 40% of the total
      // bitrate allocation for that simulcast layer. Make sure the gap between
      // the target of the lower simulcast layer and first temporal layer of the
      // higher one is at most 2x the bitrate, so that upswitching is not
      // hampered by stalled bitrate estimates.
      max_bitrate_bps = 2 * ((layers[0].target_bitrate_bps * 10) / 4);
    }

    layers[1].width = width;
    layers[1].height = height;
    layers[1].max_qp = max_qp;
    layers[1].max_framerate = kDefaultVideoMaxFramerate;
    layers[1].num_temporal_layers =
        temporal_layers_supported ? DefaultNumberOfTemporalLayers(1, true) : 0;
    layers[1].min_bitrate_bps = using_boosted_bitrate
                                    ? kScreenshareHighStreamMinBitrateBps
                                    : layers[0].target_bitrate_bps * 2;

    // Cap max bitrate so it isn't overly high for the given resolution.
    int resolution_limited_bitrate = std::max(
        FindSimulcastMaxBitrateBps(width, height), layers[1].min_bitrate_bps);
    max_bitrate_bps =
        std::min<int>(max_bitrate_bps, resolution_limited_bitrate);

    layers[1].target_bitrate_bps = max_bitrate_bps;
    layers[1].max_bitrate_bps = max_bitrate_bps;
  }

  // The bitrate priority currently implemented on a per-sender level, so we
  // just set it for the first simulcast layer.
  layers[0].bitrate_priority = bitrate_priority;
  return layers;
}

bool ScreenshareSimulcastFieldTrialEnabled() {
  return webrtc::field_trial::IsEnabled(kSimulcastScreenshareFieldTrialName);
}

}  // namespace cricket
