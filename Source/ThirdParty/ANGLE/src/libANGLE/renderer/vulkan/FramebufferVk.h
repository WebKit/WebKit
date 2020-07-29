//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferVk.h:
//    Defines the class interface for FramebufferVk, implementing FramebufferImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_

#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/RenderTargetCache.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/UtilsVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"

namespace rx
{
class RendererVk;
class RenderTargetVk;
class WindowSurfaceVk;

class FramebufferVk : public FramebufferImpl
{
  public:
    // Factory methods so we don't have to use constructors with overloads.
    static FramebufferVk *CreateUserFBO(RendererVk *renderer, const gl::FramebufferState &state);

    // The passed-in SurfaceVK must be destroyed after this FBO is destroyed. Our Surface code is
    // ref-counted on the number of 'current' contexts, so we shouldn't get any dangling surface
    // references. See Surface::setIsCurrent(bool).
    static FramebufferVk *CreateDefaultFBO(RendererVk *renderer,
                                           const gl::FramebufferState &state,
                                           WindowSurfaceVk *backbuffer);

    ~FramebufferVk() override;
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

    const gl::InternalFormat &getImplementationColorReadFormat(
        const gl::Context *context) const override;
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

    bool checkStatus(const gl::Context *context) const override;

    angle::Result syncState(const gl::Context *context,
                            GLenum binding,
                            const gl::Framebuffer::DirtyBits &dirtyBits) override;

    angle::Result getSamplePosition(const gl::Context *context,
                                    size_t index,
                                    GLfloat *xy) const override;
    RenderTargetVk *getDepthStencilRenderTarget() const;

    // Internal helper function for readPixels operations.
    angle::Result readPixelsImpl(ContextVk *contextVk,
                                 const gl::Rectangle &area,
                                 const PackPixelsParams &packPixelsParams,
                                 VkImageAspectFlagBits copyAspectFlags,
                                 RenderTargetVk *renderTarget,
                                 void *pixels);

    gl::Extents getReadImageExtents() const;
    gl::Rectangle getNonRotatedCompleteRenderArea() const;
    gl::Rectangle getRotatedCompleteRenderArea(ContextVk *contextVk) const;
    gl::Rectangle getRotatedScissoredRenderArea(ContextVk *contextVk) const;

    const gl::DrawBufferMask &getEmulatedAlphaAttachmentMask() const;
    RenderTargetVk *getColorDrawRenderTarget(size_t colorIndex) const;
    RenderTargetVk *getColorReadRenderTarget() const;

    angle::Result startNewRenderPass(ContextVk *contextVk,
                                     const gl::Rectangle &renderArea,
                                     vk::CommandBuffer **commandBufferOut);

    RenderTargetVk *getFirstRenderTarget() const;
    GLint getSamples() const;

    const vk::RenderPassDesc &getRenderPassDesc() const { return mRenderPassDesc; }
    const vk::FramebufferDesc &getFramebufferDesc() const { return mCurrentFramebufferDesc; }
    // We only support depth/stencil packed format and depthstencil attachment always follow all
    // color attachments
    size_t getDepthStencilAttachmentIndexVk() const
    {
        return getState().getEnabledDrawBuffers().count();
    }

  private:
    FramebufferVk(RendererVk *renderer,
                  const gl::FramebufferState &state,
                  WindowSurfaceVk *backbuffer);

    // The 'in' rectangles must be clipped to the scissor and FBO. The clipping is done in 'blit'.
    angle::Result blitWithCommand(ContextVk *contextVk,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Rectangle &destArea,
                                  RenderTargetVk *readRenderTarget,
                                  RenderTargetVk *drawRenderTarget,
                                  GLenum filter,
                                  bool colorBlit,
                                  bool depthBlit,
                                  bool stencilBlit,
                                  bool flipX,
                                  bool flipY);

    // Resolve color with vkCmdResolveImage
    angle::Result resolveColorWithCommand(ContextVk *contextVk,
                                          const UtilsVk::BlitResolveParameters &params,
                                          vk::ImageHelper *srcImage);

    angle::Result getFramebuffer(ContextVk *contextVk, vk::Framebuffer **framebufferOut);

