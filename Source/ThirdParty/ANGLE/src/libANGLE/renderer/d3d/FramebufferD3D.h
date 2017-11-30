//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferD3D.h: Defines the DefaultAttachmentD3D and FramebufferD3D classes.

#ifndef LIBANGLE_RENDERER_D3D_FRAMBUFFERD3D_H_
#define LIBANGLE_RENDERER_D3D_FRAMBUFFERD3D_H_

#include <cstdint>
#include <vector>

#include "common/Color.h"
#include "common/Optional.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/FramebufferImpl.h"

namespace gl
{
class FramebufferAttachment;
struct PixelPackState;

typedef std::vector<const FramebufferAttachment *> AttachmentList;
}

namespace rx
{
class RendererD3D;
class RenderTargetD3D;

struct ClearParameters
{
    ClearParameters();
    ClearParameters(const ClearParameters &other);

    bool clearColor[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
    gl::ColorF colorF;
    gl::ColorI colorI;
    gl::ColorUI colorUI;
    GLenum colorType;
    bool colorMaskRed;
    bool colorMaskGreen;
    bool colorMaskBlue;
    bool colorMaskAlpha;

    bool clearDepth;
    float depthValue;

    bool clearStencil;
    GLint stencilValue;
    GLuint stencilWriteMask;

    bool scissorEnabled;
    gl::Rectangle scissor;
};

class FramebufferD3D : public FramebufferImpl
{
  public:
    FramebufferD3D(const gl::FramebufferState &data, RendererD3D *renderer);
    ~FramebufferD3D() override;

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

    bool checkStatus(const gl::Context *context) const override;

    void syncState(const gl::Context *context,
                   const gl::Framebuffer::DirtyBits &dirtyBits) override;

    const gl::AttachmentList &getColorAttachmentsForRender(const gl::Context *context);

  private:
    virtual gl::Error clearImpl(const gl::Context *context, const ClearParameters &clearParams) = 0;

    virtual gl::Error readPixelsImpl(const gl::Context *context,
                                     const gl::Rectangle &area,
                                     GLenum format,
                                     GLenum type,
                                     size_t outputPitch,
                                     const gl::PixelPackState &pack,
                                     uint8_t *pixels) = 0;

    virtual gl::Error blitImpl(const gl::Context *context,
                               const gl::Rectangle &sourceArea,
                               const gl::Rectangle &destArea,
                               const gl::Rectangle *scissor,
                               bool blitRenderTarget,
                               bool blitDepth,
                               bool blitStencil,
                               GLenum filter,
                               const gl::Framebuffer *sourceFramebuffer) = 0;

    virtual GLenum getRenderTargetImplementationFormat(RenderTargetD3D *renderTarget) const = 0;

    RendererD3D *mRenderer;
    Optional<gl::AttachmentList> mColorAttachmentsForRender;
    gl::DrawBufferMask mCurrentActiveProgramOutputs;
};
}

#endif  // LIBANGLE_RENDERER_D3D_FRAMBUFFERD3D_H_
