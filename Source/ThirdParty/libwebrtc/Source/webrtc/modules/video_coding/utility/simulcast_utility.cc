/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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

}  // namespace webrtc
