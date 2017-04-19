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
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace rx
{
class RenderTargetVk;
class WindowSurfaceVk;

class FramebufferVk : public FramebufferImpl, public ResourceVk
{
  public:
    // Factory methods so we don't have to use constructors with overloads.
    static FramebufferVk *CreateUserFBO(const gl::FramebufferState &state);

    // The passed-in SurfaceVK must be destroyed after this FBO is destroyed. Our Surface code is
    // ref-counted on the number of 'current' contexts, so we shouldn't get any dangling surface
    // references. See Surface::setIsCurrent(bool).
    static FramebufferVk *CreateDefaultFBO(const gl::FramebufferState &state,
                                           WindowSurfaceVk *backbuffer);

    ~FramebufferVk() override;
    void destroy(ContextImpl *contextImpl) override;
    void destroyDefault(DisplayImpl *displayImpl) override;

    gl::Error discard(size_t count, const GLenum *attachments) override;
    gl::Error invalidate(size_t count, const GLenum *attachments) override;
    gl::Error invalidateSub(size_t count,
                            const GLenum *attachments,
                            const gl::Rectangle &area) override;

    gl::Error clear(ContextImpl *context, GLbitfield mask) override;
    gl::Error clearBufferfv(ContextImpl *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLfloat *values) override;
    gl::Error clearBufferuiv(ContextImpl *context,
                             GLenum buffer,
                             GLint drawbuffer,
                             const GLuint *values) override;
    gl::Error clearBufferiv(ContextImpl *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLint *values) override;
    gl::Error clearBufferfi(ContextImpl *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            GLfloat depth,
                            GLint stencil) override;

    GLenum getImplementationColorReadFormat() const override;
    GLenum getImplementationColorReadType() const override;
    gl::Error readPixels(ContextImpl *context,
                         const gl::Rectangle &area,
                         GLenum format,
                         GLenum type,
                         GLvoid *pixels) const override;

    gl::Error blit(ContextImpl *context,
                   const gl::Rectangle &sourceArea,
                   const gl::Rectangle &destArea,
                   GLbitfield mask,
                   GLenum filter) override;

    bool checkStatus() const override;

    void syncState(ContextImpl *contextImpl, const gl::Framebuffer::DirtyBits &dirtyBits) override;

    gl::Error getSamplePosition(size_t index, GLfloat *xy) const override;

    gl::Error beginRenderPass(VkDevice device,
                              vk::CommandBuffer *commandBuffer,
                              Serial queueSerial,
                              const gl::State &glState);

    gl::ErrorOrResult<vk::RenderPass *> getRenderPass(VkDevice device);

  private:
    FramebufferVk(const gl::FramebufferState &state);
    FramebufferVk(const gl::FramebufferState &state, WindowSurfaceVk *backbuffer);

    gl::ErrorOrResult<vk::Framebuffer *> getFramebuffer(VkDevice device);

    WindowSurfaceVk *mBackbuffer;

    vk::RenderPass mRenderPass;
    vk::Framebuffer mFramebuffer;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_FRAMEBUFFERVK_H_
