/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_BEAMFORMER_MOCK_BEAMFORMER_H_
#define MODULES_AUDIO_PROCESSING_BEAMFORMER_MOCK_BEAMFORMER_H_

#include <vector>

#include "modules/audio_processing/beamformer/nonlinear_beamformer.h"
#include "test/gmock.h"

namespace webrtc {

class MockNonlinearBeamformer : public NonlinearBeamformer {
 public:
  MockNonlinearBeamformer(const std::vector<Point>& array_geometry,
                          size_t num_postfilter_channels)
      : NonlinearBeamformer(array_geometry, num_postfilter_channels) {}

  MockNonlinearBeamformer(const std::vector<Point>& array_geometry)
      : NonlinearBeamformer(array_geometry, 1u) {}

  MOCK_METHOD2(Initialize, void(int chunk_size_ms, int sample_rate_hz));
  MOCK_METHOD1(AnalyzeChunk, void(const ChannelBuffer<float>& data));
  MOCK_METHOD1(PostFilter, void(ChannelBuffer<float>* data));
  MOCK_METHOD1(IsInBeam, bool(const SphericalPointf& spherical_point));
  MOCK_METHOD0(is_target_present, bool());
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_BEAMFORMER_MOCK_BEAMFORMER_H_
