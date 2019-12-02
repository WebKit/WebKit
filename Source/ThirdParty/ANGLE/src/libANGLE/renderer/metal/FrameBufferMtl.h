//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameBufferMtl.h:
//    Defines the class interface for FrameBufferMtl, implementing FrameBufferImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_FRAMEBUFFERMTL_H_
#define LIBANGLE_RENDERER_METAL_FRAMEBUFFERMTL_H_

#import <Metal/Metal.h>

#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/metal/RenderTargetMtl.h"
#include "libANGLE/renderer/metal/mtl_render_utils.h"

namespace rx
{
class ContextMtl;
class SurfaceMtl;

class FramebufferMtl : public FramebufferImpl
{
  public:
    explicit FramebufferMtl(const gl::FramebufferState &state, bool flipY);
    ~FramebufferMtl() override;
    void destroy(const gl::Context *context) override;

    angle::Result discard(const gl::Context *context,
                          size_t count,
                          const GLenum *attachments) override;
    angle::Result invalidate(const gl::Context *context,
                             size_t count,
                             const GLenum *attachments) override;
    angle::Result invalidateSub(const gl::Context *context,
                                size_t count,
                                const GLenum *attachments,
                                const gl::Rectangle &area) override;

    angle::Result clear(const gl::Context *context, GLbitfield mask) override;
    angle::Result clearBufferfv(const gl::Context *context,
                                GLenum buffer,
                                GLint drawbuffer,
                                const GLfloat *values) override;
    angle::Result clearBufferuiv(const gl::Context *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLuint *values) override;
    angle::Result clearBufferiv(const gl::Context *context,
                                GLenum buffer,
                                GLint drawbuffer,
                                const GLint *values) override;
    angle::Result clearBufferfi(const gl::Context *context,
                                GLenum buffer,
                                GLint drawbuffer,
                                GLfloat depth,
                                GLint stencil) override;

    GLenum getImplementationColorReadFormat(const gl::Context *context) const override;
    GLenum getImplementationColorReadType(const gl::Context *context) const override;
    angle::Result readPixels(const gl::Context *context,
                             const gl::Rectangle &area,
                             GLenum format,
                             GLenum type,
                             void *pixels) override;

    angle::Result blit(const gl::Context *context,
                       const gl::Rectangle &sourceArea,
                       const gl::Rectangle &destArea,
                       GLbitfield mask,
                       GLenum filter) override;

    bool checkStatus(const gl::Context *context) const override;

    angle::Result syncState(const gl::Context *context,
                            const gl::Framebuffer::DirtyBits &dirtyBits) override;

    angle::Result getSamplePosition(const gl::Context *context,
                                    size_t index,
                                    GLfloat *xy) const override;

    RenderTargetMtl *getColorReadRenderTarget() const;

    bool flipY() const { return mFlipY; }

    gl::Rectangle getCompleteRenderArea() const;

    const mtl::RenderPassDesc &getRenderPassDesc(ContextMtl *context);

    // Call this to notify FramebufferMtl whenever its render pass has started.
    void onStartedDrawingToFrameBuffer(const gl::Context *context);

    // The actual area will be adjusted based on framebuffer flipping property.
    gl::Rectangle getReadPixelArea(const gl::Rectangle &glArea);

    // NOTE: this method doesn't do the flipping of area. Caller must do it if needed before
    // callling this. See getReadPixelsArea().
    angle::Result readPixelsImpl(const gl::Context *context,
                                 const gl::Rectangle &area,
                                 const PackPixelsParams &packPixelsParams,
                                 RenderTargetMtl *renderTarget,
                                 uint8_t *pixels);

  private:
    void reset();
    angle::Result invalidateImpl(ContextMtl *contextMtl, size_t count, const GLenum *attachments);
    angle::Result clearImpl(const gl::Context *context,
                            gl::DrawBufferMask clearColorBuffers,
                            mtl::ClearRectParams *clearOpts);

    angle::Result clearWithLoadOp(const gl::Context *context,
                                  gl::DrawBufferMask clearColorBuffers,
                                  const mtl::ClearRectParams &clearOpts);

    angle::Result clearWithDraw(const gl::Context *context,
                                gl::DrawBufferMask clearColorBuffers,
                                const mtl::ClearRectParams &clearOpts);

    angle::Result prepareRenderPass(const gl::Context *context,
                                    gl::DrawBufferMask drawColorBuffers,
                                    mtl::RenderPassDesc *descOut);

    void overrideClearColor(const mtl::TextureRef &texture,
                            MTLClearColor clearColor,
                            MTLClearColor *colorOut);

    angle::Result updateColorRenderTarget(const gl::Context *context, size_t colorIndexGL);
    angle::Result updateDepthRenderTarget(const gl::Context *context);
    angle::Result updateStencilRenderTarget(const gl::Context *context);
    angle::Result updateCachedRenderTarget(const gl::Context *context,
                                           const gl::FramebufferAttachment *attachment,
                                           RenderTargetMtl **cachedRenderTarget);

    // NOTE: we cannot use RenderTargetCache here because it doesn't support separate
    // depth & stencil attachments as of now. Separate depth & stencil could be useful to
    // save spaces on iOS devices. See doc/PackedDepthStencilSupport.md.
    std::array<RenderTargetMtl *, mtl::kMaxRenderTargets> mColorRenderTargets;
    RenderTargetMtl *mDepthRenderTarget   = nullptr;
    RenderTargetMtl *mStencilRenderTarget = nullptr;
    mtl::RenderPassDesc mRenderPassDesc;
    const bool mFlipY = false;
};
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_FRAMEBUFFERMTL_H */
