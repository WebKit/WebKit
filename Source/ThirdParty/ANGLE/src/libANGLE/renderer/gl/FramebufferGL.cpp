//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferGL.cpp: Implements the class methods for FramebufferGL.

#include "libANGLE/renderer/gl/FramebufferGL.h"

#include "common/debug.h"
#include "libANGLE/Data.h"
#include "libANGLE/State.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/RenderbufferGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"

namespace rx
{

FramebufferGL::FramebufferGL(const gl::Framebuffer::Data &data, const FunctionsGL *functions, StateManagerGL *stateManager, bool isDefault)
    : FramebufferImpl(data),
      mFunctions(functions),
      mStateManager(stateManager),
      mFramebufferID(0)
{
    if (!isDefault)
    {
        mFunctions->genFramebuffers(1, &mFramebufferID);
    }
}

FramebufferGL::~FramebufferGL()
{
    if (mFramebufferID != 0)
    {
        mFunctions->deleteFramebuffers(1, &mFramebufferID);
        mFramebufferID = 0;
    }
}

static void BindFramebufferAttachment(const FunctionsGL *functions, GLenum attachmentPoint,
                                      const gl::FramebufferAttachment *attachment)
{
    if (attachment)
    {
        if (attachment->type() == GL_TEXTURE)
        {
            const gl::Texture *texture = attachment->getTexture();
            const TextureGL *textureGL = GetImplAs<TextureGL>(texture);

            if (texture->getTarget() == GL_TEXTURE_2D)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, GL_TEXTURE_2D,
                                                textureGL->getTextureID(), attachment->mipLevel());
            }
            else if (texture->getTarget() == GL_TEXTURE_CUBE_MAP)
            {
                functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, attachment->cubeMapFace(),
                                                textureGL->getTextureID(), attachment->mipLevel());
            }
            else if (texture->getTarget() == GL_TEXTURE_2D_ARRAY || texture->getTarget() == GL_TEXTURE_3D)
            {
                functions->framebufferTextureLayer(GL_FRAMEBUFFER, attachmentPoint, textureGL->getTextureID(),
                                                   attachment->mipLevel(), attachment->layer());
            }
            else
            {
                UNREACHABLE();
            }
        }
        else if (attachment->type() == GL_RENDERBUFFER)
        {
            const gl::Renderbuffer *renderbuffer = attachment->getRenderbuffer();
            const RenderbufferGL *renderbufferGL = GetImplAs<RenderbufferGL>(renderbuffer);

            functions->framebufferRenderbuffer(GL_FRAMEBUFFER, attachmentPoint, GL_RENDERBUFFER,
                                               renderbufferGL->getRenderbufferID());
        }
        else
        {
            UNREACHABLE();
        }
    }
    else
    {
        // Unbind this attachment
        functions->framebufferTexture2D(GL_FRAMEBUFFER, attachmentPoint, GL_TEXTURE_2D, 0, 0);
    }
}

void FramebufferGL::onUpdateColorAttachment(size_t index)
{
    if (mFramebufferID != 0)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        BindFramebufferAttachment(mFunctions,
                                  GL_COLOR_ATTACHMENT0 + index,
                                  mData.getColorAttachment(static_cast<unsigned int>(index)));
    }
}

void FramebufferGL::onUpdateDepthAttachment()
{
    if (mFramebufferID != 0)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        BindFramebufferAttachment(mFunctions,
                                  GL_DEPTH_ATTACHMENT,
                                  mData.getDepthAttachment());
    }
}

void FramebufferGL::onUpdateStencilAttachment()
{
    if (mFramebufferID != 0)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        BindFramebufferAttachment(mFunctions,
                                  GL_STENCIL_ATTACHMENT,
                                  mData.getStencilAttachment());
    }
}

void FramebufferGL::onUpdateDepthStencilAttachment()
{
    if (mFramebufferID != 0)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        BindFramebufferAttachment(mFunctions,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  mData.getDepthStencilAttachment());
    }
}

void FramebufferGL::setDrawBuffers(size_t count, const GLenum *buffers)
{
    if (mFramebufferID != 0)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->drawBuffers(count, buffers);
    }
}

void FramebufferGL::setReadBuffer(GLenum buffer)
{
    if (mFramebufferID != 0)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->readBuffer(buffer);
    }
}

