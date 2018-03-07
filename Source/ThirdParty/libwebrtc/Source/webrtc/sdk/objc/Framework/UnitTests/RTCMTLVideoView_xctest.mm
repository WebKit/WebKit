/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>

#include <WebRTC/RTCMTLVideoView.h>
#include <WebRTC/RTCVideoFrameBuffer.h>

#include "Metal/RTCMTLNV12Renderer.h"

// Extension of RTCMTLVideoView for testing purposes.
@interface RTCMTLVideoView (Testing)

+ (BOOL)isMetalAvailable;
+ (UIView *)createMetalView:(CGRect)frame;
+ (id<RTCMTLRenderer>)createNV12Renderer;
+ (id<RTCMTLRenderer>)createI420Renderer;
- (void)drawInMTKView:(id)view;
@end

@interface RTCMTLVideoViewTests : XCTestCase
@property(nonatomic, strong) id classMock;
@property(nonatomic, strong) id rendererNV12Mock;
@property(nonatomic, strong) id rendererI420Mock;
@property(nonatomic, strong) id frameMock;
@end

@implementation RTCMTLVideoViewTests

@synthesize classMock = _classMock;
@synthesize rendererNV12Mock = _rendererNV12Mock;
@synthesize rendererI420Mock = _rendererI420Mock;
@synthesize frameMock = _frameMock;

- (void)setUp {
  self.classMock = OCMClassMock([RTCMTLVideoView class]);
  [self startMockingNilView];
}

- (void)tearDown {
  [self.classMock stopMocking];
  [self.rendererI420Mock stopMocking];
  [self.rendererNV12Mock stopMocking];
  [self.frameMock stopMocking];
  self.classMock = nil;
  self.rendererI420Mock = nil;
  self.rendererNV12Mock = nil;
  self.frameMock = nil;
}

- (id)frameMockWithCVPixelBuffer:(BOOL)hasCVPixelBuffer {
  id frameMock = OCMClassMock([RTCVideoFrame class]);
  if (hasCVPixelBuffer) {
    CVPixelBufferRef pixelBufferRef;
    CVPixelBufferCreate(kCFAllocatorDefault,
                        200,
                        200,
                        kCVPixelFormatType_420YpCbCr8Planar,
                        nullptr,
                        &pixelBufferRef);
    OCMStub([frameMock buffer])
        .andReturn([[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef]);
  } else {
    OCMStub([frameMock buffer]).andReturn([[RTCI420Buffer alloc] initWithWidth:200 height:200]);
  }
  return frameMock;
}

- (id)rendererMockWithSuccessfulSetup:(BOOL)sucess {
  id rendererMock = OCMProtocolMock(@protocol(RTCMTLRenderer));
  OCMStub([rendererMock addRenderingDestination:[OCMArg any]]).andReturn(sucess);

  return rendererMock;
}

- (void)startMockingNilView {
  // Use OCMock 2 syntax here until OCMock is upgraded to 3.4
  [[[self.classMock stub] andReturn:nil] createMetalView:CGRectZero];
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

  XCTAssertTrue(asserts);
}

- (void)testRTCVideoRenderNilFrameCallback {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);

  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];
  self.frameMock = OCMClassMock([RTCVideoFrame class]);

  [[self.frameMock reject] buffer];
  [[self.classMock reject] createNV12Renderer];
  [[self.classMock reject] createI420Renderer];

  // when
  [realView renderFrame:nil];
  [realView drawInMTKView:nil];

  // then
  [self.frameMock verify];
  [self.classMock verify];
}

- (void)testRTCVideoRenderFrameCallbackI420 {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);
  self.rendererI420Mock = [self rendererMockWithSuccessfulSetup:YES];
  self.frameMock = [self frameMockWithCVPixelBuffer:NO];

  OCMExpect([self.rendererI420Mock drawFrame:self.frameMock]);
  OCMExpect([self.classMock createI420Renderer]).andReturn(self.rendererI420Mock);
  [[self.classMock reject] createNV12Renderer];

  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // when
  [realView renderFrame:self.frameMock];
  [realView drawInMTKView:nil];

  // then
  [self.rendererI420Mock verify];
  [self.classMock verify];
}

- (void)testRTCVideoRenderFrameCallbackNV12 {
  // given
  OCMStub([self.classMock isMetalAvailable]).andReturn(YES);
  self.rendererNV12Mock = [self rendererMockWithSuccessfulSetup:YES];
  self.frameMock = [self frameMockWithCVPixelBuffer:YES];

  OCMExpect([self.rendererNV12Mock drawFrame:self.frameMock]);
  OCMExpect([self.classMock createNV12Renderer]).andReturn(self.rendererNV12Mock);
  [[self.classMock reject] createI420Renderer];

  RTCMTLVideoView *realView = [[RTCMTLVideoView alloc] init];

  // when
  [realView renderFrame:self.frameMock];
  [realView drawInMTKView:nil];

  // then
  [self.rendererNV12Mock verify];
  [self.classMock verify];
}

@end
