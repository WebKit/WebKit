//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// NativeBufferImageSiblingAndroid.h: Defines the NativeBufferImageSiblingAndroid to wrap EGL images
// created from ANativeWindowBuffer objects

#ifndef LIBANGLE_RENDERER_GL_EGL_ANDROID_NATIVEBUFFERIMAGESIBLINGANDROID_H_
#define LIBANGLE_RENDERER_GL_EGL_ANDROID_NATIVEBUFFERIMAGESIBLINGANDROID_H_

#include "libANGLE/renderer/gl/egl/ExternalImageSiblingEGL.h"

namespace rx
{

class NativeBufferImageSiblingAndroid : public ExternalImageSiblingEGL
{
  public:
    NativeBufferImageSiblingAndroid(EGLClientBuffer buffer);
    virtual ~NativeBufferImageSiblingAndroid();

    egl::Error initialize(const egl::Display *display) override;

    // ExternalImageSiblingImpl interface
    gl::Format getFormat() const override;
    bool isRenderable(const gl::Context *context) const override;
    bool isTexturable(const gl::Context *context) const override;
    gl::Extents getSize() const override;
    size_t getSamples() const override;

    // ExternalImageSiblingEGL interface
    EGLClientBuffer getBuffer() const override;

  private:
    EGLClientBuffer mBuffer;
    gl::Extents mSize;
    gl::Format mFormat;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EGL_ANDROID_NATIVEBUFFERIMAGESIBLINGANDROID_H_
