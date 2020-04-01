/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "test/test_main_lib.h"

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace
  // TODO(crbug.com/1050976): Breaks iossim tests, re-enable when fixed.
  // absl::InitializeSymbolizer(argv[0]);

  // absl::FailureSignalHandlerOptions options;
  // absl::InstallFailureSignalHandler(options);

  std::unique_ptr<webrtc::TestMain> main = webrtc::TestMain::Create();
  int err_code = main->Init(&argc, argv);
  if (err_code != 0) {
    return err_code;
  }
  return main->Run(argc, argv);
}
