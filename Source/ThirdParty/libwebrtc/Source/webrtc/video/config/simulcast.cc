/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/config/simulcast.h"

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "api/field_trials_view.h"
#include "api/video/video_codec_constants.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/min_video_bitrate_experiment.h"
#include "rtc_base/experiments/normalize_simulcast_size_experiment.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/logging.h"

namespace cricket {

namespace {

using ::webrtc::FieldTrialsView;

constexpr char kUseLegacySimulcastLayerLimitFieldTrial[] =
    "WebRTC-LegacySimulcastLayerLimit";

constexpr double kDefaultMaxRoundupRate = 0.1;

// Limits for legacy conference screensharing mode. Currently used for the
// lower of the two simulcast streams.
constexpr webrtc::DataRate kScreenshareDefaultTl0Bitrate =
    webrtc::DataRate::KilobitsPerSec(200);
constexpr webrtc::DataRate kScreenshareDefaultTl1Bitrate =
    webrtc::DataRate::KilobitsPerSec(1000);

// Min/max bitrate for the higher one of the two simulcast stream used for
// screen content.
constexpr webrtc::DataRate kScreenshareHighStreamMinBitrate =
    webrtc::DataRate::KilobitsPerSec(600);
constexpr webrtc::DataRate kScreenshareHighStreamMaxBitrate =
    webrtc::DataRate::KilobitsPerSec(1250);

constexpr int kDefaultNumTemporalLayers = 3;
constexpr int kScreenshareMaxSimulcastLayers = 2;
constexpr int kScreenshareTemporalLayers = 2;

struct SimulcastFormat {
  int width;
  int height;
  // The maximum number of simulcast layers can be used for
  // resolutions at `widthxheight` for legacy applications.
  size_t max_layers;
  // The maximum bitrate for encoding stream at `widthxheight`, when we are
  // not sending the next higher spatial stream.
  webrtc::DataRate max_bitrate;
  // The target bitrate for encoding stream at `widthxheight`, when this layer
  // is not the highest layer (i.e., when we are sending another higher spatial
  // stream).
  webrtc::DataRate target_bitrate;
  // The minimum bitrate needed for encoding stream at `widthxheight`.
  webrtc::DataRate min_bitrate;
};

// These tables describe from which resolution we can use how many
// simulcast layers at what bitrates (maximum, target, and minimum).
// Important!! Keep this table from high resolution to low resolution.
constexpr const SimulcastFormat kSimulcastFormatsVP8[] = {
    {1920, 1080, 3, webrtc::DataRate::KilobitsPerSec(5000),
     webrtc::DataRate::KilobitsPerSec(4000),
     webrtc::DataRate::KilobitsPerSec(800)},
    {1280, 720, 3, webrtc::DataRate::KilobitsPerSec(2500),
     webrtc::DataRate::KilobitsPerSec(2500),
     webrtc::DataRate::KilobitsPerSec(600)},
    {960, 540, 3, webrtc::DataRate::KilobitsPerSec(1200),
     webrtc::DataRate::KilobitsPerSec(1200),
     webrtc::DataRate::KilobitsPerSec(350)},
    {640, 360, 2, webrtc::DataRate::KilobitsPerSec(700),
     webrtc::DataRate::KilobitsPerSec(500),
     webrtc::DataRate::KilobitsPerSec(150)},
    {480, 270, 2, webrtc::DataRate::KilobitsPerSec(450),
     webrtc::DataRate::KilobitsPerSec(350),
     webrtc::DataRate::KilobitsPerSec(150)},
    {320, 180, 1, webrtc::DataRate::KilobitsPerSec(200),
     webrtc::DataRate::KilobitsPerSec(150),
     webrtc::DataRate::KilobitsPerSec(30)},
    // As the resolution goes down, interpolate the target and max bitrates down
    // towards zero. The min bitrate is still limited at 30 kbps and the target
    // and the max will be capped from below accordingly.
    {0, 0, 1, webrtc::DataRate::KilobitsPerSec(0),
     webrtc::DataRate::KilobitsPerSec(0),
     webrtc::DataRate::KilobitsPerSec(30)}};

// These tables describe from which resolution we can use how many
// simulcast layers at what bitrates (maximum, target, and minimum).
// Important!! Keep this table from high resolution to low resolution.
constexpr const SimulcastFormat kSimulcastFormatsVP9[] = {
    {1920, 1080, 3, webrtc::DataRate::KilobitsPerSec(3367),
     webrtc::DataRate::KilobitsPerSec(3367),
     webrtc::DataRate::KilobitsPerSec(769)},
    {1280, 720, 3, webrtc::DataRate::KilobitsPerSec(1524),
     webrtc::DataRate::KilobitsPerSec(1524),
     webrtc::DataRate::KilobitsPerSec(481)},
    {960, 540, 3, webrtc::DataRate::KilobitsPerSec(879),
     webrtc::DataRate::KilobitsPerSec(879),
     webrtc::DataRate::KilobitsPerSec(337)},
    {640, 360, 2, webrtc::DataRate::KilobitsPerSec(420),
     webrtc::DataRate::KilobitsPerSec(420),
     webrtc::DataRate::KilobitsPerSec(193)},
    {480, 270, 2, webrtc::DataRate::KilobitsPerSec(257),
     webrtc::DataRate::KilobitsPerSec(257),
     webrtc::DataRate::KilobitsPerSec(121)},
    {320, 180, 1, webrtc::DataRate::KilobitsPerSec(142),
     webrtc::DataRate::KilobitsPerSec(142),
     webrtc::DataRate::KilobitsPerSec(30)},
    {240, 135, 1, webrtc::DataRate::KilobitsPerSec(101),
     webrtc::DataRate::KilobitsPerSec(101),
     webrtc::DataRate::KilobitsPerSec(30)},
    // As the resolution goes down, interpolate the target and max bitrates down
    // towards zero. The min bitrate is still limited at 30 kbps and the target
    // and the max will be capped from below accordingly.
    {0, 0, 1, webrtc::DataRate::KilobitsPerSec(0),
     webrtc::DataRate::KilobitsPerSec(0),
     webrtc::DataRate::KilobitsPerSec(30)}};

constexpr webrtc::DataRate Interpolate(const webrtc::DataRate& a,
                                       const webrtc::DataRate& b,
                                       float rate) {
  return a * (1.0 - rate) + b * rate;
}

// TODO(webrtc:12415): Flip this to a kill switch when this feature launches.
bool EnableLowresBitrateInterpolation(const webrtc::FieldTrialsView& trials) {
  return absl::StartsWith(
      trials.Lookup("WebRTC-LowresSimulcastBitrateInterpolation"), "Enabled");
}

std::vector<SimulcastFormat> GetSimulcastFormats(
    bool enable_lowres_bitrate_interpolation,
    webrtc::VideoCodecType codec) {
  std::vector<SimulcastFormat> formats;
  switch (codec) {
    case webrtc::kVideoCodecVP8:
      formats.insert(formats.begin(), std::begin(kSimulcastFormatsVP8),
                     std::end(kSimulcastFormatsVP8));
      break;
    case webrtc::kVideoCodecVP9:
      formats.insert(formats.begin(), std::begin(kSimulcastFormatsVP9),
                     std::end(kSimulcastFormatsVP9));
      break;
    default:
      formats.insert(formats.begin(), std::begin(kSimulcastFormatsVP8),
                     std::end(kSimulcastFormatsVP8));
      break;
  }
  if (!enable_lowres_bitrate_interpolation) {
    RTC_CHECK_GE(formats.size(), 2u);
    SimulcastFormat& format0x0 = formats[formats.size() - 1];
    const SimulcastFormat& format_prev = formats[formats.size() - 2];
    format0x0.max_bitrate = format_prev.max_bitrate;
    format0x0.target_bitrate = format_prev.target_bitrate;
    format0x0.min_bitrate = format_prev.min_bitrate;
  }
  return formats;
}

int FindSimulcastFormatIndex(int width,
                             int height,
                             bool enable_lowres_bitrate_interpolation,
                             webrtc::VideoCodecType codec) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  const auto formats =
      GetSimulcastFormats(enable_lowres_bitrate_interpolation, codec);
  for (uint32_t i = 0; i < formats.size(); ++i) {
    if (width * height >= formats[i].width * formats[i].height) {
      return i;
    }
  }
  RTC_DCHECK_NOTREACHED();
  return -1;
}

SimulcastFormat InterpolateSimulcastFormat(
    int width,
    int height,
    std::optional<double> max_roundup_rate,
    bool enable_lowres_bitrate_interpolation,
    webrtc::VideoCodecType codec) {
  const auto formats =
      GetSimulcastFormats(enable_lowres_bitrate_interpolation, codec);
  const int index = FindSimulcastFormatIndex(
      width, height, enable_lowres_bitrate_interpolation, codec);
  if (index == 0)
    return formats[index];
  const int total_pixels_up =
      formats[index - 1].width * formats[index - 1].height;
  const int total_pixels_down = formats[index].width * formats[index].height;
  const int total_pixels = width * height;
  const float rate = (total_pixels_up - total_pixels) /
                     static_cast<float>(total_pixels_up - total_pixels_down);

  // Use upper resolution if `rate` is below the configured threshold.
  size_t max_layers = (rate < max_roundup_rate.value_or(kDefaultMaxRoundupRate))
                          ? formats[index - 1].max_layers
                          : formats[index].max_layers;
  webrtc::DataRate max_bitrate = Interpolate(formats[index - 1].max_bitrate,
                                             formats[index].max_bitrate, rate);
  webrtc::DataRate target_bitrate = Interpolate(
      formats[index - 1].target_bitrate, formats[index].target_bitrate, rate);
  webrtc::DataRate min_bitrate = Interpolate(formats[index - 1].min_bitrate,
                                             formats[index].min_bitrate, rate);

  return {width, height, max_layers, max_bitrate, target_bitrate, min_bitrate};
}

std::vector<webrtc::VideoStream> GetNormalSimulcastLayers(
    rtc::ArrayView<const webrtc::Resolution> resolutions,
    bool temporal_layers_supported,
    bool base_heavy_tl3_rate_alloc,
    const webrtc::FieldTrialsView& trials,
    webrtc::VideoCodecType codec) {
  const bool enable_lowres_bitrate_interpolation =
      EnableLowresBitrateInterpolation(trials);
  const int num_temporal_layers =
      temporal_layers_supported ? kDefaultNumTemporalLayers : 1;
  // Add simulcast streams, from highest resolution (`s` = num_simulcast_layers
  // -1) to lowest resolution at `s` = 0.
  std::vector<webrtc::VideoStream> layers(resolutions.size());
  for (size_t s = 0; s < resolutions.size(); ++s) {
    layers[s].width = resolutions[s].width;
    layers[s].height = resolutions[s].height;
    layers[s].num_temporal_layers = num_temporal_layers;

    SimulcastFormat interpolated_format = InterpolateSimulcastFormat(
        layers[s].width, layers[s].height, /*max_roundup_rate=*/std::nullopt,
        enable_lowres_bitrate_interpolation, codec);

    layers[s].max_bitrate_bps = interpolated_format.max_bitrate.bps();
    layers[s].target_bitrate_bps = interpolated_format.target_bitrate.bps();
    layers[s].min_bitrate_bps = interpolated_format.min_bitrate.bps();

    if (s == 0) {
      // If alternative temporal rate allocation is selected, adjust the
      // bitrate of the lowest simulcast stream so that absolute bitrate for
      // the base temporal layer matches the bitrate for the base temporal
      // layer with the default 3 simulcast streams. Otherwise we risk a
      // higher threshold for receiving a feed at all.
      float rate_factor = 1.0;
      if (num_temporal_layers == 3) {
        if (base_heavy_tl3_rate_alloc) {
          // Base heavy allocation increases TL0 bitrate from 40% to 60%.
          rate_factor = 0.4 / 0.6;
        }
      } else {
        rate_factor =
            webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                3, 0, /*base_heavy_tl3_rate_alloc=*/false) /
            webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
                num_temporal_layers, 0, /*base_heavy_tl3_rate_alloc=*/false);
      }

      layers[s].max_bitrate_bps =
          static_cast<int>(layers[s].max_bitrate_bps * rate_factor);
      layers[s].target_bitrate_bps =
          static_cast<int>(layers[s].target_bitrate_bps * rate_factor);
    }

