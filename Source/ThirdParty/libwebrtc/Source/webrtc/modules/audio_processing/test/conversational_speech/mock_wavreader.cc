/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/conversational_speech/mock_wavreader.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

using testing::Return;

MockWavReader::MockWavReader(int sample_rate,
                             size_t num_channels,
                             size_t num_samples)
    : sample_rate_(sample_rate),
      num_channels_(num_channels),
      num_samples_(num_samples) {
  ON_CALL(*this, SampleRate()).WillByDefault(Return(sample_rate_));
  ON_CALL(*this, NumChannels()).WillByDefault(Return(num_channels_));
  ON_CALL(*this, NumSamples()).WillByDefault(Return(num_samples_));
}

MockWavReader::~MockWavReader() = default;

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
