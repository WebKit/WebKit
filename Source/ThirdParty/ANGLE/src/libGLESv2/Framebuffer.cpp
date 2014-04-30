#include "precompiled.h"
//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libGLESv2/Framebuffer.h"

#include "libGLESv2/main.h"
#include "common/utilities.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/Renderbuffer.h"

namespace gl
{

Framebuffer::Framebuffer(rx::Renderer *renderer)
    : mRenderer(renderer)
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        mDrawBufferStates[colorAttachment] = GL_NONE;
    }
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
    mReadBufferState = GL_COLOR_ATTACHMENT0_EXT;
}

Framebuffer::~Framebuffer()
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        mColorbuffers[colorAttachment].set(NULL, GL_NONE, 0, 0);
    }
    mDepthbuffer.set(NULL, GL_NONE, 0, 0);
    mStencilbuffer.set(NULL, GL_NONE, 0, 0);
}

Renderbuffer *Framebuffer::lookupRenderbuffer(GLenum type, GLuint handle, GLint level, GLint layer) const
{
    gl::Context *context = gl::getContext();

    switch (type)
    {
      case GL_NONE:
        return NULL;

      case GL_RENDERBUFFER:
        return context->getRenderbuffer(handle);

      case GL_TEXTURE_2D:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_2D)
            {
                return static_cast<Texture2D*>(texture)->getRenderbuffer(level);
            }
            else
            {
                return NULL;
            }
        }

      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_CUBE_MAP)
            {
                return static_cast<TextureCubeMap*>(texture)->getRenderbuffer(type, level);
            }
            else
            {
                return NULL;
            }
        }

      case GL_TEXTURE_3D:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_3D)
            {
                return static_cast<Texture3D*>(texture)->getRenderbuffer(level, layer);
            }
            else
            {
                return NULL;
            }
        }

      case GL_TEXTURE_2D_ARRAY:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_2D_ARRAY)
            {
                return static_cast<Texture2DArray*>(texture)->getRenderbuffer(level, layer);
            }
            else
            {
                return NULL;
            }
        }

      default:
        UNREACHABLE();
        return NULL;
    }
}

void Framebuffer::setColorbuffer(unsigned int colorAttachment, GLenum type, GLuint colorbuffer, GLint level, GLint layer)
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    Renderbuffer *renderBuffer = lookupRenderbuffer(type, colorbuffer, level, layer);
    if (renderBuffer)
    {
        mColorbuffers[colorAttachment].set(renderBuffer, type, level, layer);
    }
    else
    {
        mColorbuffers[colorAttachment].set(NULL, GL_NONE, 0, 0);
    }
}

void Framebuffer::setDepthbuffer(GLenum type, GLuint depthbuffer, GLint level, GLint layer)
{
    Renderbuffer *renderBuffer = lookupRenderbuffer(type, depthbuffer, level, layer);
    if (renderBuffer)
    {
        mDepthbuffer.set(renderBuffer, type, level, layer);
    }
    else
    {
        mDepthbuffer.set(NULL, GL_NONE, 0, 0);
    }
}

void Framebuffer::setStencilbuffer(GLenum type, GLuint stencilbuffer, GLint level, GLint layer)
{
    Renderbuffer *renderBuffer = lookupRenderbuffer(type, stencilbuffer, level, layer);
    if (renderBuffer)
    {
        mStencilbuffer.set(renderBuffer, type, level, layer);
    }
    else
    {
        mStencilbuffer.set(NULL, GL_NONE, 0, 0);
    }
}

void Framebuffer::setDepthStencilBuffer(GLenum type, GLuint depthStencilBuffer, GLint level, GLint layer)
{
    Renderbuffer *renderBuffer = lookupRenderbuffer(type, depthStencilBuffer, level, layer);
    if (renderBuffer && renderBuffer->getDepthSize() > 0 && renderBuffer->getStencilSize() > 0)
    {
        mDepthbuffer.set(renderBuffer, type, level, layer);
        mStencilbuffer.set(renderBuffer, type, level, layer);
    }
    else
    {
        mDepthbuffer.set(NULL, GL_NONE, 0, 0);
        mStencilbuffer.set(NULL, GL_NONE, 0, 0);
    }
}

