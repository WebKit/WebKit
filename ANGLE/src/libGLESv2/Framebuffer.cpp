//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
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
    mColorbufferHandle = 0;

    mDepthbufferType = GL_NONE;
    mDepthbufferHandle = 0;

    mStencilbufferType = GL_NONE;
    mStencilbufferHandle = 0;
}

Framebuffer::~Framebuffer()
{
}

void Framebuffer::setColorbuffer(GLenum type, GLuint colorbuffer)
{
    mColorbufferType = type;
    mColorbufferHandle = colorbuffer;
}

void Framebuffer::setDepthbuffer(GLenum type, GLuint depthbuffer)
{
    mDepthbufferType = type;
    mDepthbufferHandle = depthbuffer;
}

void Framebuffer::setStencilbuffer(GLenum type, GLuint stencilbuffer)
{
    mStencilbufferType = type;
    mStencilbufferHandle = stencilbuffer;
}

void Framebuffer::detachTexture(GLuint texture)
{
    if (mColorbufferHandle == texture && IsTextureTarget(mColorbufferType))
    {
        mColorbufferType = GL_NONE;
        mColorbufferHandle = 0;
    }

    if (mDepthbufferHandle == texture && IsTextureTarget(mDepthbufferType))
    {
        mDepthbufferType = GL_NONE;
        mDepthbufferHandle = 0;
    }

    if (mStencilbufferHandle == texture && IsTextureTarget(mStencilbufferType))
    {
        mStencilbufferType = GL_NONE;
        mStencilbufferHandle = 0;
    }
}

void Framebuffer::detachRenderbuffer(GLuint renderbuffer)
{
    if (mColorbufferHandle == renderbuffer && mColorbufferType == GL_RENDERBUFFER)
    {
        mColorbufferType = GL_NONE;
        mColorbufferHandle = 0;
    }

    if (mDepthbufferHandle == renderbuffer && mDepthbufferType == GL_RENDERBUFFER)
    {
        mDepthbufferType = GL_NONE;
        mDepthbufferHandle = 0;
    }

    if (mStencilbufferHandle == renderbuffer && mStencilbufferType == GL_RENDERBUFFER)
    {
        mStencilbufferType = GL_NONE;
        mStencilbufferHandle = 0;
    }
}

unsigned int Framebuffer::getRenderTargetSerial()
{
    Renderbuffer *colorbuffer = getColorbuffer();

    if (colorbuffer)
    {
        return colorbuffer->getSerial();
    }

    return 0;
}

IDirect3DSurface9 *Framebuffer::getRenderTarget()
{
    Renderbuffer *colorbuffer = getColorbuffer();

    if (colorbuffer)
    {
        return colorbuffer->getRenderTarget();
    }

    return NULL;
}

unsigned int Framebuffer::getDepthbufferSerial()
{
    gl::Context *context = gl::getContext();
    Depthbuffer *depthbuffer = context->getDepthbuffer(mDepthbufferHandle);

    if (depthbuffer)
    {
        return depthbuffer->getSerial();
    }

    return 0;
}

IDirect3DSurface9 *Framebuffer::getDepthStencil()
{
    gl::Context *context = gl::getContext();
    Depthbuffer *depthbuffer = context->getDepthbuffer(mDepthbufferHandle);

    if (depthbuffer)
    {
        return depthbuffer->getDepthStencil();
    }

    return NULL;
}

Colorbuffer *Framebuffer::getColorbuffer()
{
    gl::Context *context = gl::getContext();
    Colorbuffer *colorbuffer = NULL;

    if (mColorbufferType == GL_NONE)
    {
        UNREACHABLE();
        colorbuffer = NULL;
    }
    else if (mColorbufferType == GL_RENDERBUFFER)
    {
        colorbuffer = context->getColorbuffer(mColorbufferHandle);
    }
    else
    {
        colorbuffer = context->getTexture(mColorbufferHandle)->getColorbuffer(mColorbufferType);
    }

    if (colorbuffer && colorbuffer->isColorbuffer())
    {
        return colorbuffer;
    }

    return NULL;
}

Depthbuffer *Framebuffer::getDepthbuffer()
{
    gl::Context *context = gl::getContext();
    Depthbuffer *depthbuffer = context->getDepthbuffer(mDepthbufferHandle);

    if (depthbuffer && depthbuffer->isDepthbuffer())
    {
        return depthbuffer;
    }

    return NULL;
}

Stencilbuffer *Framebuffer::getStencilbuffer()
{
    gl::Context *context = gl::getContext();
    Stencilbuffer *stencilbuffer = context->getStencilbuffer(mStencilbufferHandle);

    if (stencilbuffer && stencilbuffer->isStencilbuffer())
    {
        return stencilbuffer;
    }

    return NULL;
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
    return mColorbufferHandle;
}

GLuint Framebuffer::getDepthbufferHandle()
{
    return mDepthbufferHandle;
}

GLuint Framebuffer::getStencilbufferHandle()
{
    return mStencilbufferHandle;
}

GLenum Framebuffer::completeness()
{
    gl::Context *context = gl::getContext();

    int width = 0;
    int height = 0;

    if (mColorbufferType != GL_NONE)
    {
        Colorbuffer *colorbuffer = getColorbuffer();

        if (!colorbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (colorbuffer->getWidth() == 0 || colorbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        width = colorbuffer->getWidth();
        height = colorbuffer->getHeight();
    }

    if (mDepthbufferType != GL_NONE)
    {
        Depthbuffer *depthbuffer = context->getDepthbuffer(mDepthbufferHandle);

        if (!depthbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (depthbuffer->getWidth() == 0 || depthbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (width == 0)
        {
            width = depthbuffer->getWidth();
            height = depthbuffer->getHeight();
        }
        else if (width != depthbuffer->getWidth() || height != depthbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if (mStencilbufferType != GL_NONE)
    {
        Stencilbuffer *stencilbuffer = context->getStencilbuffer(mStencilbufferHandle);

        if (!stencilbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (stencilbuffer->getWidth() == 0 || stencilbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (width == 0)
        {
            width = stencilbuffer->getWidth();
            height = stencilbuffer->getHeight();
        }
        else if (width != stencilbuffer->getWidth() || height != stencilbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    return GL_FRAMEBUFFER_COMPLETE;
}
}
