//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferWgpu.h:
//    Defines the class interface for FramebufferWgpu, implementing FramebufferImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_FRAMEBUFFERWGPU_H_
#define LIBANGLE_RENDERER_WGPU_FRAMEBUFFERWGPU_H_

#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/RenderTargetCache.h"
#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"

namespace rx
{

class FramebufferWgpu : public FramebufferImpl
{
  public:
    FramebufferWgpu(const gl::FramebufferState &state);
    ~FramebufferWgpu() override;

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

    angle::Result readPixels(const gl::Context *context,
                             const gl::Rectangle &area,
                             GLenum format,
                             GLenum type,
                             const gl::PixelPackState &pack,
                             gl::Buffer *packBuffer,
                             void *pixels) override;

    angle::Result blit(const gl::Context *context,
                       const gl::Rectangle &sourceArea,
                       const gl::Rectangle &destArea,
                       GLbitfield mask,
                       GLenum filter) override;

    gl::FramebufferStatus checkStatus(const gl::Context *context) const override;

    angle::Result syncState(const gl::Context *context,
                            GLenum binding,
                            const gl::Framebuffer::DirtyBits &dirtyBits,
                            gl::Command command) override;

    angle::Result getSamplePosition(const gl::Context *context,
                                    size_t index,
                                    GLfloat *xy) const override;

    RenderTargetWgpu *getReadPixelsRenderTarget() const;

    void addNewColorAttachments(std::vector<wgpu::RenderPassColorAttachment> newColorAttachments);

    angle::Result flushOneColorAttachmentUpdate(const gl::Context *context,
                                                bool deferClears,
                                                uint32_t colorIndexGL);

    angle::Result flushColorAttachmentUpdates(const gl::Context *context,
                                              gl::DrawBufferMask dirtyColorAttachments,
                                              bool deferClears);

    angle::Result flushDeferredClears(ContextWgpu *contextWgpu);

    angle::Result startNewRenderPass(ContextWgpu *contextWgpu);

    const gl::DrawBuffersArray<wgpu::TextureFormat> &getCurrentColorAttachmentFormats() const
    {
        return mCurrentColorAttachmentFormats;
    }

    wgpu::TextureFormat getCurrentDepthStencilAttachmentFormat() const
    {
        return mCurrentDepthStencilFormat;
    }

  private:
    RenderTargetCache<RenderTargetWgpu> mRenderTargetCache;
    wgpu::RenderPassDescriptor mCurrentRenderPassDesc;
    std::vector<wgpu::RenderPassColorAttachment> mCurrentColorAttachments;
    gl::DrawBuffersArray<wgpu::TextureFormat> mCurrentColorAttachmentFormats;
    wgpu::TextureFormat mCurrentDepthStencilFormat = wgpu::TextureFormat::Undefined;

    // Secondary vector to track new clears that are added and should be run in a new render pass
    // during flushColorAttachmentUpdates.
    std::vector<wgpu::RenderPassColorAttachment> mNewColorAttachments;

    webgpu::ClearValuesArray mDeferredClears;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_FRAMEBUFFERWGPU_H_