void Framebuffer::detachTexture(GLuint texture)
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (mColorbuffers[colorAttachment].id() == texture &&
            IsInternalTextureTarget(mColorbuffers[colorAttachment].type(), mRenderer->getCurrentClientVersion()))
        {
            mColorbuffers[colorAttachment].set(NULL, GL_NONE, 0, 0);
        }
    }

    if (mDepthbuffer.id() == texture && IsInternalTextureTarget(mDepthbuffer.type(), mRenderer->getCurrentClientVersion()))
    {
        mDepthbuffer.set(NULL, GL_NONE, 0, 0);
    }

    if (mStencilbuffer.id() == texture && IsInternalTextureTarget(mStencilbuffer.type(), mRenderer->getCurrentClientVersion()))
    {
        mStencilbuffer.set(NULL, GL_NONE, 0, 0);
    }
}

void Framebuffer::detachRenderbuffer(GLuint renderbuffer)
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (mColorbuffers[colorAttachment].id() == renderbuffer && mColorbuffers[colorAttachment].type() == GL_RENDERBUFFER)
        {
            mColorbuffers[colorAttachment].set(NULL, GL_NONE, 0, 0);
        }
    }

    if (mDepthbuffer.id() == renderbuffer && mDepthbuffer.type() == GL_RENDERBUFFER)
    {
        mDepthbuffer.set(NULL, GL_NONE, 0, 0);
    }

    if (mStencilbuffer.id() == renderbuffer && mStencilbuffer.type() == GL_RENDERBUFFER)
    {
        mStencilbuffer.set(NULL, GL_NONE, 0, 0);
    }
}

unsigned int Framebuffer::getRenderTargetSerial(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);

    Renderbuffer *colorbuffer = mColorbuffers[colorAttachment].get();

    if (colorbuffer)
    {
        return colorbuffer->getSerial();
    }

    return 0;
}

unsigned int Framebuffer::getDepthbufferSerial() const
{
    Renderbuffer *depthbuffer = mDepthbuffer.get();

    if (depthbuffer)
    {
        return depthbuffer->getSerial();
    }

    return 0;
}

unsigned int Framebuffer::getStencilbufferSerial() const
{
    Renderbuffer *stencilbuffer = mStencilbuffer.get();

    if (stencilbuffer)
    {
        return stencilbuffer->getSerial();
    }

    return 0;
}

Renderbuffer *Framebuffer::getColorbuffer(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return mColorbuffers[colorAttachment].get();
}

Renderbuffer *Framebuffer::getDepthbuffer() const
{
    return mDepthbuffer.get();
}

Renderbuffer *Framebuffer::getStencilbuffer() const
{
    return mStencilbuffer.get();
}

Renderbuffer *Framebuffer::getDepthStencilBuffer() const
{
    return (mDepthbuffer.id() == mStencilbuffer.id()) ? mDepthbuffer.get() : NULL;
}

Renderbuffer *Framebuffer::getDepthOrStencilbuffer() const
{
    Renderbuffer *depthstencilbuffer = mDepthbuffer.get();
    
    if (!depthstencilbuffer)
    {
        depthstencilbuffer = mStencilbuffer.get();
    }

    return depthstencilbuffer;
}

Renderbuffer *Framebuffer::getReadColorbuffer() const
{
    // Will require more logic if glReadBuffers is supported
    return mColorbuffers[0].get();
}

GLenum Framebuffer::getReadColorbufferType() const
{
    // Will require more logic if glReadBuffers is supported
    return mColorbuffers[0].type();
}

Renderbuffer *Framebuffer::getFirstColorbuffer() const
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (mColorbuffers[colorAttachment].type() != GL_NONE)
        {
            return mColorbuffers[colorAttachment].get();
        }
    }

    return NULL;
}

GLenum Framebuffer::getColorbufferType(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return mColorbuffers[colorAttachment].type();
}

GLenum Framebuffer::getDepthbufferType() const
{
    return mDepthbuffer.type();
}

GLenum Framebuffer::getStencilbufferType() const
{
    return mStencilbuffer.type();
}

GLenum Framebuffer::getDepthStencilbufferType() const
{
    return (mDepthbuffer.id() == mStencilbuffer.id()) ? mDepthbuffer.type() : GL_NONE;
}

GLuint Framebuffer::getColorbufferHandle(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return mColorbuffers[colorAttachment].id();
}

GLuint Framebuffer::getDepthbufferHandle() const
{
    return mDepthbuffer.id();
}

GLuint Framebuffer::getStencilbufferHandle() const
{
    return mStencilbuffer.id();
}

GLenum Framebuffer::getDepthStencilbufferHandle() const
{
    return (mDepthbuffer.id() == mStencilbuffer.id()) ? mDepthbuffer.id() : 0;
}

GLenum Framebuffer::getColorbufferMipLevel(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return mColorbuffers[colorAttachment].mipLevel();
}

