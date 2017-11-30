//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLImplFactory.h:
//   Factory interface for EGL Impl objects.
//

#ifndef LIBANGLE_RENDERER_EGLIMPLFACTORY_H_
#define LIBANGLE_RENDERER_EGLIMPLFACTORY_H_

#include "libANGLE/Stream.h"

namespace egl
{
class AttributeMap;
struct Config;
class ImageSibling;
struct ImageState;
struct SurfaceState;
}

namespace gl
{
class Context;
class ContextState;
}

namespace rx
{
class ContextImpl;
class ImageImpl;
class SurfaceImpl;

class EGLImplFactory : angle::NonCopyable
{
  public:
    EGLImplFactory() {}
    virtual ~EGLImplFactory() {}

    virtual SurfaceImpl *createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs) = 0;
    virtual SurfaceImpl *createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs) = 0;
    virtual SurfaceImpl *createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs) = 0;
    virtual SurfaceImpl *createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs) = 0;

    virtual ImageImpl *createImage(const egl::ImageState &state,
                                   EGLenum target,
                                   const egl::AttributeMap &attribs) = 0;

    virtual ContextImpl *createContext(const gl::ContextState &state) = 0;

    virtual StreamProducerImpl *createStreamProducerD3DTextureNV12(
        egl::Stream::ConsumerType consumerType,
        const egl::AttributeMap &attribs) = 0;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_EGLIMPLFACTORY_H_
