/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCShader.h"

#include <vector>

#import "RTCShader+Private.h"
#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/base/optional.h"

// |kNumTextures| must not exceed 8, which is the limit in OpenGLES2. Two sets
// of 3 textures are used here, one for each of the Y, U and V planes. Having
// two sets alleviates CPU blockage in the event that the GPU is asked to render
// to a texture that is already in use.
static const GLsizei kNumTextureSets = 2;
static const GLsizei kNumTexturesPerSet = 3;
static const GLsizei kNumTextures = kNumTexturesPerSet * kNumTextureSets;

// Fragment shader converts YUV values from input textures into a final RGB
// pixel. The conversion formula is from http://www.fourcc.org/fccyvrgb.php.
static const char kI420FragmentShaderSource[] =
  SHADER_VERSION
  "precision highp float;"
  FRAGMENT_SHADER_IN " vec2 v_texcoord;\n"
  "uniform lowp sampler2D s_textureY;\n"
  "uniform lowp sampler2D s_textureU;\n"
  "uniform lowp sampler2D s_textureV;\n"
  FRAGMENT_SHADER_OUT
  "void main() {\n"
  "    float y, u, v, r, g, b;\n"
  "    y = " FRAGMENT_SHADER_TEXTURE "(s_textureY, v_texcoord).r;\n"
  "    u = " FRAGMENT_SHADER_TEXTURE "(s_textureU, v_texcoord).r;\n"
  "    v = " FRAGMENT_SHADER_TEXTURE "(s_textureV, v_texcoord).r;\n"
  "    u = u - 0.5;\n"
  "    v = v - 0.5;\n"
  "    r = y + 1.403 * v;\n"
  "    g = y - 0.344 * u - 0.714 * v;\n"
  "    b = y + 1.770 * u;\n"
  "    " FRAGMENT_SHADER_COLOR " = vec4(r, g, b, 1.0);\n"
  "  }\n";

@implementation RTCI420Shader {
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
  rtc::Optional<RTCVideoRotation> _currentRotation;
  // Used to create a non-padded plane for GPU upload when we receive padded
  // frames.
  std::vector<uint8_t> _planeBuffer;
}

- (instancetype)initWithContext:(GlContextType *)context {
  if (self = [super init]) {
#if TARGET_OS_IPHONE
    _hasUnpackRowLength = (context.API == kEAGLRenderingAPIOpenGLES3);
#else
    _hasUnpackRowLength = YES;
#endif
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (![self setupI420Program] || ![self setupTextures] ||
        !RTCSetupVerticesForProgram(_i420Program, &_vertexBuffer, &_vertexArray)) {
      self = nil;
    }
  }
  return self;
}

- (void)dealloc {
  glDeleteProgram(_i420Program);
  glDeleteTextures(kNumTextures, _textures);
  glDeleteBuffers(1, &_vertexBuffer);
  glDeleteVertexArrays(1, &_vertexArray);
}

- (BOOL)setupI420Program {
  _i420Program = RTCCreateProgramFromFragmentSource(kI420FragmentShaderSource);
  if (!_i420Program) {
    return NO;
  }
  _ySampler = glGetUniformLocation(_i420Program, "s_textureY");
  _uSampler = glGetUniformLocation(_i420Program, "s_textureU");
  _vSampler = glGetUniformLocation(_i420Program, "s_textureV");

  return (_ySampler >= 0 && _uSampler >= 0 && _vSampler >= 0);
}

- (BOOL)setupTextures {
  glGenTextures(kNumTextures, _textures);
  // Set parameters for each of the textures we created.
  for (GLsizei i = 0; i < kNumTextures; i++) {
    glBindTexture(GL_TEXTURE_2D, _textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  return YES;
}

- (BOOL)drawFrame:(RTCVideoFrame*)frame {
  glUseProgram(_i420Program);
  if (![self updateTextureDataForFrame:frame]) {
    return NO;
  }
#if !TARGET_OS_IPHONE
  glBindVertexArray(_vertexArray);
#endif
  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  if (!_currentRotation || frame.rotation != *_currentRotation) {
    _currentRotation = rtc::Optional<RTCVideoRotation>(frame.rotation);
    RTCSetVertexData(*_currentRotation);
  }
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  return YES;
}

- (void)uploadPlane:(const uint8_t *)plane
            sampler:(GLint)sampler
             offset:(GLint)offset
              width:(size_t)width
             height:(size_t)height
             stride:(int32_t)stride {
  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + offset));
  glBindTexture(GL_TEXTURE_2D, _textures[offset]);

  // When setting texture sampler uniforms, the texture index is used not
  // the texture handle.
  glUniform1i(sampler, offset);
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

- (BOOL)updateTextureDataForFrame:(RTCVideoFrame *)frame {
  GLint textureOffset = _currentTextureSet * 3;
  NSAssert(textureOffset + 3 <= kNumTextures, @"invalid offset");

  const int chromaWidth = (frame.width + 1) / 2;
  const int chromaHeight = (frame.height + 1) / 2;
  if (frame.strideY != frame.width ||
      frame.strideU != chromaWidth ||
      frame.strideV != chromaWidth) {
    _planeBuffer.resize(frame.width * frame.height);
  }

  [self uploadPlane:frame.dataY
            sampler:_ySampler
             offset:textureOffset
              width:frame.width
             height:frame.height
             stride:frame.strideY];

  [self uploadPlane:frame.dataU
            sampler:_uSampler
             offset:textureOffset + 1
              width:chromaWidth
             height:chromaHeight
             stride:frame.strideU];

  [self uploadPlane:frame.dataV
            sampler:_vSampler
             offset:textureOffset + 2
              width:chromaWidth
             height:chromaHeight
             stride:frame.strideV];

  _currentTextureSet = (_currentTextureSet + 1) % kNumTextureSets;
  return YES;
}

@end
