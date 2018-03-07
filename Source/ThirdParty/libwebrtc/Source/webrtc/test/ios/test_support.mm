/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <UIKit/UIKit.h>

#include "test/ios/test_support.h"

#import "sdk/objc/Framework/Classes/Common/RTCUIApplicationStatusObserver.h"

// Springboard will kill any iOS app that fails to check in after launch within
// a given time. Starting a UIApplication before invoking TestSuite::Run
// prevents this from happening.

// InitIOSRunHook saves the TestSuite and argc/argv, then invoking
// RunTestsFromIOSApp calls UIApplicationMain(), providing an application
// delegate class: WebRtcUnitTestDelegate. The delegate implements
// application:didFinishLaunchingWithOptions: to invoke the TestSuite's Run
// method.

// Since the executable isn't likely to be a real iOS UI, the delegate puts up a
// window displaying the app name. If a bunch of apps using MainHook are being
// run in a row, this provides an indication of which one is currently running.

static int (*g_test_suite)(void) = NULL;
static int g_argc;
static char **g_argv;

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

@interface WebRtcUnitTestDelegate : NSObject {
  UIWindow *_window;
}
- (void)runTests;
@end

@implementation WebRtcUnitTestDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  CGRect bounds = [[UIScreen mainScreen] bounds];

  _window = [[UIWindow alloc] initWithFrame:bounds];
  [_window setBackgroundColor:[UIColor whiteColor]];
  [_window makeKeyAndVisible];

  // Add a label with the app name.
  UILabel *label = [[UILabel alloc] initWithFrame:bounds];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = NSTextAlignmentCenter;
  [_window addSubview:label];

  // An NSInternalInconsistencyException is thrown if the app doesn't have a
  // root view controller. Set an empty one here.
  [_window setRootViewController:[[UIViewController alloc] init]];

  // We want to call `RTCUIApplicationStatusObserver sharedInstance` as early as
  // possible in the application lifecycle to set observation properly.
  __unused RTCUIApplicationStatusObserver *observer =
      [RTCUIApplicationStatusObserver sharedInstance];

  // Queue up the test run.
  [self performSelector:@selector(runTests) withObject:nil afterDelay:0.1];
  return YES;
}

- (void)runTests {
  int exitStatus = g_test_suite();

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  // TODO(crbug.com/137010): Figure out how much time is actually needed, and
  // sleep only to make sure that much time has elapsed since launch.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication *application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end
namespace rtc {
namespace test {

void InitTestSuite(int (*test_suite)(void), int argc, char *argv[]) {
  g_test_suite = test_suite;
  g_argc = argc;
  g_argv = argv;
}

void RunTestsFromIOSApp() {
  @autoreleasepool {
    exit(UIApplicationMain(g_argc, g_argv, nil, @"WebRtcUnitTestDelegate"));
  }
}
}  // namespace test
}  // namespace rtc
