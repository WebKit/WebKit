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

#include "webrtc/base/arraysize.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/streamparams.h"
#include "webrtc/media/engine/constants.h"
#include "webrtc/media/engine/simulcast.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace cricket {

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
const SimulcastFormat kSimulcastFormats[] = {
  {1920, 1080, 3, 5000, 4000, 800},
  {1280, 720, 3,  2500, 2500, 600},
  {960, 540, 3, 900, 900, 450},
  {640, 360, 2, 700, 500, 150},
  {480, 270, 2, 450, 350, 150},
  {320, 180, 1, 200, 150, 30},
  {0, 0, 1, 200, 150, 30}
};

const int kDefaultScreenshareSimulcastStreams = 2;

// Multiway: Number of temporal layers for each simulcast stream, for maximum
// possible number of simulcast streams |kMaxSimulcastStreams|. The array
// goes from lowest resolution at position 0 to highest resolution.
// For example, first three elements correspond to say: QVGA, VGA, WHD.
static const int
    kDefaultConferenceNumberOfTemporalLayers[webrtc::kMaxSimulcastStreams] =
    {3, 3, 3, 3};

void GetSimulcastSsrcs(const StreamParams& sp, std::vector<uint32_t>* ssrcs) {
  const SsrcGroup* sim_group = sp.get_ssrc_group(kSimSsrcGroupSemantics);
  if (sim_group) {
    ssrcs->insert(
        ssrcs->end(), sim_group->ssrcs.begin(), sim_group->ssrcs.end());
  }
}

void MaybeExchangeWidthHeight(int* width, int* height) {
  // |kSimulcastFormats| assumes |width| >= |height|. If not, exchange them
  // before comparing.
  if (*width < *height) {
    int temp = *width;
    *width = *height;
    *height = temp;
  }
}

int FindSimulcastFormatIndex(int width, int height) {
  MaybeExchangeWidthHeight(&width, &height);

  for (uint32_t i = 0; i < arraysize(kSimulcastFormats); ++i) {
    if (width * height >=
        kSimulcastFormats[i].width * kSimulcastFormats[i].height) {
      return i;
    }
  }
  return -1;
}

int FindSimulcastFormatIndex(int width, int height, size_t max_layers) {
  MaybeExchangeWidthHeight(&width, &height);

  for (uint32_t i = 0; i < arraysize(kSimulcastFormats); ++i) {
    if (width * height >=
            kSimulcastFormats[i].width * kSimulcastFormats[i].height &&
        max_layers == kSimulcastFormats[i].max_layers) {
      return i;
    }
  }
  return -1;
}

// Simulcast stream width and height must both be dividable by
// |2 ^ simulcast_layers - 1|.
int NormalizeSimulcastSize(int size, size_t simulcast_layers) {
  const int base2_exponent = static_cast<int>(simulcast_layers) - 1;
  return ((size >> base2_exponent) << base2_exponent);
}

size_t FindSimulcastMaxLayers(int width, int height) {
  int index = FindSimulcastFormatIndex(width, height);
  if (index == -1) {
    return -1;
  }
  return kSimulcastFormats[index].max_layers;
}

// TODO(marpan): Investigate if we should return 0 instead of -1 in
// FindSimulcast[Max/Target/Min]Bitrate functions below, since the
// codec struct max/min/targeBitrates are unsigned.
int FindSimulcastMaxBitrateBps(int width, int height) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  if (format_index == -1) {
    return -1;
  }
  return kSimulcastFormats[format_index].max_bitrate_kbps * 1000;
}

int FindSimulcastTargetBitrateBps(int width, int height) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  if (format_index == -1) {
    return -1;
  }
  return kSimulcastFormats[format_index].target_bitrate_kbps * 1000;
}

int FindSimulcastMinBitrateBps(int width, int height) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  if (format_index == -1) {
    return -1;
  }
  return kSimulcastFormats[format_index].min_bitrate_kbps * 1000;
}

bool SlotSimulcastMaxResolution(size_t max_layers, int* width, int* height) {
  int index = FindSimulcastFormatIndex(*width, *height, max_layers);
  if (index == -1) {
    LOG(LS_ERROR) << "SlotSimulcastMaxResolution";
    return false;
  }

  *width = kSimulcastFormats[index].width;
  *height = kSimulcastFormats[index].height;
  LOG(LS_INFO) << "SlotSimulcastMaxResolution to width:" << *width
               << " height:" << *height;
  return true;
}

int GetTotalMaxBitrateBps(const std::vector<webrtc::VideoStream>& streams) {
  int total_max_bitrate_bps = 0;
  for (size_t s = 0; s < streams.size() - 1; ++s) {
    total_max_bitrate_bps += streams[s].target_bitrate_bps;
  }
  total_max_bitrate_bps += streams.back().max_bitrate_bps;
  return total_max_bitrate_bps;
}

