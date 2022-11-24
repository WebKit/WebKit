//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// android_util.h: Utilities for the using the Android platform

#ifndef COMMON_ANDROIDUTIL_H_
#define COMMON_ANDROIDUTIL_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <array>
#include <cstdint>

#include "angle_gl.h"

struct ANativeWindowBuffer;
struct AHardwareBuffer;

namespace angle
{

namespace android
{

constexpr std::array<GLenum, 3> kSupportedSizedInternalFormats = {GL_RGBA8, GL_RGB8, GL_RGB565};

ANativeWindowBuffer *ClientBufferToANativeWindowBuffer(EGLClientBuffer clientBuffer);
EGLClientBuffer AHardwareBufferToClientBuffer(const AHardwareBuffer *hardwareBuffer);
AHardwareBuffer *ClientBufferToAHardwareBuffer(EGLClientBuffer clientBuffer);

EGLClientBuffer CreateEGLClientBufferFromAHardwareBuffer(int width,
                                                         int height,
                                                         int depth,
                                                         int androidFormat,
                                                         int usage);

void GetANativeWindowBufferProperties(const ANativeWindowBuffer *buffer,
                                      int *width,
                                      int *height,
                                      int *depth,
                                      int *pixelFormat,
                                      uint64_t *usage);
GLenum NativePixelFormatToGLInternalFormat(int pixelFormat);
int GLInternalFormatToNativePixelFormat(GLenum internalFormat);

bool NativePixelFormatIsYUV(int pixelFormat);

AHardwareBuffer *ANativeWindowBufferToAHardwareBuffer(ANativeWindowBuffer *windowBuffer);

uint64_t GetAHBUsage(int eglNativeBufferUsage);

}  // namespace android
}  // namespace angle

#endif  // COMMON_ANDROIDUTIL_H_
