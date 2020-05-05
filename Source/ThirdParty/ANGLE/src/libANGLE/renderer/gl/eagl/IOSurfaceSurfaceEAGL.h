//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_GL_EAGL_IOSURFACESURFACEEAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_IOSURFACESURFACEEAGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"
#include "libANGLE/renderer/gl/eagl/DisplayEAGL.h"

#include <IOSurface/IOSurfaceRef.h>

namespace egl
{
class AttributeMap;
}  // namespace egl

namespace rx
{

class DisplayEAGL;
class FunctionsGL;
class StateManagerGL;

class IOSurfaceSurfaceEAGL : public SurfaceGL
{
  public:
    IOSurfaceSurfaceEAGL(const egl::SurfaceState &state,
                         EAGLContextObj cglContext,
                         EGLClientBuffer buffer,
                         const egl::AttributeMap &attribs);
    ~IOSurfaceSurfaceEAGL() override;

    egl::Error initialize(const egl::Display *display) override;
    egl::Error makeCurrent(const gl::Context *context) override;
    egl::Error unMakeCurrent(const gl::Context *context) override;

    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    static bool validateAttributes(EGLClientBuffer buffer, const egl::AttributeMap &attribs);
    FramebufferImpl *createDefaultFramebuffer(const gl::Context *context,
                                              const gl::FramebufferState &state) override;

    bool hasEmulatedAlphaChannel() const override;

  private:
    angle::Result initializeAlphaChannel(const gl::Context *context, GLuint texture);

#if defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    IOSurfaceLockOptions getIOSurfaceLockOptions() const;
#endif

    EAGLContextObj mEAGLContext;
    IOSurfaceRef mIOSurface;
    int mWidth;
    int mHeight;
    int mPlane;
    int mFormatIndex;

    bool mAlphaInitialized;
#if defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    GLuint mBoundTextureID;
    bool mUploadFromIOSurface;
    bool mReadbackToIOSurface;
#endif
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EAGL_IOSURFACESURFACEEAGL_H_
