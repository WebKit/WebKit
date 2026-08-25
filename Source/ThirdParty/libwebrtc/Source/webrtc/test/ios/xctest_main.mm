/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "rtc_base/thread.h"
#include "test/ios/coverage_util_ios.h"
#include "test/run_loop.h"

// Shared entry point for iOS XCTest bundles. Unlike test/test_main.cc, which
// drives gtest via RUN_ALL_TESTS, XCTest targets are launched by the XCTest
// harness, so main() only needs to spin up the host application.
int main(int argc, char* argv[]) {
  webrtc::test::ConfigureCoverageReportPath();

  webrtc::test::RunLoop main_thread;

  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, nil);
  }
}
