/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// #ifndef AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_FLAGS_H_
// #define AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_FLAGS_H_

#include "absl/flags/flag.h"

ABSL_FLAG(int,
          sample_rate_hz,
          16000,
          "Sample rate (Hz) of the produced audio files.");

ABSL_FLAG(bool,
          quick,
          false,
          "Don't do the full audio recording. "
          "Used to quickly check that the test runs without crashing.");

ABSL_FLAG(std::string, test_case_prefix, "", "Test case prefix.");

// #endif  // AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_FLAGS_H_
