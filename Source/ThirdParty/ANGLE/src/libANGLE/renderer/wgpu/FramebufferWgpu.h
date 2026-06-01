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
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

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

    void addNewColorAttachments(
        std::vector<webgpu::PackedRenderPassColorAttachment> newColorAttachments);

    void updateDepthStencilAttachment(
        webgpu::PackedRenderPassDepthStencilAttachment newRenderPassDepthStencilAttachment);

    angle::Result flushOneColorAttachmentUpdate(const gl::Context *context,
                                                bool deferClears,
                                                uint32_t colorIndexGL);

    angle::Result flushAttachmentUpdates(const gl::Context *context,
                                         gl::DrawBufferMask dirtyColorAttachments,
                                         bool dirtyDepthStencilAttachment,
                                         bool deferColorClears,
                                         bool deferDepthStencilClears);

    angle::Result flushDeferredClears(ContextWgpu *contextWgpu);

    // Starts a new render pass iff there are new color and/or depth/stencil attachments.
    angle::Result startRenderPassNewAttachments(ContextWgpu *contextWgpu);

    angle::Result startNewRenderPass(ContextWgpu *contextWgpu);

    // WGPU's Framebuffer coordinates are Y down. OpenGL's are Y up. This corrects the coordinates
    // of the read area based on the currently active RenderTarget.
    gl::Rectangle getReadArea(const gl::Context *context, const gl::Rectangle &glArea) const;

    const gl::DrawBuffersArray<WGPUTextureFormat> &getCurrentColorAttachmentFormats() const
    {
        return mCurrentColorAttachmentFormats;
    }

    WGPUTextureFormat getCurrentDepthStencilAttachmentFormat() const
    {
        return mCurrentDepthStencilFormat;
    }

    void setFlipY(bool flipY) { mFlipY = flipY; }
    bool flipY() const { return mFlipY; }

  private:
    gl::ColorF getClearColorWithCorrectAlpha(const gl::ColorF &clearValue, size_t drawBufferIndex);

    angle::Result clearImpl(const gl::Context *context,
                            gl::DrawBufferMask clearColorBuffers,
                            bool clearDepth,
                            bool clearStencil,
                            const gl::ColorF &clearColorValue,
                            float clearDepthValue,
                            uint32_t clearStencilValue);

    void mergeClearWithDeferredClears(const gl::ColorF &clearValue,
                                      gl::DrawBufferMask clearColorBuffers,
                                      float depthValue,
                                      uint32_t stencilValue,
                                      bool clearColor,
                                      bool clearDepth,
                                      bool clearStencil);

    bool formatsAndSizesMatchForDirectCopy(const gl::Context *context,
                                           const FramebufferWgpu *readFramebuffer,
                                           RenderTargetWgpu *readRenderTarget,
                                           RenderTargetWgpu *drawRenderTarget,
                                           const gl::Rectangle &sourceArea,
                                           const gl::Rectangle &destArea) const;

    angle::Result blitWithDirectCopy(ContextWgpu *contextWgpu,
                                     RenderTargetWgpu *readRenderTarget,
                                     RenderTargetWgpu *drawRenderTarget,
                                     const gl::Rectangle &sourceArea,
                                     const gl::Rectangle &destArea,
                                     bool srcFlipY,
                                     bool dstFlipY,
                                     WGPUTextureAspect aspect);

    RenderTargetCache<RenderTargetWgpu> mRenderTargetCache;
    webgpu::PackedRenderPassDescriptor mCurrentRenderPassDesc;

    gl::DrawBuffersArray<WGPUTextureFormat> mCurrentColorAttachmentFormats;
    WGPUTextureFormat mCurrentDepthStencilFormat = WGPUTextureFormat_Undefined;

    // Secondary descriptor to track new clears that are added and should be run in a new render
    // pass during flushColorAttachmentUpdates.
    std::optional<webgpu::PackedRenderPassDescriptor> mNewRenderPassDesc;

    webgpu::ClearValuesArray mDeferredClears;

    // Whether this framebuffer should be rendered to with a negated y coordinate.
    bool mFlipY = false;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_FRAMEBUFFERWGPU_H_
