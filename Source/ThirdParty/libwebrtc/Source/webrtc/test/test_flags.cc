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

ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

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

ABSL_FLAG(std::string,
          webrtc_test_metrics_output_path,
          "",
          "Path where the test perf metrics should be stored using "
          "api/test/metrics/metric.proto proto format. File will contain "
          "MetricsSet as a root proto. On iOS, this MUST be a file name "
          "and the file will be stored under NSDocumentDirectory.");

ABSL_FLAG(bool,
          export_perf_results_new_api,
          false,
          "Tells to initialize new API for exporting performance metrics");

ABSL_FLAG(bool,
          webrtc_quick_perf_test,
          false,
          "Runs webrtc perfomance tests in quick mode.");
