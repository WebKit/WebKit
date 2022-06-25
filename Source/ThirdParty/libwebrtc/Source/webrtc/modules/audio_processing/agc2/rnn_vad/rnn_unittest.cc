/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/rnn.h"

#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr std::array<float, kFeatureVectorSize> kFeatures = {
    -1.00131f,   -0.627069f, -7.81097f,  7.86285f,    -2.87145f,  3.32365f,
    -0.653161f,  0.529839f,  -0.425307f, 0.25583f,    0.235094f,  0.230527f,
    -0.144687f,  0.182785f,  0.57102f,   0.125039f,   0.479482f,  -0.0255439f,
    -0.0073141f, -0.147346f, -0.217106f, -0.0846906f, -8.34943f,  3.09065f,
    1.42628f,    -0.85235f,  -0.220207f, -0.811163f,  2.09032f,   -2.01425f,
    -0.690268f,  -0.925327f, -0.541354f, 0.58455f,    -0.606726f, -0.0372358f,
    0.565991f,   0.435854f,  0.420812f,  0.162198f,   -2.13f,     10.0089f};

void WarmUpRnnVad(RnnVad& rnn_vad) {
  for (int i = 0; i < 10; ++i) {
    rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/false);
  }
}

// Checks that the speech probability is zero with silence.
TEST(RnnVadTest, CheckZeroProbabilityWithSilence) {
  RnnVad rnn_vad(GetAvailableCpuFeatures());
  WarmUpRnnVad(rnn_vad);
  EXPECT_EQ(rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/true), 0.f);
}

// Checks that the same output is produced after reset given the same input
// sequence.
TEST(RnnVadTest, CheckRnnVadReset) {
  RnnVad rnn_vad(GetAvailableCpuFeatures());
  WarmUpRnnVad(rnn_vad);
  float pre = rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/false);
  rnn_vad.Reset();
  WarmUpRnnVad(rnn_vad);
  float post = rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/false);
  EXPECT_EQ(pre, post);
}

// Checks that the same output is produced after silence is observed given the
// same input sequence.
TEST(RnnVadTest, CheckRnnVadSilence) {
  RnnVad rnn_vad(GetAvailableCpuFeatures());
  WarmUpRnnVad(rnn_vad);
  float pre = rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/false);
  rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/true);
  WarmUpRnnVad(rnn_vad);
  float post = rnn_vad.ComputeVadProbability(kFeatures, /*is_silence=*/false);
  EXPECT_EQ(pre, post);
}

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
