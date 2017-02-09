/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCOpenGLDefines.h"
#import <vector>
#import "webrtc/base/optional.h"
#import "webrtc/common_video/rotation.h"

@class RTCVideoFrame;

@protocol RTCShader <NSObject>

- (BOOL)drawFrame:(RTCVideoFrame *)frame;

@end

// |kNumTextures| must not exceed 8, which is the limit in OpenGLES2. Two sets
// of 3 textures are used here, one for each of the Y, U and V planes. Having
// two sets alleviates CPU blockage in the event that the GPU is asked to render
// to a texture that is already in use.
static const GLsizei kNumTextureSets = 2;
static const GLsizei kNumTexturesPerSet = 3;
static const GLsizei kNumTextures = kNumTexturesPerSet * kNumTextureSets;

// Shader for non-native I420 frames.
@interface RTCI420Shader : NSObject <RTCShader> {
  BOOL _hasUnpackRowLength;
  GLint _currentTextureSet;
  // Handles for OpenGL constructs.
  GLuint _textures[kNumTextures];
  GLuint _i420Program;
  GLuint _vertexArray;
  GLuint _vertexBuffer;
  GLint _ySampler;
  GLint _uSampler;
  GLint _vSampler;
  // Store current rotation and only upload new vertex data when rotation
  // changes.
  rtc::Optional<webrtc::VideoRotation> _currentRotation;
  // Used to create a non-padded plane for GPU upload when we receive padded
  // frames.
  std::vector<uint8_t> _planeBuffer;
}

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContext:(GlContextType *)context NS_DESIGNATED_INITIALIZER;
- (BOOL)drawFrame:(RTCVideoFrame *)frame;

@end

// Native CVPixelBufferRef rendering is only supported on iPhone because it
// depends on CVOpenGLESTextureCacheCreate.
#if TARGET_OS_IPHONE

// Shader for native NV12 frames.
@interface RTCNativeNV12Shader : NSObject <RTCShader>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContext:(GlContextType *)context NS_DESIGNATED_INITIALIZER;
- (BOOL)drawFrame:(RTCVideoFrame *)frame;

@end

#endif  // TARGET_OS_IPHONE
