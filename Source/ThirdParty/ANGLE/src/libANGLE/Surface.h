//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.h: Defines the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#ifndef LIBANGLE_SURFACE_H_
#define LIBANGLE_SURFACE_H_

#include <EGL/egl.h>

#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/SurfaceImpl.h"

namespace gl
{
class Framebuffer;
class Texture;
}

namespace rx
{
class EGLImplFactory;
}

namespace egl
{
class AttributeMap;
class Display;
struct Config;

struct SurfaceState final : angle::NonCopyable
{
    SurfaceState(const egl::Config *configIn);

    gl::Framebuffer *defaultFramebuffer;
    const egl::Config *config;
};

class Surface : public gl::FramebufferAttachmentObject
{
  public:
    virtual ~Surface();

    rx::SurfaceImpl *getImplementation() const { return mImplementation; }

    EGLint getType() const;

    Error initialize(const Display &display);
    Error swap(const Display &display);
    Error swapWithDamage(EGLint *rects, EGLint n_rects);
    Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height);
    Error querySurfacePointerANGLE(EGLint attribute, void **value);
    Error bindTexImage(gl::Texture *texture, EGLint buffer);
    Error releaseTexImage(EGLint buffer);

    Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc);

    EGLint isPostSubBufferSupported() const;

    void setSwapInterval(EGLint interval);
    void setIsCurrent(Display *display, bool isCurrent);
    void onDestroy(Display *display);

    const Config *getConfig() const;

    // width and height can change with client window resizing
    EGLint getWidth() const;
    EGLint getHeight() const;
    EGLint getPixelAspectRatio() const;
    EGLenum getRenderBuffer() const;
    EGLenum getSwapBehavior() const;
    EGLenum getTextureFormat() const;
    EGLenum getTextureTarget() const;

    gl::Texture *getBoundTexture() const { return mTexture.get(); }
    gl::Framebuffer *getDefaultFramebuffer() { return mState.defaultFramebuffer; }

    EGLint isFixedSize() const;

    // FramebufferAttachmentObject implementation
    gl::Extents getAttachmentSize(const gl::FramebufferAttachment::Target &target) const override;
    const gl::Format &getAttachmentFormat(
        const gl::FramebufferAttachment::Target &target) const override;
    GLsizei getAttachmentSamples(const gl::FramebufferAttachment::Target &target) const override;

    void onAttach() override {}
    void onDetach() override {}
    GLuint getId() const override;

    bool flexibleSurfaceCompatibilityRequested() const
    {
        return mFlexibleSurfaceCompatibilityRequested;
    }
    EGLint getOrientation() const { return mOrientation; }

    bool directComposition() const { return mDirectComposition; }

  protected:
    Surface(EGLint surfaceType, const egl::Config *config, const AttributeMap &attributes);
    rx::FramebufferAttachmentObjectImpl *getAttachmentImpl() const override { return mImplementation; }

    gl::Framebuffer *createDefaultFramebuffer();

    // ANGLE-only method, used internally
    friend class gl::Texture;
    void releaseTexImageFromTexture();

    SurfaceState mState;
    rx::SurfaceImpl *mImplementation;
    int mCurrentCount;
    bool mDestroyed;

    EGLint mType;

    bool mPostSubBufferRequested;
    bool mFlexibleSurfaceCompatibilityRequested;

    bool mFixedSize;
    size_t mFixedWidth;
    size_t mFixedHeight;

    bool mDirectComposition;

    EGLenum mTextureFormat;
    EGLenum mTextureTarget;

    EGLint mPixelAspectRatio;      // Display aspect ratio
    EGLenum mRenderBuffer;         // Render buffer
    EGLenum mSwapBehavior;         // Buffer swap behavior

    EGLint mOrientation;

    BindingPointer<gl::Texture> mTexture;

    gl::Format mBackFormat;
    gl::Format mDSFormat;

  private:
    void destroy(const egl::Display *display);
};

class WindowSurface final : public Surface
{
  public:
    WindowSurface(rx::EGLImplFactory *implFactory,
                  const Config *config,
                  EGLNativeWindowType window,
                  const AttributeMap &attribs);
    ~WindowSurface() override;
};

class PbufferSurface final : public Surface
{
  public:
    PbufferSurface(rx::EGLImplFactory *implFactory,
                   const Config *config,
                   const AttributeMap &attribs);
    PbufferSurface(rx::EGLImplFactory *implFactory,
                   const Config *config,
                   EGLenum buftype,
                   EGLClientBuffer clientBuffer,
                   const AttributeMap &attribs);
    ~PbufferSurface() override;
};

class PixmapSurface final : public Surface
{
  public:
    PixmapSurface(rx::EGLImplFactory *implFactory,
                  const Config *config,
                  NativePixmapType nativePixmap,
                  const AttributeMap &attribs);
    ~PixmapSurface() override;
};

}  // namespace egl

#endif   // LIBANGLE_SURFACE_H_
