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
#import <OCMock/OCMock.h>

#include "webrtc/base/gunit.h"

#include <Metal/RTCMTLNV12Renderer.h>
#include <WebRTC/RTCMTLVideoView.h>

// Extension of RTCMTLVideoView for testing purposes.
@interface RTCMTLVideoView (Testing)
@property(nonatomic, strong) id<RTCMTLRenderer> renderer;
@property(nonatomic, strong) UIView* metalView;
@property(atomic, strong) RTCVideoFrame* videoFrame;

+ (BOOL)isMetalAvailable;
+ (UIView*)createMetalView:(CGRect)frame;
+ (id<RTCMTLRenderer>)createMetalRenderer;
- (void)drawInMTKView:(id)view;
@end

@interface RTCMTLVideoViewTests : NSObject
@property(nonatomic, strong) id classMock;
@property(nonatomic, strong) id metalViewMock;
@property(nonatomic, strong) id rendererMock;
@end

@implementation RTCMTLVideoViewTests

@synthesize classMock = _classMock;
@synthesize metalViewMock = _metalViewMock;
@synthesize rendererMock = _rendererMock;

- (void)setup {
  self.classMock = OCMClassMock([RTCMTLVideoView class]);

  self.metalViewMock = OCMClassMock([RTCMTLVideoViewTests class]);
  // NOTE: OCMock doesen't provide modern syntax for -ignoringNonObjectArgs.
  [[[[self.classMock stub] ignoringNonObjectArgs] andReturn:self.metalViewMock]
      createMetalView:CGRectZero];

  self.rendererMock = OCMProtocolMock(@protocol(RTCMTLRenderer));
  OCMStub([self.classMock createMetalRenderer]).andReturn(self.rendererMock);
}

- (void)tearDown {
  [self.classMock stopMocking];
  [self.rendererMock stopMocking];
  [self.metalViewMock stopMocking];
  self.classMock = nil;
  self.rendererMock = nil;
  self.metalViewMock = nil;
}

- (void)testMetalConfigureNotExecuted {
  // when
  OCMStub([self.classMock isMetalAvailable]).andReturn(NO);
  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // then
  EXPECT_TRUE(realView.renderer == nil);
  EXPECT_TRUE(realView.metalView == nil);
}

- (void)testMetalConfigureExecuted {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);
  OCMStub([self.rendererMock addRenderingDestination:self.metalViewMock])
      .andReturn(NO);

  // when
  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // then
  EXPECT_TRUE(realView.renderer == nil);
  EXPECT_TRUE(realView.metalView != nil);
}

- (void)testMetalDrawCallback {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(NO);
  OCMExpect([self.rendererMock drawFrame:[OCMArg any]]);
  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];
  realView.metalView = self.metalViewMock;
  realView.renderer = self.rendererMock;

  // when
  [realView drawInMTKView:self.metalViewMock];

  // then
  [self.rendererMock verify];
}

- (void)testRTCVideoRenderNilFrameCallback {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(NO);
  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // when
  [realView renderFrame:nil];

  // then
  EXPECT_TRUE(realView.videoFrame == nil);
}

- (void)testRTCVideoRenderFrameCallback {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(NO);
  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];
  id frame = OCMClassMock([RTCVideoFrame class]);
  realView.metalView = self.metalViewMock;
  realView.renderer = self.rendererMock;
  OCMExpect([self.rendererMock drawFrame:frame]);

  // when
  [realView renderFrame:frame];
  [realView drawInMTKView:self.metalViewMock];

  // then
  EXPECT_EQ(realView.videoFrame, frame);
  [self.rendererMock verify];
}
@end

TEST(RTCMTLVideoViewTests, MetalConfigureNotExecuted) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];
  [test testMetalConfigureNotExecuted];
  [test tearDown];
}


TEST(RTCMTLVideoViewTests, MetalConfigureExecuted) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];
  [test testMetalConfigureExecuted];
  [test tearDown];
}

TEST(RTCMTLVideoViewTests, MetalDrawCallback) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];
  [test testMetalDrawCallback];
  [test tearDown];
}

TEST(RTCMTLVideoViewTests, RTCVideoRenderNilFrameCallback) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];
  [test testRTCVideoRenderNilFrameCallback];
  [test tearDown];
}

TEST(RTCMTLVideoViewTests, RTCVideoRenderFrameCallback) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];
  [test testRTCVideoRenderFrameCallback];
  [test tearDown];
}