    // Ensure consistency.
    layers[s].max_bitrate_bps =
        std::max(layers[s].min_bitrate_bps, layers[s].max_bitrate_bps);
    layers[s].target_bitrate_bps =
        std::max(layers[s].min_bitrate_bps, layers[s].target_bitrate_bps);

    layers[s].max_framerate = kDefaultVideoMaxFramerate;
  }

  return layers;
}

std::vector<webrtc::VideoStream> GetScreenshareLayers(
    size_t max_layers,
    int width,
    int height,
    bool temporal_layers_supported,
    bool base_heavy_tl3_rate_alloc,
    const webrtc::FieldTrialsView& trials) {
  size_t num_simulcast_layers =
      std::min<int>(max_layers, kScreenshareMaxSimulcastLayers);

  std::vector<webrtc::VideoStream> layers(num_simulcast_layers);
  // For legacy screenshare in conference mode, tl0 and tl1 bitrates are
  // piggybacked on the VideoCodec struct as target and max bitrates,
  // respectively. See eg. webrtc::LibvpxVp8Encoder::SetRates().
  layers[0].width = width;
  layers[0].height = height;
  layers[0].max_framerate = 5;
  layers[0].min_bitrate_bps = webrtc::kDefaultMinVideoBitrateBps;
  layers[0].target_bitrate_bps = kScreenshareDefaultTl0Bitrate.bps();
  layers[0].max_bitrate_bps = kScreenshareDefaultTl1Bitrate.bps();
  layers[0].num_temporal_layers = temporal_layers_supported ? 2 : 1;

  // With simulcast enabled, add another spatial layer. This one will have a
  // more normal layout, with the regular 3 temporal layer pattern and no fps
  // restrictions. The base simulcast layer will still use legacy setup.
  if (num_simulcast_layers == kScreenshareMaxSimulcastLayers) {
    // Add optional upper simulcast layer.
    int max_bitrate_bps;
    bool using_boosted_bitrate = false;
    if (!temporal_layers_supported) {
      // Set the max bitrate to where the base layer would have been if temporal
      // layers were enabled.
      max_bitrate_bps = static_cast<int>(
          kScreenshareHighStreamMaxBitrate.bps() *
          webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
              kScreenshareTemporalLayers, 0, base_heavy_tl3_rate_alloc));
    } else {
      // Experimental temporal layer mode used, use increased max bitrate.
      max_bitrate_bps = kScreenshareHighStreamMaxBitrate.bps();
      using_boosted_bitrate = true;
    }

    layers[1].width = width;
    layers[1].height = height;
    layers[1].max_framerate = kDefaultVideoMaxFramerate;
    layers[1].num_temporal_layers =
        temporal_layers_supported ? kScreenshareTemporalLayers : 1;
    layers[1].min_bitrate_bps = using_boosted_bitrate
                                    ? kScreenshareHighStreamMinBitrate.bps()
                                    : layers[0].target_bitrate_bps * 2;
    layers[1].target_bitrate_bps = max_bitrate_bps;
    layers[1].max_bitrate_bps = max_bitrate_bps;
  }

  return layers;
}

}  // namespace

