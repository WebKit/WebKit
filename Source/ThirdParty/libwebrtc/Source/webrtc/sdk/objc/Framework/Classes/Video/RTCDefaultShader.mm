/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDefaultShader.h"

#if TARGET_OS_IPHONE
#import <OpenGLES/ES3/gl.h>
#else
#import <OpenGL/gl3.h>
#endif

#import "RTCOpenGLDefines.h"
#import "RTCShader.h"
#import "WebRTC/RTCLogging.h"

#include "api/optional.h"

static const int kYTextureUnit = 0;
static const int kUTextureUnit = 1;
static const int kVTextureUnit = 2;
static const int kUvTextureUnit = 1;

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

@implementation RTCDefaultShader {
  GLuint _vertexBuffer;
  GLuint _vertexArray;
  // Store current rotation and only upload new vertex data when rotation changes.
  rtc::Optional<RTCVideoRotation> _currentRotation;

  GLuint _i420Program;
  GLuint _nv12Program;
}

- (void)dealloc {
  glDeleteProgram(_i420Program);
  glDeleteProgram(_nv12Program);
  glDeleteBuffers(1, &_vertexBuffer);
  glDeleteVertexArrays(1, &_vertexArray);
}

- (BOOL)createAndSetupI420Program {
  NSAssert(!_i420Program, @"I420 program already created");
  _i420Program = RTCCreateProgramFromFragmentSource(kI420FragmentShaderSource);
  if (!_i420Program) {
    return NO;
  }
  GLint ySampler = glGetUniformLocation(_i420Program, "s_textureY");
  GLint uSampler = glGetUniformLocation(_i420Program, "s_textureU");
  GLint vSampler = glGetUniformLocation(_i420Program, "s_textureV");

  if (ySampler < 0 || uSampler < 0 || vSampler < 0) {
    RTCLog(@"Failed to get uniform variable locations in I420 shader");
    glDeleteProgram(_i420Program);
    _i420Program = 0;
    return NO;
  }

  glUseProgram(_i420Program);
  glUniform1i(ySampler, kYTextureUnit);
  glUniform1i(uSampler, kUTextureUnit);
  glUniform1i(vSampler, kVTextureUnit);

  return YES;
}

- (BOOL)createAndSetupNV12Program {
  NSAssert(!_nv12Program, @"NV12 program already created");
  _nv12Program = RTCCreateProgramFromFragmentSource(kNV12FragmentShaderSource);
  if (!_nv12Program) {
    return NO;
  }
  GLint ySampler = glGetUniformLocation(_nv12Program, "s_textureY");
  GLint uvSampler = glGetUniformLocation(_nv12Program, "s_textureUV");

  if (ySampler < 0 || uvSampler < 0) {
    RTCLog(@"Failed to get uniform variable locations in NV12 shader");
    glDeleteProgram(_nv12Program);
    _nv12Program = 0;
    return NO;
  }

  glUseProgram(_nv12Program);
  glUniform1i(ySampler, kYTextureUnit);
  glUniform1i(uvSampler, kUvTextureUnit);

  return YES;
}

- (BOOL)prepareVertexBufferWithRotation:(RTCVideoRotation)rotation {
  if (!_vertexBuffer && !RTCCreateVertexBuffer(&_vertexBuffer, &_vertexArray)) {
    RTCLog(@"Failed to setup vertex buffer");
    return NO;
  }
#if !TARGET_OS_IPHONE
  glBindVertexArray(_vertexArray);
#endif
  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  if (!_currentRotation || rotation != *_currentRotation) {
    _currentRotation = rtc::Optional<RTCVideoRotation>(rotation);
    RTCSetVertexData(*_currentRotation);
  }
  return YES;
}

- (void)applyShadingForFrameWithWidth:(int)width
                               height:(int)height
                             rotation:(RTCVideoRotation)rotation
                               yPlane:(GLuint)yPlane
                               uPlane:(GLuint)uPlane
                               vPlane:(GLuint)vPlane {
  if (![self prepareVertexBufferWithRotation:rotation]) {
    return;
  }

  if (!_i420Program && ![self createAndSetupI420Program]) {
    RTCLog(@"Failed to setup I420 program");
    return;
  }

  glUseProgram(_i420Program);

  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kYTextureUnit));
  glBindTexture(GL_TEXTURE_2D, yPlane);

  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kUTextureUnit));
  glBindTexture(GL_TEXTURE_2D, uPlane);

  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kVTextureUnit));
  glBindTexture(GL_TEXTURE_2D, vPlane);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

- (void)applyShadingForFrameWithWidth:(int)width
                               height:(int)height
                             rotation:(RTCVideoRotation)rotation
                               yPlane:(GLuint)yPlane
                              uvPlane:(GLuint)uvPlane {
  if (![self prepareVertexBufferWithRotation:rotation]) {
    return;
  }

  if (!_nv12Program && ![self createAndSetupNV12Program]) {
    RTCLog(@"Failed to setup NV12 shader");
    return;
  }

  glUseProgram(_nv12Program);

  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kYTextureUnit));
  glBindTexture(GL_TEXTURE_2D, yPlane);

  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kUvTextureUnit));
  glBindTexture(GL_TEXTURE_2D, uvPlane);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

@end
