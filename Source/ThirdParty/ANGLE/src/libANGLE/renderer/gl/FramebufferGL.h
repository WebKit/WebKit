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
class ClearMultiviewGL;
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
                  ClearMultiviewGL *multiviewClearer,
                  bool isDefault);
    // Constructor called when we need to create a FramebufferGL from an
    // existing framebuffer name, for example for the default framebuffer
    // on the Mac EGL CGL backend.
    FramebufferGL(GLuint id,
                  const gl::FramebufferState &data,
                  const FunctionsGL *functions,
                  const WorkaroundsGL &workarounds,
                  BlitGL *blitter,
                  ClearMultiviewGL *multiviewClearer,
                  StateManagerGL *stateManager);
    ~FramebufferGL() override;

    gl::Error discard(const gl::Context *context, size_t count, const GLenum *attachments) override;
    gl::Error invalidate(const gl::Context *context,
                         size_t count,
                         const GLenum *attachments) override;
    gl::Error invalidateSub(const gl::Context *context,
                            size_t count,
                            const GLenum *attachments,
                            const gl::Rectangle &area) override;

    gl::Error clear(const gl::Context *context, GLbitfield mask) override;
    gl::Error clearBufferfv(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLfloat *values) override;
    gl::Error clearBufferuiv(const gl::Context *context,
                             GLenum buffer,
                             GLint drawbuffer,
                             const GLuint *values) override;
    gl::Error clearBufferiv(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLint *values) override;
    gl::Error clearBufferfi(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            GLfloat depth,
                            GLint stencil) override;

    GLenum getImplementationColorReadFormat(const gl::Context *context) const override;
    GLenum getImplementationColorReadType(const gl::Context *context) const override;

    gl::Error readPixels(const gl::Context *context,
                         const gl::Rectangle &area,
                         GLenum format,
                         GLenum type,
                         void *pixels) override;

    gl::Error blit(const gl::Context *context,
                   const gl::Rectangle &sourceArea,
                   const gl::Rectangle &destArea,
                   GLbitfield mask,
                   GLenum filter) override;

    gl::Error getSamplePosition(size_t index, GLfloat *xy) const override;

    bool checkStatus(const gl::Context *context) const override;

    void syncState(const gl::Context *context,
                   const gl::Framebuffer::DirtyBits &dirtyBits) override;

    GLuint getFramebufferID() const;
    bool isDefault() const;

    void maskOutInactiveOutputDrawBuffers(gl::DrawBufferMask maxSet);

  private:
    void syncClearState(const gl::Context *context, GLbitfield mask);
    void syncClearBufferState(const gl::Context *context, GLenum buffer, GLint drawBuffer);

    bool modifyInvalidateAttachmentsForEmulatedDefaultFBO(
        size_t count,
        const GLenum *attachments,
        std::vector<GLenum> *modifiedAttachments) const;

    gl::Error readPixelsRowByRow(const gl::Context *context,
                                 const gl::Rectangle &area,
                                 GLenum format,
                                 GLenum type,
                                 const gl::PixelPackState &pack,
                                 GLubyte *pixels) const;

    gl::Error readPixelsAllAtOnce(const gl::Context *context,
                                  const gl::Rectangle &area,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelPackState &pack,
                                  GLubyte *pixels,
                                  bool readLastRowSeparately) const;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;
    const WorkaroundsGL &mWorkarounds;
    BlitGL *mBlitter;
    ClearMultiviewGL *mMultiviewClearer;

    GLuint mFramebufferID;
    bool mIsDefault;

    gl::DrawBufferMask mAppliedEnabledDrawBuffers;
};
}

#endif  // LIBANGLE_RENDERER_GL_FRAMEBUFFERGL_H_
