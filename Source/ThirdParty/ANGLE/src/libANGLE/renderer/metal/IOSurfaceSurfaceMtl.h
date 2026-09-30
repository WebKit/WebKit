//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_METAL_IOSURFACESURFACEMTL_H_
#define LIBANGLE_RENDERER_METAL_IOSURFACESURFACEMTL_H_

#include <IOSurface/IOSurfaceRef.h>
#include <array>
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

// Offscreen created from IOSurface
class IOSurfaceSurfaceMtl : public OffscreenSurfaceMtl
{
  public:
    IOSurfaceSurfaceMtl(DisplayMtl *display,
                        const egl::SurfaceState &state,
                        EGLClientBuffer buffer,
                        const egl::AttributeMap &attribs);
    ~IOSurfaceSurfaceMtl() override;

    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

    static bool ValidateAttributes(EGLClientBuffer buffer, const egl::AttributeMap &attribs);

    // Multiplanar YUV (NV12) support for GL_CHROMIUM_copy_texture.  When true, getColorTexture()
    // returns the luma (R8) plane and getChromaTexture() the chroma (RG8) plane.
    bool isYUV() const override { return mIsYUV; }
    const mtl::TextureRef &getChromaTexture() const { return mChromaTexture; }
    // Row-major YCbCr->RGB matrix: rgb[i] = dot(matrix[i], float4(y, cb, cr, 1)).
    const std::array<float, 12> &getYUVToRGBMatrix() const { return mYUVToRGBMatrix; }
    // When true, getColorTexture() is a single hardware-converting MTLPixelFormatYCBCR8 texture
    // that samples directly to RGB (no chroma plane / manual matrix needed).
    bool canSampleYUVDirectly() const { return mCanSampleYUVDirectly; }
    // EGL_IOSURFACE_ORIENTATION_ANGLE bits describing how the YUV planes map to the pbuffer.
    EGLint getYUVOrientation() const { return mYUVOrientation; }

  protected:
    angle::Result ensureTexturesSizeCorrect(const gl::Context *context) override;

  private:
    angle::Result ensureColorTextureCreated(const gl::Context *context);

    IOSurfaceRef mIOSurface;
    NSUInteger mIOSurfacePlane;
    int mIOSurfaceFormatIdx;

    bool mIsYUV = false;
    mtl::Format mChromaFormat;
    mtl::TextureRef mChromaTexture;
    std::array<float, 12> mYUVToRGBMatrix{};
    // YUV colorspace/range hints, used to pick the hardware CSC matrix in the SPI path.
    EGLint mYUVColorSpaceHint  = 0;
    EGLint mYUVRangeHint       = 0;
    EGLint mYUVOrientation     = 0;
    bool mCanSampleYUVDirectly = false;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_METAL_IOSURFACESURFACEMTL_H_
