//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferGL.h: Defines the class interface for FramebufferGL.

#ifndef LIBANGLE_RENDERER_GL_FRAMEBUFFERGL_H_
#define LIBANGLE_RENDERER_GL_FRAMEBUFFERGL_H_

#include "libANGLE/renderer/FramebufferImpl.h"

namespace rx
{

class BlitGL;
class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;

class FramebufferGL : public FramebufferImpl
{
  public:
    FramebufferGL(const gl::FramebufferState &data,
                  const FunctionsGL *functions,
                  StateManagerGL *stateManager,
                  const WorkaroundsGL &workarounds,
                  BlitGL *blitter,
                  bool isDefault);
    // Constructor called when we need to create a FramebufferGL from an
    // existing framebuffer name, for example for the default framebuffer
    // on the Mac EGL CGL backend.
    FramebufferGL(GLuint id,
                  const gl::FramebufferState &data,
                  const FunctionsGL *functions,
                  const WorkaroundsGL &workarounds,
                  BlitGL *blitter,
                  StateManagerGL *stateManager);
    ~FramebufferGL() override;

    gl::Error discard(size_t count, const GLenum *attachments) override;
    gl::Error invalidate(size_t count, const GLenum *attachments) override;
    gl::Error invalidateSub(size_t count, const GLenum *attachments, const gl::Rectangle &area) override;

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

    void syncState(const gl::Framebuffer::DirtyBits &dirtyBits) override;

    void syncDrawState() const;

    GLuint getFramebufferID() const;

  private:
    void syncClearState(GLbitfield mask);
    void syncClearBufferState(GLenum buffer, GLint drawBuffer);

    gl::Error readPixelsRowByRowWorkaround(const gl::Rectangle &area,
                                           GLenum format,
                                           GLenum type,
                                           const gl::PixelPackState &pack,
                                           GLvoid *pixels) const;

    gl::Error readPixelsPaddingWorkaround(const gl::Rectangle &area,
                                          GLenum format,
                                          GLenum type,
                                          const gl::PixelPackState &pack,
                                          GLvoid *pixels) const;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;
    const WorkaroundsGL &mWorkarounds;
    BlitGL *mBlitter;

    GLuint mFramebufferID;
    bool mIsDefault;
};

}

#endif // LIBANGLE_RENDERER_GL_FRAMEBUFFERGL_H_
