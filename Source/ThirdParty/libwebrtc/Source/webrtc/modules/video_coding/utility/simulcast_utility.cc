/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "modules/video_coding/utility/simulcast_utility.h"

namespace webrtc {

uint32_t SimulcastUtility::SumStreamMaxBitrate(int streams,
                                               const VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].maxBitrate;
  }
  return bitrate_sum;
}

int SimulcastUtility::NumberOfSimulcastStreams(const VideoCodec& codec) {
  int streams =
      codec.numberOfSimulcastStreams < 1 ? 1 : codec.numberOfSimulcastStreams;
  uint32_t simulcast_max_bitrate = SumStreamMaxBitrate(streams, codec);
  if (simulcast_max_bitrate == 0) {
    streams = 1;
  }
  return streams;
}

bool SimulcastUtility::ValidSimulcastResolutions(const VideoCodec& codec,
                                                 int num_streams) {
  if (codec.width != codec.simulcastStream[num_streams - 1].width ||
      codec.height != codec.simulcastStream[num_streams - 1].height) {
    return false;
  }
  for (int i = 0; i < num_streams; ++i) {
    if (codec.width * codec.simulcastStream[i].height !=
        codec.height * codec.simulcastStream[i].width) {
      return false;
    }
  }
  for (int i = 1; i < num_streams; ++i) {
    if (codec.simulcastStream[i].width !=
        codec.simulcastStream[i - 1].width * 2) {
      return false;
    }
  }
  return true;
}

bool SimulcastUtility::ValidSimulcastTemporalLayers(const VideoCodec& codec,
                                                    int num_streams) {
  for (int i = 0; i < num_streams - 1; ++i) {
    if (codec.simulcastStream[i].numberOfTemporalLayers !=
        codec.simulcastStream[i + 1].numberOfTemporalLayers)
      return false;
  }
  return true;
}

bool SimulcastUtility::IsConferenceModeScreenshare(const VideoCodec& codec) {
  if (codec.mode != VideoCodecMode::kScreensharing ||
      NumberOfTemporalLayers(codec, 0) != 2) {
    return false;
  }
  // Fixed default bitrates for legacy screenshare layers mode.
  return (codec.numberOfSimulcastStreams == 0 && codec.maxBitrate == 1000) ||
         (codec.numberOfSimulcastStreams >= 1 &&
          codec.simulcastStream[0].maxBitrate == 1000 &&
          codec.simulcastStream[0].targetBitrate == 200);
}

int SimulcastUtility::NumberOfTemporalLayers(const VideoCodec& codec,
                                             int spatial_id) {
  uint8_t num_temporal_layers =
      std::max<uint8_t>(1, codec.VP8().numberOfTemporalLayers);
  if (codec.numberOfSimulcastStreams > 0) {
    RTC_DCHECK_LT(spatial_id, codec.numberOfSimulcastStreams);
    num_temporal_layers =
        std::max(num_temporal_layers,
                 codec.simulcastStream[spatial_id].numberOfTemporalLayers);
  }
  return num_temporal_layers;
}

}  // namespace webrtc
