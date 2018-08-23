/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_SIMULCAST_UTILITY_H_
#define MODULES_VIDEO_CODING_UTILITY_SIMULCAST_UTILITY_H_

#include "api/video_codecs/video_encoder.h"

namespace webrtc {

class SimulcastUtility {
 public:
  static uint32_t SumStreamMaxBitrate(int streams, const VideoCodec& codec);
  static int NumberOfSimulcastStreams(const VideoCodec& codec);
  static bool ValidSimulcastResolutions(const VideoCodec& codec,
                                        int num_streams);
  static bool ValidSimulcastTemporalLayers(const VideoCodec& codec,
                                           int num_streams);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_SIMULCAST_UTILITY_H_