size_t LimitSimulcastLayerCount(size_t min_num_layers,
                                size_t max_num_layers,
                                int width,
                                int height,
                                const webrtc::FieldTrialsView& trials,
                                webrtc::VideoCodecType codec) {
  if (!absl::StartsWith(trials.Lookup(kUseLegacySimulcastLayerLimitFieldTrial),
                        "Disabled")) {
    // Max layers from one higher resolution in kSimulcastFormats will be used
    // if the ratio (pixels_up - pixels) / (pixels_up - pixels_down) is less
    // than configured `max_ratio`. pixels_down is the selected index in
    // kSimulcastFormats based on pixels.
    webrtc::FieldTrialOptional<double> max_ratio("max_ratio");
    webrtc::ParseFieldTrial({&max_ratio},
                            trials.Lookup("WebRTC-SimulcastLayerLimitRoundUp"));

    size_t reduced_num_layers =
        std::max(min_num_layers,
                 InterpolateSimulcastFormat(
                     width, height, max_ratio.GetOptional(),
                     /*enable_lowres_bitrate_interpolation=*/false, codec)
                     .max_layers);
    if (max_num_layers > reduced_num_layers) {
      RTC_LOG(LS_WARNING) << "Reducing simulcast layer count from "
                          << max_num_layers << " to " << reduced_num_layers;
      return reduced_num_layers;
    }
  }
  return max_num_layers;
}

