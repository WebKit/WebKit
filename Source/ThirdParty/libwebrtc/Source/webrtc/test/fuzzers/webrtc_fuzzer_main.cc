/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file is intended to provide a common interface for fuzzing functions.
// It's intended to set sane defaults, such as removing logging for further
// fuzzing efficiency.

#include <cstddef>
#include <span>

#include "rtc_base/logging.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace {
bool g_initialized = false;
void InitializeWebRtcFuzzDefaults() {
  if (g_initialized)
    return;

// Remove default logging to prevent huge slowdowns.
// TODO(pbos): Disable in Chromium: http://crbug.com/561667
#if !defined(WEBRTC_CHROMIUM_BUILD)
  webrtc::LogMessage::LogToDebug(webrtc::LS_NONE);
#endif  // !defined(WEBRTC_CHROMIUM_BUILD)

  g_initialized = true;
}
}  // namespace

namespace webrtc {
extern void FuzzOneInput(FuzzDataHelper fuzz_data);
}  // namespace webrtc

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  InitializeWebRtcFuzzDefaults();
  webrtc::FuzzDataHelper fuzz_data(std::span(data, size));
  webrtc::FuzzOneInput(fuzz_data);
  return 0;
}
