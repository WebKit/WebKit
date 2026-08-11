/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
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

int main(int argc, char* argv[]) {
  rtc::test::ConfigureCoverageReportPath();

  rtc::AutoThread main_thread;

  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, nil);
  }
}