void BoostMaxSimulcastLayer(webrtc::DataRate max_bitrate,
                            std::vector<webrtc::VideoStream>* layers) {
  if (layers->empty())
    return;

  const webrtc::DataRate total_bitrate = GetTotalMaxBitrate(*layers);

  // We're still not using all available bits.
  if (total_bitrate < max_bitrate) {
    // Spend additional bits to boost the max layer.
    const webrtc::DataRate bitrate_left = max_bitrate - total_bitrate;
    layers->back().max_bitrate_bps += bitrate_left.bps();
  }
}

webrtc::DataRate GetTotalMaxBitrate(
    const std::vector<webrtc::VideoStream>& layers) {
  if (layers.empty())
    return webrtc::DataRate::Zero();

  int total_max_bitrate_bps = 0;
  for (size_t s = 0; s < layers.size() - 1; ++s) {
    total_max_bitrate_bps += layers[s].target_bitrate_bps;
  }
  total_max_bitrate_bps += layers.back().max_bitrate_bps;
  return webrtc::DataRate::BitsPerSec(total_max_bitrate_bps);
}

std::vector<webrtc::VideoStream> GetSimulcastConfig(
    rtc::ArrayView<const webrtc::Resolution> resolutions,
    bool is_screenshare_with_conference_mode,
    bool temporal_layers_supported,
    const webrtc::FieldTrialsView& trials,
    webrtc::VideoCodecType codec) {
  RTC_DCHECK(!resolutions.empty());

  const bool base_heavy_tl3_rate_alloc =
      webrtc::RateControlSettings(trials).Vp8BaseHeavyTl3RateAllocation();
  if (is_screenshare_with_conference_mode) {
    return GetScreenshareLayers(
        resolutions.size(), resolutions[0].width, resolutions[0].height,
        temporal_layers_supported, base_heavy_tl3_rate_alloc, trials);
  } else {
    return GetNormalSimulcastLayers(resolutions, temporal_layers_supported,
                                    base_heavy_tl3_rate_alloc, trials, codec);
  }
}

}  // namespace cricket
