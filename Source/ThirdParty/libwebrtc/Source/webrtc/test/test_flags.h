/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TEST_FLAGS_H_
#define TEST_TEST_FLAGS_H_

#include <string>
#include <vector>

#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, force_fieldtrials);
ABSL_DECLARE_FLAG(std::vector<std::string>, plot);
ABSL_DECLARE_FLAG(std::string, isolated_script_test_perf_output);

#endif  // TEST_TEST_FLAGS_H_
