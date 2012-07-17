//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libGLESv2/Framebuffer.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/utilities.h"

namespace gl
{

Framebuffer::Framebuffer()
{
    mColorbufferType = GL_NONE;
    mDepthbufferType = GL_NONE;
    mStencilbufferType = GL_NONE;
}

Framebuffer::~Framebuffer()
{
    mColorbufferPointer.set(NULL);
    mDepthbufferPointer.set(NULL);
    mStencilbufferPointer.set(NULL);
    mNullColorbufferPointer.set(NULL);
}

Renderbuffer *Framebuffer::lookupRenderbuffer(GLenum type, GLuint handle) const
{
    gl::Context *context = gl::getContext();
    Renderbuffer *buffer = NULL;

    if (type == GL_NONE)
    {
        buffer = NULL;
    }
    else if (type == GL_RENDERBUFFER)
    {
        buffer = context->getRenderbuffer(handle);
    }
    else if (IsInternalTextureTarget(type))
    {
        buffer = context->getTexture(handle)->getRenderbuffer(type);
    }
    else
    {
        UNREACHABLE();
    }

    return buffer;
}

void Framebuffer::setColorbuffer(GLenum type, GLuint colorbuffer)
{
    mColorbufferType = (colorbuffer != 0) ? type : GL_NONE;
    mColorbufferPointer.set(lookupRenderbuffer(type, colorbuffer));
}

void Framebuffer::setDepthbuffer(GLenum type, GLuint depthbuffer)
{
    mDepthbufferType = (depthbuffer != 0) ? type : GL_NONE;
    mDepthbufferPointer.set(lookupRenderbuffer(type, depthbuffer));
}

void Framebuffer::setStencilbuffer(GLenum type, GLuint stencilbuffer)
{
    mStencilbufferType = (stencilbuffer != 0) ? type : GL_NONE;
    mStencilbufferPointer.set(lookupRenderbuffer(type, stencilbuffer));
}

void Framebuffer::detachTexture(GLuint texture)
{
    if (mColorbufferPointer.id() == texture && IsInternalTextureTarget(mColorbufferType))
    {
        mColorbufferType = GL_NONE;
        mColorbufferPointer.set(NULL);
    }

    if (mDepthbufferPointer.id() == texture && IsInternalTextureTarget(mDepthbufferType))
    {
        mDepthbufferType = GL_NONE;
        mDepthbufferPointer.set(NULL);
    }

    if (mStencilbufferPointer.id() == texture && IsInternalTextureTarget(mStencilbufferType))
    {
        mStencilbufferType = GL_NONE;
        mStencilbufferPointer.set(NULL);
    }
}

void Framebuffer::detachRenderbuffer(GLuint renderbuffer)
{
    if (mColorbufferPointer.id() == renderbuffer && mColorbufferType == GL_RENDERBUFFER)
    {
        mColorbufferType = GL_NONE;
        mColorbufferPointer.set(NULL);
    }

    if (mDepthbufferPointer.id() == renderbuffer && mDepthbufferType == GL_RENDERBUFFER)
    {
        mDepthbufferType = GL_NONE;
        mDepthbufferPointer.set(NULL);
    }

    if (mStencilbufferPointer.id() == renderbuffer && mStencilbufferType == GL_RENDERBUFFER)
    {
        mStencilbufferType = GL_NONE;
        mStencilbufferPointer.set(NULL);
    }
}

unsigned int Framebuffer::getRenderTargetSerial()
{
    Renderbuffer *colorbuffer = mColorbufferPointer.get();

    if (colorbuffer)
    {
        return colorbuffer->getSerial();
    }

    return 0;
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *Framebuffer::getRenderTarget()
{
    Renderbuffer *colorbuffer = mColorbufferPointer.get();

    if (colorbuffer)
    {
        return colorbuffer->getRenderTarget();
    }

    return NULL;
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *Framebuffer::getDepthStencil()
{
    Renderbuffer *depthstencilbuffer = mDepthbufferPointer.get();
    
    if (!depthstencilbuffer)
    {
        depthstencilbuffer = mStencilbufferPointer.get();
    }

    if (depthstencilbuffer)
    {
        return depthstencilbuffer->getDepthStencil();
    }

    return NULL;
}

unsigned int Framebuffer::getDepthbufferSerial()
{
    Renderbuffer *depthbuffer = mDepthbufferPointer.get();

    if (depthbuffer)
    {
        return depthbuffer->getSerial();
    }

    return 0;
}

unsigned int Framebuffer::getStencilbufferSerial()
{
    Renderbuffer *stencilbuffer = mStencilbufferPointer.get();

    if (stencilbuffer)
    {
        return stencilbuffer->getSerial();
    }

    return 0;
}

Renderbuffer *Framebuffer::getColorbuffer()
{
    return mColorbufferPointer.get();
}

Renderbuffer *Framebuffer::getDepthbuffer()
{
    return mDepthbufferPointer.get();
}

Renderbuffer *Framebuffer::getStencilbuffer()
{
    return mStencilbufferPointer.get();
}

Renderbuffer *Framebuffer::getNullColorbuffer()
{
    Renderbuffer *nullbuffer  = mNullColorbufferPointer.get();
    Renderbuffer *depthbuffer = getDepthbuffer();

    if (!depthbuffer)
    {
        ERR("Unexpected null depthbuffer for depth-only FBO.");
        return NULL;
    }

    GLsizei width  = depthbuffer->getWidth();
    GLsizei height = depthbuffer->getHeight();

    if (!nullbuffer ||
        width != nullbuffer->getWidth() || height != nullbuffer->getHeight())
    {
        nullbuffer = new Renderbuffer(0, new Colorbuffer(width, height, GL_NONE, 0));
        mNullColorbufferPointer.set(nullbuffer);
    }

    return nullbuffer;
}

GLenum Framebuffer::getColorbufferType()
{
    return mColorbufferType;
}

GLenum Framebuffer::getDepthbufferType()
{
    return mDepthbufferType;
}

GLenum Framebuffer::getStencilbufferType()
{
    return mStencilbufferType;
}

GLuint Framebuffer::getColorbufferHandle()
{
    return mColorbufferPointer.id();
}

GLuint Framebuffer::getDepthbufferHandle()
{
    return mDepthbufferPointer.id();
}

GLuint Framebuffer::getStencilbufferHandle()
{
    return mStencilbufferPointer.id();
}

bool Framebuffer::hasStencil()
{
    if (mStencilbufferType != GL_NONE)
    {
        Renderbuffer *stencilbufferObject = getStencilbuffer();

        if (stencilbufferObject)
        {
            return stencilbufferObject->getStencilSize() > 0;
        }
    }

    return false;
}

GLenum Framebuffer::completeness()
{
    gl::Context *context = gl::getContext();
    int width = 0;
    int height = 0;
    int samples = -1;
    bool missingAttachment = true;

    if (mColorbufferType != GL_NONE)
    {
        Renderbuffer *colorbuffer = getColorbuffer();

        if (!colorbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (colorbuffer->getWidth() == 0 || colorbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (mColorbufferType == GL_RENDERBUFFER)
        {
            if (!gl::IsColorRenderable(colorbuffer->getInternalFormat()))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (IsInternalTextureTarget(mColorbufferType))
        {
            GLenum internalformat = colorbuffer->getInternalFormat();
            D3DFORMAT d3dformat = colorbuffer->getD3DFormat();

            if (IsCompressed(internalformat) ||
                internalformat == GL_ALPHA ||
                internalformat == GL_LUMINANCE ||
                internalformat == GL_LUMINANCE_ALPHA)
            {
                return GL_FRAMEBUFFER_UNSUPPORTED;
            }

            if ((dx2es::IsFloat32Format(d3dformat) && !context->supportsFloat32RenderableTextures()) ||
                (dx2es::IsFloat16Format(d3dformat) && !context->supportsFloat16RenderableTextures()))
            {
                return GL_FRAMEBUFFER_UNSUPPORTED;
            }

            if (dx2es::IsDepthTextureFormat(d3dformat) || dx2es::IsStencilTextureFormat(d3dformat))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            UNREACHABLE();
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        width = colorbuffer->getWidth();
        height = colorbuffer->getHeight();
        samples = colorbuffer->getSamples();
        missingAttachment = false;
    }

    Renderbuffer *depthbuffer = NULL;
    Renderbuffer *stencilbuffer = NULL;

    if (mDepthbufferType != GL_NONE)
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

        if (mDepthbufferType == GL_RENDERBUFFER)
        {
            if (!gl::IsDepthRenderable(depthbuffer->getInternalFormat()))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (IsInternalTextureTarget(mDepthbufferType))
        {
            D3DFORMAT d3dformat = depthbuffer->getD3DFormat();

            // depth texture attachments require OES/ANGLE_depth_texture
            if (!context->supportsDepthTextures())
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!dx2es::IsDepthTextureFormat(d3dformat))
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

    if (mStencilbufferType != GL_NONE)
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

        if (mStencilbufferType == GL_RENDERBUFFER)
        {
            if (!gl::IsStencilRenderable(stencilbuffer->getInternalFormat()))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (IsInternalTextureTarget(mStencilbufferType))
        {
            D3DFORMAT d3dformat = stencilbuffer->getD3DFormat();

            // texture stencil attachments come along as part
            // of OES_packed_depth_stencil + OES/ANGLE_depth_texture
            if (!context->supportsDepthTextures())
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!dx2es::IsStencilTextureFormat(d3dformat))
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

DefaultFramebuffer::DefaultFramebuffer(Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil)
{
    mColorbufferPointer.set(new Renderbuffer(0, colorbuffer));

    Renderbuffer *depthStencilRenderbuffer = new Renderbuffer(0, depthStencil);
    mDepthbufferPointer.set(depthStencilRenderbuffer);
    mStencilbufferPointer.set(depthStencilRenderbuffer);

    mColorbufferType = GL_RENDERBUFFER;
    mDepthbufferType = (depthStencilRenderbuffer->getDepthSize() != 0) ? GL_RENDERBUFFER : GL_NONE;
    mStencilbufferType = (depthStencilRenderbuffer->getStencilSize() != 0) ? GL_RENDERBUFFER : GL_NONE;
}

int Framebuffer::getSamples()
{
    if (completeness() == GL_FRAMEBUFFER_COMPLETE)
    {
        return getColorbuffer()->getSamples();
    }
    else
    {
        return 0;
    }
}

GLenum DefaultFramebuffer::completeness()
{
    // The default framebuffer should always be complete
    ASSERT(Framebuffer::completeness() == GL_FRAMEBUFFER_COMPLETE);

    return GL_FRAMEBUFFER_COMPLETE;
}

}
