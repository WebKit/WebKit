//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferD3D.h: Defines the DefaultAttachmentD3D and FramebufferD3D classes.

#ifndef LIBANGLE_RENDERER_D3D_FRAMBUFFERD3D_H_
#define LIBANGLE_RENDERER_D3D_FRAMBUFFERD3D_H_

#include <vector>
#include <cstdint>

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
struct WorkaroundsD3D;

struct ClearParameters
{
    bool clearColor[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
    gl::ColorF colorFClearValue;
    gl::ColorI colorIClearValue;
    gl::ColorUI colorUIClearValue;
    GLenum colorClearType;
    bool colorMaskRed;
    bool colorMaskGreen;
    bool colorMaskBlue;
    bool colorMaskAlpha;

    bool clearDepth;
    float depthClearValue;

    bool clearStencil;
    GLint stencilClearValue;
    GLuint stencilWriteMask;

    bool scissorEnabled;
    gl::Rectangle scissor;
};

class FramebufferD3D : public FramebufferImpl
{
  public:
    FramebufferD3D(const gl::Framebuffer::Data &data, RendererD3D *renderer);
    virtual ~FramebufferD3D();

    gl::Error clear(const gl::Data &data, GLbitfield mask) override;
    gl::Error clearBufferfv(const gl::Data &data,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLfloat *values) override;
    gl::Error clearBufferuiv(const gl::Data &data,
                             GLenum buffer,
                             GLint drawbuffer,
                             const GLuint *values) override;
    gl::Error clearBufferiv(const gl::Data &data,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLint *values) override;
    gl::Error clearBufferfi(const gl::Data &data,
                            GLenum buffer,
                            GLint drawbuffer,
                            GLfloat depth,
                            GLint stencil) override;

    GLenum getImplementationColorReadFormat() const override;
    GLenum getImplementationColorReadType() const override;
    gl::Error readPixels(const gl::State &state, const gl::Rectangle &area, GLenum format, GLenum type, GLvoid *pixels) const override;

    gl::Error blit(const gl::State &state, const gl::Rectangle &sourceArea, const gl::Rectangle &destArea,
                   GLbitfield mask, GLenum filter, const gl::Framebuffer *sourceFramebuffer) override;

    bool checkStatus() const override;

    void syncState(const gl::Framebuffer::DirtyBits &dirtyBits) override;

    const gl::AttachmentList &getColorAttachmentsForRender() const;

  private:
    virtual gl::Error clear(const gl::Data &data, const ClearParameters &clearParams) = 0;

    virtual gl::Error readPixelsImpl(const gl::Rectangle &area,
                                     GLenum format,
                                     GLenum type,
                                     size_t outputPitch,
                                     const gl::PixelPackState &pack,
                                     uint8_t *pixels) const = 0;

    virtual gl::Error blit(const gl::Rectangle &sourceArea, const gl::Rectangle &destArea, const gl::Rectangle *scissor,
                           bool blitRenderTarget, bool blitDepth, bool blitStencil, GLenum filter,
                           const gl::Framebuffer *sourceFramebuffer) = 0;

    virtual GLenum getRenderTargetImplementationFormat(RenderTargetD3D *renderTarget) const = 0;

    RendererD3D *mRenderer;
    Optional<gl::AttachmentList> mColorAttachmentsForRender;
};

}

#endif // LIBANGLE_RENDERER_D3D_FRAMBUFFERD3D_H_