gl::Error FramebufferGL::invalidate(size_t count, const GLenum *attachments)
{
    // Since this function is just a hint and not available until OpenGL 4.3, only call it if it is available.
    if (mFunctions->invalidateFramebuffer)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->invalidateFramebuffer(GL_FRAMEBUFFER, count, attachments);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error FramebufferGL::invalidateSub(size_t count, const GLenum *attachments, const gl::Rectangle &area)
{
    // Since this function is just a hint and not available until OpenGL 4.3, only call it if it is available.
    if (mFunctions->invalidateSubFramebuffer)
    {
        mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
        mFunctions->invalidateSubFramebuffer(GL_FRAMEBUFFER, count, attachments, area.x, area.y, area.width, area.height);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error FramebufferGL::clear(const gl::Data &data, GLbitfield mask)
{
    mStateManager->setClearState(*data.state, mask);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clear(mask);

    return gl::Error(GL_NO_ERROR);
}

static GLbitfield GetClearBufferMask(GLenum buffer)
{
    switch (buffer)
    {
      case GL_COLOR:          return GL_COLOR_BUFFER_BIT;
      case GL_DEPTH:          return GL_DEPTH_BUFFER_BIT;
      case GL_STENCIL:        return GL_STENCIL_BUFFER_BIT;
      case GL_DEPTH_STENCIL:  return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
      default: UNREACHABLE(); return 0;
    }
}

gl::Error FramebufferGL::clearBufferfv(const gl::State &state, GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    mStateManager->setClearState(state, GetClearBufferMask(buffer));
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferfv(buffer, drawbuffer, values);

    return gl::Error(GL_NO_ERROR);
}

gl::Error FramebufferGL::clearBufferuiv(const gl::State &state, GLenum buffer, GLint drawbuffer, const GLuint *values)
{
    mStateManager->setClearState(state, GetClearBufferMask(buffer));
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferuiv(buffer, drawbuffer, values);

    return gl::Error(GL_NO_ERROR);
}

gl::Error FramebufferGL::clearBufferiv(const gl::State &state, GLenum buffer, GLint drawbuffer, const GLint *values)
{
    mStateManager->setClearState(state, GetClearBufferMask(buffer));
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferiv(buffer, drawbuffer, values);

    return gl::Error(GL_NO_ERROR);
}

gl::Error FramebufferGL::clearBufferfi(const gl::State &state, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    mStateManager->setClearState(state, GetClearBufferMask(buffer));
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctions->clearBufferfi(buffer, drawbuffer, depth, stencil);

    return gl::Error(GL_NO_ERROR);
}

GLenum FramebufferGL::getImplementationColorReadFormat() const
{
    const gl::FramebufferAttachment *readAttachment = getData().getReadAttachment();
    GLenum internalFormat = readAttachment->getInternalFormat();
    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);
    return internalFormatInfo.format;
}

GLenum FramebufferGL::getImplementationColorReadType() const
{
    const gl::FramebufferAttachment *readAttachment = getData().getReadAttachment();
    GLenum internalFormat = readAttachment->getInternalFormat();
    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);
    return internalFormatInfo.type;
}

gl::Error FramebufferGL::readPixels(const gl::State &state, const gl::Rectangle &area, GLenum format, GLenum type, GLvoid *pixels) const
{
    const gl::PixelPackState &packState = state.getPackState();

    // TODO: set pack state
    if (packState.rowLength != 0 || packState.skipRows != 0 || packState.skipPixels != 0)
    {
        UNIMPLEMENTED();
        return gl::Error(GL_INVALID_OPERATION, "invalid pixel store parameters in readPixels");
    }

    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, mFramebufferID);
    mFunctions->readPixels(area.x, area.y, area.width, area.height, format, type, pixels);

    return gl::Error(GL_NO_ERROR);
}

gl::Error FramebufferGL::blit(const gl::State &state, const gl::Rectangle &sourceArea, const gl::Rectangle &destArea,
                              GLbitfield mask, GLenum filter, const gl::Framebuffer *sourceFramebuffer)
{
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(sourceFramebuffer);

    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());
    mStateManager->bindFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebufferID);

    mFunctions->blitFramebuffer(sourceArea.x, sourceArea.y, sourceArea.x + sourceArea.width, sourceArea.y + sourceArea.height,
                                destArea.x, destArea.y, destArea.x + destArea.width, destArea.y + destArea.height,
                                mask, filter);

    return gl::Error(GL_NO_ERROR);
}

GLenum FramebufferGL::checkStatus() const
{
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    return mFunctions->checkFramebufferStatus(GL_FRAMEBUFFER);
}

GLuint FramebufferGL::getFramebufferID() const
{
    return mFramebufferID;
}

}
