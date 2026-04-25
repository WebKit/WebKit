/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/test_flags.h"

#include <string>
#include <vector>

#include "absl/flags/flag.h"

ABSL_FLAG(std::vector<std::string>,
          plot,
          {},
          "List of metrics that should be exported for plotting (if they are "
          "available). Example: psnr,ssim,encode_time. To plot all available "
          " metrics pass 'all' as flag value");

ABSL_FLAG(
    std::string,
    isolated_script_test_perf_output,
    "",
    "Path where the perf results should be stored in proto format described "
    "described by histogram.proto in "
    "https://chromium.googlesource.com/catapult/.");

ABSL_FLAG(bool,
          webrtc_quick_perf_test,
          false,
          "Runs webrtc perfomance tests in quick mode.");
