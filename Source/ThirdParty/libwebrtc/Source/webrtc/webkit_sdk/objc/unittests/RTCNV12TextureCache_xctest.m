/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>
#import <XCTest/XCTest.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/renderer/opengl/RTCNV12TextureCache.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

@interface RTCNV12TextureCacheTests : XCTestCase
@end

@implementation RTCNV12TextureCacheTests {
  EAGLContext *_glContext;
  RTCNV12TextureCache *_nv12TextureCache;
}

- (void)setUp {
  [super setUp];
  _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  if (!_glContext) {
    _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
  }
  _nv12TextureCache = [[RTCNV12TextureCache alloc] initWithContext:_glContext];
}

- (void)tearDown {
  _nv12TextureCache = nil;
  _glContext = nil;
  [super tearDown];
}

- (void)testNV12TextureCacheDoesNotCrashOnEmptyFrame {
  CVPixelBufferRef nullPixelBuffer = NULL;
  RTCCVPixelBuffer *badFrameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:nullPixelBuffer];
  RTCVideoFrame *badFrame = [[RTCVideoFrame alloc] initWithBuffer:badFrameBuffer
                                                         rotation:RTCVideoRotation_0
                                                      timeStampNs:0];
  [_nv12TextureCache uploadFrameToTextures:badFrame];
}

@end