GLenum Framebuffer::getDepthbufferMipLevel() const
{
    return mDepthbuffer.mipLevel();
}

GLenum Framebuffer::getStencilbufferMipLevel() const
{
    return mStencilbuffer.mipLevel();
}

GLenum Framebuffer::getDepthStencilbufferMipLevel() const
{
    return (mDepthbuffer.id() == mStencilbuffer.id()) ? mDepthbuffer.mipLevel() : 0;
}

GLenum Framebuffer::getColorbufferLayer(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return mColorbuffers[colorAttachment].layer();
}

GLenum Framebuffer::getDepthbufferLayer() const
{
    return mDepthbuffer.layer();
}

GLenum Framebuffer::getStencilbufferLayer() const
{
    return mStencilbuffer.layer();
}

GLenum Framebuffer::getDepthStencilbufferLayer() const
{
    return (mDepthbuffer.id() == mStencilbuffer.id()) ? mDepthbuffer.layer() : 0;
}

GLenum Framebuffer::getDrawBufferState(unsigned int colorAttachment) const
{
    return mDrawBufferStates[colorAttachment];
}

void Framebuffer::setDrawBufferState(unsigned int colorAttachment, GLenum drawBuffer)
{
    mDrawBufferStates[colorAttachment] = drawBuffer;
}

bool Framebuffer::isEnabledColorAttachment(unsigned int colorAttachment) const
{
    return (mColorbuffers[colorAttachment].type() != GL_NONE && mDrawBufferStates[colorAttachment] != GL_NONE);
}

bool Framebuffer::hasEnabledColorAttachment() const
{
    for (unsigned int colorAttachment = 0; colorAttachment < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (isEnabledColorAttachment(colorAttachment))
        {
            return true;
        }
    }

    return false;
}

bool Framebuffer::hasStencil() const
{
    if (mStencilbuffer.type() != GL_NONE)
    {
        const Renderbuffer *stencilbufferObject = getStencilbuffer();

        if (stencilbufferObject)
        {
            return stencilbufferObject->getStencilSize() > 0;
        }
    }

    return false;
}

bool Framebuffer::usingExtendedDrawBuffers() const
{
    for (unsigned int colorAttachment = 1; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (isEnabledColorAttachment(colorAttachment))
        {
            return true;
        }
    }

    return false;
}