std::vector<webrtc::VideoStream> GetSimulcastConfig(size_t max_streams,
                                                    int width,
                                                    int height,
                                                    int max_bitrate_bps,
                                                    int max_qp,
                                                    int max_framerate,
                                                    bool is_screencast) {
  size_t num_simulcast_layers;
  if (is_screencast) {
    num_simulcast_layers =
        UseSimulcastScreenshare() ? kDefaultScreenshareSimulcastStreams : 1;
  } else {
    num_simulcast_layers = FindSimulcastMaxLayers(width, height);
  }

  if (num_simulcast_layers > max_streams) {
    // If the number of SSRCs in the group differs from our target
    // number of simulcast streams for current resolution, switch down
    // to a resolution that matches our number of SSRCs.
    if (!SlotSimulcastMaxResolution(max_streams, &width, &height)) {
      return std::vector<webrtc::VideoStream>();
    }
    num_simulcast_layers = max_streams;
  }
  std::vector<webrtc::VideoStream> streams;
  streams.resize(num_simulcast_layers);

  if (!is_screencast) {
    // Format width and height has to be divisible by |2 ^ number_streams - 1|.
    width = NormalizeSimulcastSize(width, num_simulcast_layers);
    height = NormalizeSimulcastSize(height, num_simulcast_layers);
  }

  // Add simulcast sub-streams from lower resolution to higher resolutions.
  // Add simulcast streams, from highest resolution (|s| = number_streams -1)
  // to lowest resolution at |s| = 0.
  for (size_t s = num_simulcast_layers - 1;; --s) {
    streams[s].width = width;
    streams[s].height = height;
    // TODO(pbos): Fill actual temporal-layer bitrate thresholds.
    streams[s].max_qp = max_qp;
    if (is_screencast && s == 0) {
      ScreenshareLayerConfig config = ScreenshareLayerConfig::GetDefault();
      // For legacy screenshare in conference mode, tl0 and tl1 bitrates are
      // piggybacked on the VideoCodec struct as target and max bitrates,
      // respectively. See eg. webrtc::VP8EncoderImpl::SetRates().
      streams[s].min_bitrate_bps = kMinVideoBitrateKbps * 1000;
      streams[s].target_bitrate_bps = config.tl0_bitrate_kbps * 1000;
      streams[s].max_bitrate_bps = config.tl1_bitrate_kbps * 1000;
      streams[s].temporal_layer_thresholds_bps.clear();
      streams[s].temporal_layer_thresholds_bps.push_back(
          config.tl0_bitrate_kbps * 1000);
      streams[s].max_framerate = 5;
    } else {
      streams[s].temporal_layer_thresholds_bps.resize(
          kDefaultConferenceNumberOfTemporalLayers[s] - 1);
      streams[s].max_bitrate_bps = FindSimulcastMaxBitrateBps(width, height);
      streams[s].target_bitrate_bps =
          FindSimulcastTargetBitrateBps(width, height);
      streams[s].min_bitrate_bps = FindSimulcastMinBitrateBps(width, height);
      streams[s].max_framerate = max_framerate;
    }

    if (!is_screencast) {
      width /= 2;
      height /= 2;
    }
    if (s == 0)
      break;
  }

  // Spend additional bits to boost the max stream.
  int bitrate_left_bps = max_bitrate_bps - GetTotalMaxBitrateBps(streams);
  if (bitrate_left_bps > 0) {
    streams.back().max_bitrate_bps += bitrate_left_bps;
  }

  return streams;
}

static const int kScreenshareMinBitrateKbps = 50;
static const int kScreenshareMaxBitrateKbps = 6000;
static const int kScreenshareDefaultTl0BitrateKbps = 200;
static const int kScreenshareDefaultTl1BitrateKbps = 1000;

static const char* kScreencastLayerFieldTrialName =
    "WebRTC-ScreenshareLayerRates";
static const char* kSimulcastScreenshareFieldTrialName =
    "WebRTC-SimulcastScreenshare";

ScreenshareLayerConfig::ScreenshareLayerConfig(int tl0_bitrate, int tl1_bitrate)
    : tl0_bitrate_kbps(tl0_bitrate), tl1_bitrate_kbps(tl1_bitrate) {
}

ScreenshareLayerConfig ScreenshareLayerConfig::GetDefault() {
  std::string group =
      webrtc::field_trial::FindFullName(kScreencastLayerFieldTrialName);

  ScreenshareLayerConfig config(kScreenshareDefaultTl0BitrateKbps,
                                kScreenshareDefaultTl1BitrateKbps);
  if (!group.empty() && !FromFieldTrialGroup(group, &config)) {
    LOG(LS_WARNING) << "Unable to parse WebRTC-ScreenshareLayerRates"
                       " field trial group: '" << group << "'.";
  }
  return config;
}

bool ScreenshareLayerConfig::FromFieldTrialGroup(
    const std::string& group,
    ScreenshareLayerConfig* config) {
  // Parse field trial group name, containing bitrates for tl0 and tl1.
  int tl0_bitrate;
  int tl1_bitrate;
  if (sscanf(group.c_str(), "%d-%d", &tl0_bitrate, &tl1_bitrate) != 2) {
    return false;
  }

  // Sanity check.
  if (tl0_bitrate < kScreenshareMinBitrateKbps ||
      tl0_bitrate > kScreenshareMaxBitrateKbps ||
      tl1_bitrate < kScreenshareMinBitrateKbps ||
      tl1_bitrate > kScreenshareMaxBitrateKbps || tl0_bitrate > tl1_bitrate) {
    return false;
  }

  config->tl0_bitrate_kbps = tl0_bitrate;
  config->tl1_bitrate_kbps = tl1_bitrate;

  return true;
}

bool UseSimulcastScreenshare() {
  return webrtc::field_trial::IsEnabled(kSimulcastScreenshareFieldTrialName);
}

}  // namespace cricket
