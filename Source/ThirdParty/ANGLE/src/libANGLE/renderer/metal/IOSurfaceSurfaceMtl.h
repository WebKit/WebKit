//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_METAL_IOSURFACESURFACEMTL_H_
#define LIBANGLE_RENDERER_METAL_IOSURFACESURFACEMTL_H_

#include <IOSurface/IOSurfaceRef.h>
#include "libANGLE/renderer/SurfaceImpl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/SurfaceMtl.h"

namespace metal
{
class AttributeMap;
}  // namespace metal

namespace rx
{

class DisplayMTL;

class IOSurfaceSurfaceMtl : public SurfaceMtlProtocol
{
  public:
    IOSurfaceSurfaceMtl(DisplayMtl *display,
                        const egl::SurfaceState &state,
                        EGLClientBuffer buffer,
                        const egl::AttributeMap &attribs);
    ~IOSurfaceSurfaceMtl() override;

    void destroy(const egl::Display *display) override;

    egl::Error initialize(const egl::Display *display) override;
    FramebufferImpl *createDefaultFramebuffer(const gl::Context *context,
                                              const gl::FramebufferState &state) override;

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
    egl::Error getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc) override;
    egl::Error getMscRate(EGLint *numerator, EGLint *denominator) override;
    void setSwapInterval(EGLint interval) override;
    void setFixedWidth(EGLint width) override;
    void setFixedHeight(EGLint height) override;

    // Width can change when the client window resizes.
    EGLint getWidth() const override;
    // Height can change when the client window resizes.
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    angle::Result ensureCurrentDrawableObtained(const gl::Context *context) override;
    angle::Result ensureCurrentDrawableObtained(const gl::Context *context,
                                                bool *newDrawableOut) override;
    int getSamples() const override { return 1; }
    bool preserveBuffer() const override { return false; }
    mtl::TextureRef getTexture(const gl::Context *context);
    bool hasRobustResourceInit() const override { return false; }
    angle::Result ensureColorTextureReadyForReadPixels(const gl::Context *context) override
    {
        return angle::Result::Continue;
    }
    const mtl::TextureRef &getColorTexture() override { return mIOSurfaceTexture; }

  private:
    angle::Result ensureTextureCreated(const gl::Context *context);
    angle::Result createBackingTexture(const gl::Context *context);
    angle::Result createTexture(const gl::Context *context,
                                const mtl::Format &format,
                                uint32_t width,
                                uint32_t height,
                                bool renderTargetOnly,
                                mtl::TextureRef *textureOut);

    angle::Result initializeAlphaChannel(const gl::Context *context, GLuint texture);

#if defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    IOSurfaceLockOptions getIOSurfaceLockOptions() const;
#endif

    ContextMtl *contextMTL;
    IOSurfaceRef mIOSurface;
    GLint mGLInternalFormat;
    mtl::TextureRef mIOSurfaceTexture;
    mtl::TextureRef mIOSurfaceTextureView;
    mtl::Format mFormat;
    mtl::Format mInternalFormat;
    RenderTargetMtl mRenderTarget;
    bool mIOSurfaceTextureCreated;
    int mWidth;
    int mHeight;
    int mPlane;
    int mFormatIndex;
    int mRowStrideInPixels;

#if defined(ANGLE_PLATFORM_IOS_SIMULATOR)
    GLuint mBoundTextureID;
    bool mUploadFromIOSurface;
    bool mReadbackToIOSurface;
#endif
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EAGL_IOSURFACESURFACEEAGL_H_
