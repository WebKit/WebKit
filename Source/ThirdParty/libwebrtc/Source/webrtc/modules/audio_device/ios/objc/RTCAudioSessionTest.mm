/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include "webrtc/test/gtest.h"

#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession+Private.h"

@interface RTCAudioSessionTestDelegate : NSObject <RTCAudioSessionDelegate>
@end

@implementation RTCAudioSessionTestDelegate

- (void)audioSessionDidBeginInterruption:(RTCAudioSession *)session {
}

- (void)audioSessionDidEndInterruption:(RTCAudioSession *)session
                   shouldResumeSession:(BOOL)shouldResumeSession {
}

- (void)audioSessionDidChangeRoute:(RTCAudioSession *)session
           reason:(AVAudioSessionRouteChangeReason)reason
    previousRoute:(AVAudioSessionRouteDescription *)previousRoute {
}

- (void)audioSessionMediaServicesWereLost:(RTCAudioSession *)session {
}

- (void)audioSessionMediaServicesWereReset:(RTCAudioSession *)session {
}

- (void)audioSessionShouldConfigure:(RTCAudioSession *)session {
}

- (void)audioSessionShouldUnconfigure:(RTCAudioSession *)session {
}

@end

// A delegate that adds itself to the audio session on init and removes itself
// in its dealloc.
@interface RTCTestRemoveOnDeallocDelegate : RTCAudioSessionTestDelegate
@end

@implementation RTCTestRemoveOnDeallocDelegate

- (instancetype)init {
  if (self = [super init]) {
    RTCAudioSession *session = [RTCAudioSession sharedInstance];
    [session addDelegate:self];
  }
  return self;
}

- (void)dealloc {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  [session removeDelegate:self];
}

@end


@interface RTCAudioSessionTest : NSObject

- (void)testLockForConfiguration;

@end

@implementation RTCAudioSessionTest

- (void)testLockForConfiguration {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];

  for (size_t i = 0; i < 2; i++) {
    [session lockForConfiguration];
    EXPECT_TRUE(session.isLocked);
  }
  for (size_t i = 0; i < 2; i++) {
    EXPECT_TRUE(session.isLocked);
    [session unlockForConfiguration];
  }
  EXPECT_FALSE(session.isLocked);
}

- (void)testAddAndRemoveDelegates {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  NSMutableArray *delegates = [NSMutableArray array];
  const size_t count = 5;
  for (size_t i = 0; i < count; ++i) {
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    [delegates addObject:delegate];
    EXPECT_EQ(i + 1, session.delegates.size());
  }
  [delegates enumerateObjectsUsingBlock:^(RTCAudioSessionTestDelegate *obj,
                                          NSUInteger idx,
                                          BOOL *stop) {
    [session removeDelegate:obj];
  }];
  EXPECT_EQ(0u, session.delegates.size());
}

- (void)testPushDelegate {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  NSMutableArray *delegates = [NSMutableArray array];
  const size_t count = 2;
  for (size_t i = 0; i < count; ++i) {
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    [delegates addObject:delegate];
  }
  // Test that it gets added to the front of the list.
  RTCAudioSessionTestDelegate *pushedDelegate =
      [[RTCAudioSessionTestDelegate alloc] init];
  [session pushDelegate:pushedDelegate];
  EXPECT_TRUE(pushedDelegate == session.delegates[0]);

  // Test that it stays at the front of the list.
  for (size_t i = 0; i < count; ++i) {
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    [delegates addObject:delegate];
  }
  EXPECT_TRUE(pushedDelegate == session.delegates[0]);

  // Test that the next one goes to the front too.
  pushedDelegate = [[RTCAudioSessionTestDelegate alloc] init];
  [session pushDelegate:pushedDelegate];
  EXPECT_TRUE(pushedDelegate == session.delegates[0]);
}

// Tests that delegates added to the audio session properly zero out. This is
// checking an implementation detail (that vectors of __weak work as expected).
- (void)testZeroingWeakDelegate {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  @autoreleasepool {
    // Add a delegate to the session. There should be one delegate at this
    // point.
    RTCAudioSessionTestDelegate *delegate =
        [[RTCAudioSessionTestDelegate alloc] init];
    [session addDelegate:delegate];
    EXPECT_EQ(1u, session.delegates.size());
    EXPECT_TRUE(session.delegates[0]);
  }
  // The previously created delegate should've de-alloced, leaving a nil ptr.
  EXPECT_FALSE(session.delegates[0]);
  RTCAudioSessionTestDelegate *delegate =
      [[RTCAudioSessionTestDelegate alloc] init];
  [session addDelegate:delegate];
  // On adding a new delegate, nil ptrs should've been cleared.
  EXPECT_EQ(1u, session.delegates.size());
  EXPECT_TRUE(session.delegates[0]);
}

// Tests that we don't crash when removing delegates in dealloc.
// Added as a regression test.
- (void)testRemoveDelegateOnDealloc {
  @autoreleasepool {
    RTCTestRemoveOnDeallocDelegate *delegate =
        [[RTCTestRemoveOnDeallocDelegate alloc] init];
    EXPECT_TRUE(delegate);
  }
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  EXPECT_EQ(0u, session.delegates.size());
}

@end

namespace webrtc {

class AudioSessionTest : public ::testing::Test {
 protected:
  void TearDown() {
    RTCAudioSession *session = [RTCAudioSession sharedInstance];
    for (id<RTCAudioSessionDelegate> delegate : session.delegates) {
      [session removeDelegate:delegate];
    }
  }
};

TEST_F(AudioSessionTest, LockForConfiguration) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testLockForConfiguration];
}

TEST_F(AudioSessionTest, AddAndRemoveDelegates) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testAddAndRemoveDelegates];
}

TEST_F(AudioSessionTest, PushDelegate) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testPushDelegate];
}

TEST_F(AudioSessionTest, ZeroingWeakDelegate) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testZeroingWeakDelegate];
}

TEST_F(AudioSessionTest, RemoveDelegateOnDealloc) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testRemoveDelegateOnDealloc];
}

}  // namespace webrtc
