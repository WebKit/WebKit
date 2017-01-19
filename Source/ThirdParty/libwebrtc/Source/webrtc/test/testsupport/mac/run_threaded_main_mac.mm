/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "webrtc/test/testsupport/mac/run_threaded_main_mac.h"

#import <Cocoa/Cocoa.h>

// This class passes parameter from main to the worked thread and back.
@interface AutoTestInWorkerThread : NSObject {
  int    argc_;
  char** argv_;
  int    result_;
  bool   done_;
}

- (void)setDone:(bool)done;
- (bool)done;
- (void)setArgc:(int)argc argv:(char**)argv;
- (int) result;
- (void)runTest:(NSObject*)ignored;

@end

@implementation AutoTestInWorkerThread

- (void)setDone:(bool)done {
  done_ = done;
}

- (bool)done {
  return done_;
}

- (void)setArgc:(int)argc argv:(char**)argv {
  argc_ = argc;
  argv_ = argv;
}

- (void)runTest:(NSObject*)ignored {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    result_ = ImplementThisToRunYourTest(argc_, argv_);
    done_ = true;

    [pool release];
    return;
}

- (int)result {
  return result_;
}

@end

int main(int argc, char * argv[]) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [NSApplication sharedApplication];

    int result = 0;
    AutoTestInWorkerThread* tests = [[AutoTestInWorkerThread alloc] init];

    [tests setArgc:argc argv:argv];
    [tests setDone:false];

    [NSThread detachNewThreadSelector:@selector(runTest:)
                             toTarget:tests
                           withObject:nil];

    NSRunLoop* main_run_loop = [NSRunLoop mainRunLoop];
    NSDate *loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
    bool runloop_ok = true;
    while (![tests done] && runloop_ok) {
      runloop_ok = [main_run_loop runMode:NSDefaultRunLoopMode
                               beforeDate:loop_until];
      loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
    }

    result = [tests result];

    [pool release];
    return result;
}
