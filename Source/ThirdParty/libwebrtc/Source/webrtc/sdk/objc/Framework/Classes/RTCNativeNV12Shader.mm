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

// Native CVPixelBufferRef rendering is only supported on iPhone because it
// depends on CVOpenGLESTextureCacheCreate.
#if TARGET_OS_IPHONE

#import <CoreVideo/CVOpenGLESTextureCache.h>

#import "RTCShader+Private.h"
#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"

static const char kNV12FragmentShaderSource[] =
  SHADER_VERSION
  "precision mediump float;"
  FRAGMENT_SHADER_IN " vec2 v_texcoord;\n"
  "uniform lowp sampler2D s_textureY;\n"
  "uniform lowp sampler2D s_textureUV;\n"
  FRAGMENT_SHADER_OUT
  "void main() {\n"
  "    mediump float y;\n"
  "    mediump vec2 uv;\n"
  "    y = " FRAGMENT_SHADER_TEXTURE "(s_textureY, v_texcoord).r;\n"
  "    uv = " FRAGMENT_SHADER_TEXTURE "(s_textureUV, v_texcoord).ra -\n"
  "        vec2(0.5, 0.5);\n"
  "    " FRAGMENT_SHADER_COLOR " = vec4(y + 1.403 * uv.y,\n"
  "                                     y - 0.344 * uv.x - 0.714 * uv.y,\n"
  "                                     y + 1.770 * uv.x,\n"
  "                                     1.0);\n"
  "  }\n";

@implementation RTCNativeNV12Shader {
  GLuint _vertexBuffer;
  GLuint _nv12Program;
  GLint _ySampler;
  GLint _uvSampler;
  CVOpenGLESTextureCacheRef _textureCache;
  // Store current rotation and only upload new vertex data when rotation
  // changes.
  rtc::Optional<RTCVideoRotation> _currentRotation;
}

- (instancetype)initWithContext:(GlContextType *)context {
  if (self = [super init]) {
    if (![self setupNV12Program] || ![self setupTextureCacheWithContext:context] ||
        !RTCSetupVerticesForProgram(_nv12Program, &_vertexBuffer, nullptr)) {
      self = nil;
    }
  }
  return self;
}

- (void)dealloc {
  glDeleteProgram(_nv12Program);
  glDeleteBuffers(1, &_vertexBuffer);
  if (_textureCache) {
    CFRelease(_textureCache);
    _textureCache = nullptr;
  }
}

- (BOOL)setupNV12Program {
  _nv12Program = RTCCreateProgramFromFragmentSource(kNV12FragmentShaderSource);
  if (!_nv12Program) {
    return NO;
  }
  _ySampler = glGetUniformLocation(_nv12Program, "s_textureY");
  _uvSampler = glGetUniformLocation(_nv12Program, "s_textureUV");

  return (_ySampler >= 0 && _uvSampler >= 0);
}

- (BOOL)setupTextureCacheWithContext:(GlContextType *)context {
  CVReturn ret = CVOpenGLESTextureCacheCreate(
      kCFAllocatorDefault, NULL,
#if COREVIDEO_USE_EAGLCONTEXT_CLASS_IN_API
      context,
#else
      (__bridge void *)context,
#endif
      NULL, &_textureCache);
  return ret == kCVReturnSuccess;
}

- (BOOL)drawFrame:(RTCVideoFrame *)frame {
  CVPixelBufferRef pixelBuffer = frame.nativeHandle;
  RTC_CHECK(pixelBuffer);
  glUseProgram(_nv12Program);
  const OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  RTC_CHECK(pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
            pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
      << "Unsupported native pixel format: " << pixelFormat;

  // Y-plane.
  const int lumaWidth = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
  const int lumaHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);

  CVOpenGLESTextureRef lumaTexture = nullptr;
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(_ySampler, 0);
  CVReturn ret = CVOpenGLESTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, _textureCache, pixelBuffer, NULL, GL_TEXTURE_2D,
      RTC_PIXEL_FORMAT, lumaWidth, lumaHeight, RTC_PIXEL_FORMAT,
      GL_UNSIGNED_BYTE, 0, &lumaTexture);
  if (ret != kCVReturnSuccess) {
    CFRelease(lumaTexture);
    return NO;
  }

  RTC_CHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
               CVOpenGLESTextureGetTarget(lumaTexture));
  glBindTexture(GL_TEXTURE_2D, CVOpenGLESTextureGetName(lumaTexture));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // UV-plane.
  const int chromaWidth = CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
  const int chromeHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer, 1);

  CVOpenGLESTextureRef chromaTexture = nullptr;
  glActiveTexture(GL_TEXTURE1);
  glUniform1i(_uvSampler, 1);
  ret = CVOpenGLESTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, _textureCache, pixelBuffer, NULL, GL_TEXTURE_2D,
      GL_LUMINANCE_ALPHA, chromaWidth, chromeHeight, GL_LUMINANCE_ALPHA,
      GL_UNSIGNED_BYTE, 1, &chromaTexture);
  if (ret != kCVReturnSuccess) {
    CFRelease(chromaTexture);
    CFRelease(lumaTexture);
    return NO;
  }

  RTC_CHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
               CVOpenGLESTextureGetTarget(chromaTexture));
  glBindTexture(GL_TEXTURE_2D, CVOpenGLESTextureGetName(chromaTexture));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  if (!_currentRotation || frame.rotation != *_currentRotation) {
    _currentRotation = rtc::Optional<RTCVideoRotation>(frame.rotation);
    RTCSetVertexData(*_currentRotation);
  }
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  CFRelease(chromaTexture);
  CFRelease(lumaTexture);

  return YES;
}

@end
#endif  // TARGET_OS_IPHONE