GLenum Framebuffer::completeness() const
{
    int width = 0;
    int height = 0;
    unsigned int colorbufferSize = 0;
    int samples = -1;
    bool missingAttachment = true;
    GLuint clientVersion = mRenderer->getCurrentClientVersion();

    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (mColorbuffers[colorAttachment].type() != GL_NONE)
        {
            const Renderbuffer *colorbuffer = getColorbuffer(colorAttachment);

            if (!colorbuffer)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (colorbuffer->getWidth() == 0 || colorbuffer->getHeight() == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (mColorbuffers[colorAttachment].type() == GL_RENDERBUFFER)
            {
                if (!gl::IsColorRenderingSupported(colorbuffer->getInternalFormat(), mRenderer))
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }
            else if (IsInternalTextureTarget(mColorbuffers[colorAttachment].type(), mRenderer->getCurrentClientVersion()))
            {
                GLenum internalformat = colorbuffer->getInternalFormat();

                if (!gl::IsColorRenderingSupported(internalformat, mRenderer))
                {
                    return GL_FRAMEBUFFER_UNSUPPORTED;
                }

                if (gl::GetDepthBits(internalformat, clientVersion) > 0 ||
                    gl::GetStencilBits(internalformat, clientVersion) > 0)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }
            else
            {
                UNREACHABLE();
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!missingAttachment)
            {
                // all color attachments must have the same width and height
                if (colorbuffer->getWidth() != width || colorbuffer->getHeight() != height)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
                }

                // APPLE_framebuffer_multisample, which EXT_draw_buffers refers to, requires that
                // all color attachments have the same number of samples for the FBO to be complete.
                if (colorbuffer->getSamples() != samples)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT;
                }

                // in GLES 2.0, all color attachments attachments must have the same number of bitplanes
                // in GLES 3.0, there is no such restriction
                if (clientVersion < 3)
                {
                    if (gl::GetPixelBytes(colorbuffer->getInternalFormat(), clientVersion) != colorbufferSize)
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }

                // D3D11 does not allow for overlapping RenderTargetViews, so ensure uniqueness
                for (unsigned int previousColorAttachment = 0; previousColorAttachment < colorAttachment; previousColorAttachment++)
                {
                    if (mColorbuffers[colorAttachment].get() == mColorbuffers[previousColorAttachment].get())
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }
            }
            else
            {
                width = colorbuffer->getWidth();
                height = colorbuffer->getHeight();
                samples = colorbuffer->getSamples();
                colorbufferSize = gl::GetPixelBytes(colorbuffer->getInternalFormat(), clientVersion);
                missingAttachment = false;
            }
        }
    }

    const Renderbuffer *depthbuffer = NULL;
    const Renderbuffer *stencilbuffer = NULL;

    if (mDepthbuffer.type() != GL_NONE)
    {
        depthbuffer = getDepthbuffer();

        if (!depthbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (depthbuffer->getWidth() == 0 || depthbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (mDepthbuffer.type() == GL_RENDERBUFFER)
        {
            if (!gl::IsDepthRenderingSupported(depthbuffer->getInternalFormat(), mRenderer))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (IsInternalTextureTarget(mDepthbuffer.type(), mRenderer->getCurrentClientVersion()))
        {
            GLenum internalformat = depthbuffer->getInternalFormat();

            // depth texture attachments require OES/ANGLE_depth_texture
            if (!mRenderer->getDepthTextureSupport())
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (gl::GetDepthBits(internalformat, clientVersion) == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            UNREACHABLE();
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (missingAttachment)
        {
            width = depthbuffer->getWidth();
            height = depthbuffer->getHeight();
            samples = depthbuffer->getSamples();
            missingAttachment = false;
        }
        else if (width != depthbuffer->getWidth() || height != depthbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
        else if (samples != depthbuffer->getSamples())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    if (mStencilbuffer.type() != GL_NONE)
    {
        stencilbuffer = getStencilbuffer();

        if (!stencilbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (stencilbuffer->getWidth() == 0 || stencilbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (mStencilbuffer.type() == GL_RENDERBUFFER)
        {
            if (!gl::IsStencilRenderingSupported(stencilbuffer->getInternalFormat(), mRenderer))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (IsInternalTextureTarget(mStencilbuffer.type(), mRenderer->getCurrentClientVersion()))
        {
            GLenum internalformat = stencilbuffer->getInternalFormat();

            // texture stencil attachments come along as part
            // of OES_packed_depth_stencil + OES/ANGLE_depth_texture
            if (!mRenderer->getDepthTextureSupport())
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (gl::GetStencilBits(internalformat, clientVersion) == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            UNREACHABLE();
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (missingAttachment)
        {
            width = stencilbuffer->getWidth();
            height = stencilbuffer->getHeight();
            samples = stencilbuffer->getSamples();
            missingAttachment = false;
        }
        else if (width != stencilbuffer->getWidth() || height != stencilbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
        else if (samples != stencilbuffer->getSamples())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    // if we have both a depth and stencil buffer, they must refer to the same object
    // since we only support packed_depth_stencil and not separate depth and stencil
    if (depthbuffer && stencilbuffer && (depthbuffer != stencilbuffer))
    {
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }

    // we need to have at least one attachment to be complete
    if (missingAttachment)
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

DefaultFramebuffer::DefaultFramebuffer(rx::Renderer *renderer, Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil)
    : Framebuffer(renderer)
{
    mColorbuffers[0].set(new Renderbuffer(mRenderer, 0, colorbuffer), GL_RENDERBUFFER, 0, 0);

    Renderbuffer *depthStencilRenderbuffer = new Renderbuffer(mRenderer, 0, depthStencil);
    mDepthbuffer.set(depthStencilRenderbuffer, (depthStencilRenderbuffer->getDepthSize() != 0) ? GL_RENDERBUFFER : GL_NONE, 0, 0);
    mStencilbuffer.set(depthStencilRenderbuffer, (depthStencilRenderbuffer->getStencilSize() != 0) ? GL_RENDERBUFFER : GL_NONE, 0, 0);

    mDrawBufferStates[0] = GL_BACK;
    mReadBufferState = GL_BACK;
}

int Framebuffer::getSamples() const
{
    if (completeness() == GL_FRAMEBUFFER_COMPLETE)
    {
        // for a complete framebuffer, all attachments must have the same sample count
        // in this case return the first nonzero sample size
        for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
        {
            if (mColorbuffers[colorAttachment].type() != GL_NONE)
            {
                return getColorbuffer(colorAttachment)->getSamples();
            }
        }
    }

    return 0;
}

GLenum DefaultFramebuffer::completeness() const
{
    // The default framebuffer *must* always be complete, though it may not be
    // subject to the same rules as application FBOs. ie, it could have 0x0 size.
    return GL_FRAMEBUFFER_COMPLETE;
}

}
