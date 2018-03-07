/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Cocoa/Cocoa.h>

#include "test/run_test.h"

// Converting a C++ function pointer to an Objective-C block.
typedef void(^TestBlock)();
TestBlock functionToBlock(void(*function)()) {
  return [^(void) { function(); } copy];
}

// Class calling the test function on the platform specific thread.
@interface TestRunner : NSObject {
  BOOL running_;
}
- (void)runAllTests:(TestBlock)ignored;
- (BOOL)running;
@end

@implementation TestRunner
- (id)init {
  self = [super init];
  if (self) {
    running_ = YES;
  }
  return self;
}

- (void)runAllTests:(TestBlock)testBlock {
  @autoreleasepool {
    testBlock();
    running_ = NO;
  }
}

- (BOOL)running {
  return running_;
}
@end

namespace webrtc {
namespace test {

void RunTest(void(*test)()) {
  @autoreleasepool {
    [NSApplication sharedApplication];

    // Convert the function pointer to an Objective-C block and call on a
    // separate thread, to avoid blocking the main thread.
    TestRunner *testRunner = [[TestRunner alloc] init];
    TestBlock testBlock = functionToBlock(test);
    [NSThread detachNewThreadSelector:@selector(runAllTests:)
                             toTarget:testRunner
                           withObject:testBlock];

    NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
    while ([testRunner running] && [runLoop runMode:NSDefaultRunLoopMode
                                         beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]])
      ;
  }
}

}  // namespace test
}  // namespace webrtc
