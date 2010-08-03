//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderbuffer.cpp: the gl::Renderbuffer class and its derived classes
// Colorbuffer, Depthbuffer and Stencilbuffer. Implements GL renderbuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4.3 page 108.

#include "libGLESv2/Renderbuffer.h"

#include "libGLESv2/main.h"
#include "libGLESv2/utilities.h"

namespace gl
{
unsigned int Renderbuffer::mCurrentSerial = 1;

Renderbuffer::Renderbuffer()
{
    mWidth = 0;
    mHeight = 0;
    mFormat = GL_RGBA4; // default format, needs to be one of the expected renderbuffer formats
    mSerial = issueSerial();
}

Renderbuffer::~Renderbuffer()
{
}

bool Renderbuffer::isColorbuffer()
{
    return false;
}

bool Renderbuffer::isDepthbuffer()
{
    return false;
}

bool Renderbuffer::isStencilbuffer()
{
    return false;
}

IDirect3DSurface9 *Renderbuffer::getRenderTarget()
{
    return NULL;
}

IDirect3DSurface9 *Renderbuffer::getDepthStencil()
{
    return NULL;
}

int Renderbuffer::getWidth()
{
    return mWidth;
}

int Renderbuffer::getHeight()
{
    return mHeight;
}

void Renderbuffer::setSize(int width, int height)
{
    mWidth = width;
    mHeight = height;
}


GLenum Renderbuffer::getFormat()
{
    return mFormat;
}

unsigned int Renderbuffer::getSerial() const
{
    return mSerial;
}

unsigned int Renderbuffer::issueSerial()
{
    return mCurrentSerial++;
}

Colorbuffer::Colorbuffer(IDirect3DSurface9 *renderTarget) : mRenderTarget(renderTarget)
{
    if (renderTarget)
    {
        renderTarget->AddRef();

        D3DSURFACE_DESC description;
        renderTarget->GetDesc(&description);

        setSize(description.Width, description.Height);
    }

}

Colorbuffer::Colorbuffer(int width, int height, GLenum format)
{
    IDirect3DDevice9 *device = getDevice();

    mRenderTarget = NULL;
    HRESULT result = device->CreateRenderTarget(width, height, es2dx::ConvertRenderbufferFormat(format), 
                                                D3DMULTISAMPLE_NONE, 0, FALSE, &mRenderTarget, NULL);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        error(GL_OUT_OF_MEMORY);

        return;
    }

    ASSERT(SUCCEEDED(result));

    if (mRenderTarget)
    {
        setSize(width, height);
        mFormat = format;
    }
    else
    {
        setSize(0, 0);
        mFormat = GL_RGBA4;
    }
}

Colorbuffer::~Colorbuffer()
{
    if (mRenderTarget)
    {
        mRenderTarget->Release();
    }
}

bool Colorbuffer::isColorbuffer()
{
    return true;
}

GLuint Colorbuffer::getRedSize()
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetRedSize(description.Format);
    }

    return 0;
}

GLuint Colorbuffer::getGreenSize()
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetGreenSize(description.Format);
    }

    return 0;
}

GLuint Colorbuffer::getBlueSize()
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetBlueSize(description.Format);
    }

    return 0;
}

GLuint Colorbuffer::getAlphaSize()
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetAlphaSize(description.Format);
    }

    return 0;
}

IDirect3DSurface9 *Colorbuffer::getRenderTarget()
{
    return mRenderTarget;
}

Depthbuffer::Depthbuffer(IDirect3DSurface9 *depthStencil) : mDepthStencil(depthStencil)
{
    if (depthStencil)
    {
        depthStencil->AddRef();

        D3DSURFACE_DESC description;
        depthStencil->GetDesc(&description);

        setSize(description.Width, description.Height);
        mFormat = GL_DEPTH_COMPONENT16; // If the renderbuffer parameters are queried, the calling function
                                        // will expect one of the valid renderbuffer formats for use in 
                                        // glRenderbufferStorage
    }
}

Depthbuffer::Depthbuffer(int width, int height)
{
    IDirect3DDevice9 *device = getDevice();

    mDepthStencil = NULL;
    HRESULT result = device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &mDepthStencil, 0);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        error(GL_OUT_OF_MEMORY);

        return;
    }

    ASSERT(SUCCEEDED(result));

    if (mDepthStencil)
    {
        setSize(width, height);
        mFormat = GL_DEPTH_COMPONENT16; // If the renderbuffer parameters are queried, the calling function
                                        // will expect one of the valid renderbuffer formats for use in 
                                        // glRenderbufferStorage
    }
    else
    {
        setSize(0, 0);
        mFormat = GL_RGBA4; //default format
    }
}

Depthbuffer::~Depthbuffer()
{
    if (mDepthStencil)
    {
        mDepthStencil->Release();
    }
}

bool Depthbuffer::isDepthbuffer()
{
    return true;
}

GLuint Depthbuffer::getDepthSize()
{
    if (mDepthStencil)
    {
        D3DSURFACE_DESC description;
        mDepthStencil->GetDesc(&description);

        return es2dx::GetDepthSize(description.Format);
    }

    return 0;
}

IDirect3DSurface9 *Depthbuffer::getDepthStencil()
{
    return mDepthStencil;
}

Stencilbuffer::Stencilbuffer(IDirect3DSurface9 *depthStencil) : mDepthStencil(depthStencil)
{
    if (depthStencil)
    {
        depthStencil->AddRef();

        D3DSURFACE_DESC description;
        depthStencil->GetDesc(&description);

        setSize(description.Width, description.Height);
        mFormat = GL_STENCIL_INDEX8; // If the renderbuffer parameters are queried, the calling function
                                     // will expect one of the valid renderbuffer formats for use in 
                                     // glRenderbufferStorage
    }
}

Stencilbuffer::Stencilbuffer(int width, int height)
{
    IDirect3DDevice9 *device = getDevice();

    mDepthStencil = NULL;
    HRESULT result = device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &mDepthStencil, 0);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        error(GL_OUT_OF_MEMORY);

        return;
    }

    ASSERT(SUCCEEDED(result));

    if (mDepthStencil)
    {
        setSize(width, height);
        mFormat = GL_STENCIL_INDEX8; // If the renderbuffer parameters are queried, the calling function
                                     // will expect one of the valid renderbuffer formats for use in 
                                     // glRenderbufferStorage
    }
    else
    {
        setSize(0, 0);
        mFormat = GL_RGBA4; //default format
    }
}

Stencilbuffer::~Stencilbuffer()
{
    if (mDepthStencil)
    {
        mDepthStencil->Release();
    }
}

GLuint Stencilbuffer::getStencilSize()
{
    if (mDepthStencil)
    {
        D3DSURFACE_DESC description;
        mDepthStencil->GetDesc(&description);

        return es2dx::GetStencilSize(description.Format);
    }

    return 0;
}

bool Stencilbuffer::isStencilbuffer()
{
    return true;
}

IDirect3DSurface9 *Stencilbuffer::getDepthStencil()
{
    return mDepthStencil;
}
}