    angle::Result clearImpl(const gl::Context *context,
                            gl::DrawBufferMask clearColorBuffers,
                            bool clearDepth,
                            bool clearStencil,
                            const VkClearColorValue &clearColorValue,
                            const VkClearDepthStencilValue &clearDepthStencilValue);

    angle::Result clearImmediatelyWithRenderPassOp(
        ContextVk *contextVk,
        const gl::Rectangle &clearArea,
        gl::DrawBufferMask clearColorBuffers,
        bool clearDepth,
        bool clearStencil,
        const VkClearColorValue &clearColorValue,
        const VkClearDepthStencilValue &clearDepthStencilValue);

    angle::Result clearWithDraw(ContextVk *contextVk,
                                const gl::Rectangle &clearArea,
                                gl::DrawBufferMask clearColorBuffers,
                                bool clearDepth,
                                bool clearStencil,
                                VkColorComponentFlags colorMaskFlags,
                                uint8_t stencilMask,
                                const VkClearColorValue &clearColorValue,
                                const VkClearDepthStencilValue &clearDepthStencilValue);
    void clearWithRenderPassOp(gl::DrawBufferMask clearColorBuffers,
                               bool clearDepth,
                               bool clearStencil,
                               const VkClearColorValue &clearColorValue,
                               const VkClearDepthStencilValue &clearDepthStencilValue);
    void clearWithClearAttachment(vk::CommandBuffer *renderPassCommandBuffer,
                                  const gl::Rectangle &scissoredRenderArea,
                                  gl::DrawBufferMask clearColorBuffers,
                                  bool clearDepth,
                                  bool clearStencil,
                                  const VkClearColorValue &clearColorValue,
                                  const VkClearDepthStencilValue &clearDepthStencilValue);
    void updateActiveColorMasks(size_t colorIndex, bool r, bool g, bool b, bool a);
    void updateRenderPassDesc();
    angle::Result updateColorAttachment(const gl::Context *context,
                                        bool deferClears,
                                        uint32_t colorIndex);
    angle::Result invalidateImpl(ContextVk *contextVk,
                                 size_t count,
                                 const GLenum *attachments,
                                 bool isSubInvalidate);
    // Release all FramebufferVk objects in the cache and clear cache
    void clearCache(ContextVk *contextVk);
    angle::Result updateDepthStencilAttachment(const gl::Context *context, bool deferClears);
    void updateDepthStencilAttachmentSerial(ContextVk *contextVk);

    RenderTargetVk *getReadPixelsRenderTarget(GLenum format) const;
    VkImageAspectFlagBits getReadPixelsAspectFlags(GLenum format) const;

    angle::Result flushDeferredClears(ContextVk *contextVk, const gl::Rectangle &renderArea);
    VkClearValue getCorrectedColorClearValue(size_t colorIndexGL,
                                             const VkClearColorValue &clearColor) const;

    WindowSurfaceVk *mBackbuffer;

    vk::RenderPassDesc mRenderPassDesc;
    vk::FramebufferHelper *mFramebuffer;
    RenderTargetCache<RenderTargetVk> mRenderTargetCache;

    // These two variables are used to quickly compute if we need to do a masked clear. If a color
    // channel is masked out, we check against the Framebuffer Attachments (RenderTargets) to see
    // if the masked out channel is present in any of the attachments.
    VkColorComponentFlags mActiveColorComponents;
    gl::DrawBufferMask mActiveColorComponentMasksForClear[4];
    vk::DynamicBuffer mReadPixelBuffer;

    // When we draw to the framebuffer, and the real format has an alpha channel but the format of
    // the framebuffer does not, we need to mask out the alpha channel. This DrawBufferMask will
    // contain the mask to apply to the alpha channel when drawing.
    gl::DrawBufferMask mEmulatedAlphaAttachmentMask;

    vk::FramebufferDesc mCurrentFramebufferDesc;
    std::unordered_map<vk::FramebufferDesc, vk::FramebufferHelper> mFramebufferCache;
    bool mSupportDepthStencilFeedbackLoops;

    vk::ClearValuesArray mDeferredClears;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_
