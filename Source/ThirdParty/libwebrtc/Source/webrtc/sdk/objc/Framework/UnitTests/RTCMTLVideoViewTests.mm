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

+ (BOOL)isMetalAvailable;
+ (UIView*)createMetalView:(CGRect)frame;
+ (id<RTCMTLRenderer>)createNV12Renderer;
+ (id<RTCMTLRenderer>)createI420Renderer;
- (void)drawInMTKView:(id)view;
@end

@interface RTCMTLVideoViewTests : NSObject
@property(nonatomic, strong) id classMock;
@property(nonatomic, strong) id metalViewMock;
@property(nonatomic, strong) id rendererNV12Mock;
@property(nonatomic, strong) id rendererI420Mock;
@property(nonatomic, strong) id frameMock;
@end

@implementation RTCMTLVideoViewTests

@synthesize classMock = _classMock;
@synthesize metalViewMock = _metalViewMock;
@synthesize rendererNV12Mock = _rendererNV12Mock;
@synthesize rendererI420Mock = _rendererI420Mock;
@synthesize frameMock = _frameMock;

- (void)setup {
  self.classMock = OCMClassMock([RTCMTLVideoView class]);
}

- (void)tearDown {
  [self.classMock stopMocking];
  [self.rendererI420Mock stopMocking];
  [self.rendererNV12Mock stopMocking];
  [self.metalViewMock stopMocking];
  [self.frameMock stopMocking];
  self.classMock = nil;
  self.rendererI420Mock = nil;
  self.rendererNV12Mock = nil;
  self.metalViewMock = nil;
  self.frameMock = nil;
}

- (id)frameMockWithNativeHandle:(BOOL)hasNativeHandle {
  id frameMock = OCMClassMock([RTCVideoFrame class]);
  if (hasNativeHandle) {
    OCMStub([frameMock nativeHandle]).andReturn((CVPixelBufferRef)[OCMArg anyPointer]);
  } else {
    OCMStub([frameMock nativeHandle]).andReturn((CVPixelBufferRef) nullptr);
  }
  return frameMock;
}

- (id)rendererMockWithSuccessfulSetup:(BOOL)sucess {
  id rendererMock = OCMProtocolMock(@protocol(RTCMTLRenderer));
  OCMStub([rendererMock addRenderingDestination:[OCMArg any]]).andReturn(sucess);

  return rendererMock;
}

#pragma mark - Test cases
- (void)testInitAssertsIfMetalUnavailabe {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(NO);

  // when
  BOOL asserts = NO;
  @try {
    RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] initWithFrame:CGRectZero];
    (void)realView;
  } @catch (NSException *ex) {
    asserts = YES;
  }

  EXPECT_TRUE(asserts);
}

- (void)testRTCVideoRenderNilFrameCallback {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);
  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];
  self.frameMock = OCMClassMock([RTCVideoFrame class]);

  [[self.frameMock reject] nativeHandle];
  [[self.classMock reject] createNV12Renderer];
  [[self.classMock reject] createI420Renderer];

  // when
  [realView renderFrame:nil];
  [realView drawInMTKView:self.metalViewMock];

  // then
  [self.frameMock verify];
  [self.classMock verify];
}

- (void)testRTCVideoRenderFrameCallbackI420 {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);
  self.rendererI420Mock = [self rendererMockWithSuccessfulSetup:YES];
  self.frameMock = [self frameMockWithNativeHandle:NO];

  OCMExpect([self.rendererI420Mock drawFrame:self.frameMock]);
  OCMExpect([self.classMock createI420Renderer]).andReturn(self.rendererI420Mock);
  [[self.classMock reject] createNV12Renderer];

  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // when
  [realView renderFrame:self.frameMock];
  [realView drawInMTKView:self.metalViewMock];

  // then
  [self.rendererI420Mock verify];
  [self.classMock verify];
}

- (void)testRTCVideoRenderFrameCallbackNV12 {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);
  self.rendererNV12Mock = [self rendererMockWithSuccessfulSetup:YES];
  self.frameMock = [self frameMockWithNativeHandle:YES];

  OCMExpect([self.rendererNV12Mock drawFrame:self.frameMock]);
  OCMExpect([self.classMock createNV12Renderer]).andReturn(self.rendererNV12Mock);
  [[self.classMock reject] createI420Renderer];

  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // when
  [realView renderFrame:self.frameMock];
  [realView drawInMTKView:self.metalViewMock];

  // then
  [self.rendererNV12Mock verify];
  [self.classMock verify];
}
@end

TEST(RTCMTLVideoViewTests, InitAssertsIfMetalUnavailabe) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];

  [test testInitAssertsIfMetalUnavailabe];
  [test tearDown];
}

TEST(RTCMTLVideoViewTests, RTCVideoRenderNilFrameCallback) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];
  [test testRTCVideoRenderNilFrameCallback];
  [test tearDown];
}

TEST(RTCMTLVideoViewTests, RTCVideoRenderFrameCallbackI420) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];

  [test testRTCVideoRenderFrameCallbackI420];
  [test tearDown];
}

TEST(RTCMTLVideoViewTests, RTCVideoRenderFrameCallbackNV12) {
  RTCMTLVideoViewTests *test = [[RTCMTLVideoViewTests alloc] init];
  [test setup];

  [test testRTCVideoRenderFrameCallbackNV12];
  [test tearDown];
}
