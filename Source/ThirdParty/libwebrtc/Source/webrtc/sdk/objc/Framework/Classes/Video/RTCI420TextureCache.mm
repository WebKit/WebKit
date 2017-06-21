/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCI420TextureCache.h"

#if TARGET_OS_IPHONE
#import <OpenGLES/ES3/gl.h>
#else
#import <OpenGL/gl3.h>
#endif

#include <vector>

// Two sets of 3 textures are used here, one for each of the Y, U and V planes. Having two sets
// alleviates CPU blockage in the event that the GPU is asked to render to a texture that is already
// in use.
static const GLsizei kNumTextureSets = 2;
static const GLsizei kNumTexturesPerSet = 3;
static const GLsizei kNumTextures = kNumTexturesPerSet * kNumTextureSets;

@implementation RTCI420TextureCache {
  BOOL _hasUnpackRowLength;
  GLint _currentTextureSet;
  // Handles for OpenGL constructs.
  GLuint _textures[kNumTextures];
  // Used to create a non-padded plane for GPU upload when we receive padded frames.
  std::vector<uint8_t> _planeBuffer;
}

- (GLuint)yTexture {
  return _textures[_currentTextureSet * kNumTexturesPerSet];
}

- (GLuint)uTexture {
  return _textures[_currentTextureSet * kNumTexturesPerSet + 1];
}

- (GLuint)vTexture {
  return _textures[_currentTextureSet * kNumTexturesPerSet + 2];
}

- (instancetype)initWithContext:(GlContextType *)context {
  if (self = [super init]) {
#if TARGET_OS_IPHONE
    _hasUnpackRowLength = (context.API == kEAGLRenderingAPIOpenGLES3);
#else
    _hasUnpackRowLength = YES;
#endif
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    [self setupTextures];
  }
  return self;
}

- (void)dealloc {
  glDeleteTextures(kNumTextures, _textures);
}

- (void)setupTextures {
  glGenTextures(kNumTextures, _textures);
  // Set parameters for each of the textures we created.
  for (GLsizei i = 0; i < kNumTextures; i++) {
    glBindTexture(GL_TEXTURE_2D, _textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
}

- (void)uploadPlane:(const uint8_t *)plane
            texture:(GLuint)texture
              width:(size_t)width
             height:(size_t)height
             stride:(int32_t)stride {
  glBindTexture(GL_TEXTURE_2D, texture);

  const uint8_t *uploadPlane = plane;
  if ((size_t)stride != width) {
   if (_hasUnpackRowLength) {
      // GLES3 allows us to specify stride.
      glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   RTC_PIXEL_FORMAT,
                   static_cast<GLsizei>(width),
                   static_cast<GLsizei>(height),
                   0,
                   RTC_PIXEL_FORMAT,
                   GL_UNSIGNED_BYTE,
                   uploadPlane);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      return;
    } else {
      // Make an unpadded copy and upload that instead. Quick profiling showed
      // that this is faster than uploading row by row using glTexSubImage2D.
      uint8_t *unpaddedPlane = _planeBuffer.data();
      for (size_t y = 0; y < height; ++y) {
        memcpy(unpaddedPlane + y * width, plane + y * stride, width);
      }
      uploadPlane = unpaddedPlane;
    }
  }
  glTexImage2D(GL_TEXTURE_2D,
               0,
               RTC_PIXEL_FORMAT,
               static_cast<GLsizei>(width),
               static_cast<GLsizei>(height),
               0,
               RTC_PIXEL_FORMAT,
               GL_UNSIGNED_BYTE,
               uploadPlane);
}

- (void)uploadFrameToTextures:(RTCVideoFrame *)frame {
  _currentTextureSet = (_currentTextureSet + 1) % kNumTextureSets;

  const int chromaWidth = (frame.width + 1) / 2;
  const int chromaHeight = (frame.height + 1) / 2;
  if (frame.strideY != frame.width ||
      frame.strideU != chromaWidth ||
      frame.strideV != chromaWidth) {
    _planeBuffer.resize(frame.width * frame.height);
  }

  [self uploadPlane:frame.dataY
            texture:self.yTexture
              width:frame.width
             height:frame.height
             stride:frame.strideY];

  [self uploadPlane:frame.dataU
            texture:self.uTexture
              width:chromaWidth
             height:chromaHeight
             stride:frame.strideU];

  [self uploadPlane:frame.dataV
            texture:self.vTexture
              width:chromaWidth
             height:chromaHeight
             stride:frame.strideV];
}

@end
