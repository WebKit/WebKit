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
unsigned int RenderbufferStorage::mCurrentSerial = 1;

Renderbuffer::Renderbuffer(GLuint id, RenderbufferStorage *storage) : RefCountObject(id)
{
    ASSERT(storage != NULL);
    mStorage = storage;
}

Renderbuffer::~Renderbuffer()
{
    delete mStorage;
}

bool Renderbuffer::isColorbuffer() const
{
    return mStorage->isColorbuffer();
}

bool Renderbuffer::isDepthbuffer() const
{
    return mStorage->isDepthbuffer();
}

bool Renderbuffer::isStencilbuffer() const
{
    return mStorage->isStencilbuffer();
}

IDirect3DSurface9 *Renderbuffer::getRenderTarget()
{
    return mStorage->getRenderTarget();
}

IDirect3DSurface9 *Renderbuffer::getDepthStencil()
{
    return mStorage->getDepthStencil();
}

int Renderbuffer::getWidth() const
{
    return mStorage->getWidth();
}

int Renderbuffer::getHeight() const
{
    return mStorage->getHeight();
}

GLenum Renderbuffer::getFormat() const
{
    return mStorage->getFormat();
}

D3DFORMAT Renderbuffer::getD3DFormat() const
{
    return mStorage->getD3DFormat();
}

unsigned int Renderbuffer::getSerial() const
{
    return mStorage->getSerial();
}

void Renderbuffer::setStorage(RenderbufferStorage *newStorage)
{
    ASSERT(newStorage != NULL);

    delete mStorage;
    mStorage = newStorage;
}

RenderbufferStorage::RenderbufferStorage()
{
    mSerial = issueSerial();
}

RenderbufferStorage::~RenderbufferStorage()
{
}

bool RenderbufferStorage::isColorbuffer() const
{
    return false;
}

bool RenderbufferStorage::isDepthbuffer() const
{
    return false;
}

bool RenderbufferStorage::isStencilbuffer() const
{
    return false;
}

IDirect3DSurface9 *RenderbufferStorage::getRenderTarget()
{
    return NULL;
}

IDirect3DSurface9 *RenderbufferStorage::getDepthStencil()
{
    return NULL;
}

int RenderbufferStorage::getWidth() const
{
    return mWidth;
}

int RenderbufferStorage::getHeight() const
{
    return mHeight;
}

void RenderbufferStorage::setSize(int width, int height)
{
    mWidth = width;
    mHeight = height;
}

GLenum RenderbufferStorage::getFormat() const
{
    return mFormat;
}

D3DFORMAT RenderbufferStorage::getD3DFormat() const
{
    return mD3DFormat;
}

GLsizei RenderbufferStorage::getSamples() const
{
    return mSamples;
}

unsigned int RenderbufferStorage::getSerial() const
{
    return mSerial;
}

unsigned int RenderbufferStorage::issueSerial()
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
        mD3DFormat = description.Format;
        mSamples = es2dx::GetSamplesFromMultisampleType(description.MultiSampleType);
    }
    else
    {
        mD3DFormat = D3DFMT_UNKNOWN;
        mSamples = 0;
    }
}

Colorbuffer::Colorbuffer(int width, int height, GLenum format, GLsizei samples)
{
    IDirect3DDevice9 *device = getDevice();

    mRenderTarget = NULL;
    D3DFORMAT requestedFormat = es2dx::ConvertRenderbufferFormat(format);
    int supportedSamples = getContext()->getNearestSupportedSamples(requestedFormat, samples);

    if (supportedSamples == -1)
    {
        error(GL_OUT_OF_MEMORY);

        return;
    }

    if (width > 0 && height > 0)
    {
        HRESULT result = device->CreateRenderTarget(width, height, requestedFormat, 
                                                    es2dx::GetMultisampleTypeFromSamples(supportedSamples), 0, FALSE, &mRenderTarget, NULL);

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            error(GL_OUT_OF_MEMORY);

            return;
        }

        ASSERT(SUCCEEDED(result));
    }

    if (mRenderTarget)
    {
        setSize(width, height);
        mFormat = format;
        mD3DFormat = requestedFormat;
        mSamples = supportedSamples;
    }
    else
    {
        setSize(0, 0);
        mFormat = GL_RGBA4;
        mD3DFormat = D3DFMT_UNKNOWN;
        mSamples = 0;
    }
}

Colorbuffer::~Colorbuffer()
{
    if (mRenderTarget)
    {
        mRenderTarget->Release();
    }
}

bool Colorbuffer::isColorbuffer() const
{
    return true;
}

GLuint Colorbuffer::getRedSize() const
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetRedSize(description.Format);
    }

    return 0;
}

GLuint Colorbuffer::getGreenSize() const
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetGreenSize(description.Format);
    }

    return 0;
}

GLuint Colorbuffer::getBlueSize() const
{
    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        return es2dx::GetBlueSize(description.Format);
    }

    return 0;
}

GLuint Colorbuffer::getAlphaSize() const
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

DepthStencilbuffer::DepthStencilbuffer(IDirect3DSurface9 *depthStencil) : mDepthStencil(depthStencil)
{
    if (depthStencil)
    {
        depthStencil->AddRef();

        D3DSURFACE_DESC description;
        depthStencil->GetDesc(&description);

        setSize(description.Width, description.Height);
        mFormat = (description.Format == D3DFMT_D16 ? GL_DEPTH_COMPONENT16 : GL_DEPTH24_STENCIL8_OES);
        mSamples = es2dx::GetSamplesFromMultisampleType(description.MultiSampleType); 
        mD3DFormat = description.Format;
    }
    else
    {
        mD3DFormat = D3DFMT_UNKNOWN; 
        mSamples = 0;
    }
}

DepthStencilbuffer::DepthStencilbuffer(int width, int height, GLsizei samples)
{
    IDirect3DDevice9 *device = getDevice();

    mDepthStencil = NULL;
    
    int supportedSamples = getContext()->getNearestSupportedSamples(D3DFMT_D24S8, samples);

    if (supportedSamples == -1)
    {
        error(GL_OUT_OF_MEMORY);

        return;
    }

    HRESULT result = device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, es2dx::GetMultisampleTypeFromSamples(supportedSamples),
                                                       0, FALSE, &mDepthStencil, 0);

    if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
    {
        error(GL_OUT_OF_MEMORY);

        return;
    }

    ASSERT(SUCCEEDED(result));

    if (mDepthStencil)
    {
        setSize(width, height);
        mFormat = GL_DEPTH24_STENCIL8_OES;
        mD3DFormat = D3DFMT_D24S8;
        mSamples = supportedSamples;
    }
    else
    {
        setSize(0, 0);
        mFormat = GL_RGBA4; //default format
        mD3DFormat = D3DFMT_UNKNOWN;
        mSamples = 0;
    }
}

DepthStencilbuffer::~DepthStencilbuffer()
{
    if (mDepthStencil)
    {
        mDepthStencil->Release();
    }
}

bool DepthStencilbuffer::isDepthbuffer() const
{
    return true;
}

bool DepthStencilbuffer::isStencilbuffer() const
{
    return true;
}

GLuint DepthStencilbuffer::getDepthSize() const
{
    if (mDepthStencil)
    {
        D3DSURFACE_DESC description;
        mDepthStencil->GetDesc(&description);

        return es2dx::GetDepthSize(description.Format);
    }

    return 0;
}

GLuint DepthStencilbuffer::getStencilSize() const
{
    if (mDepthStencil)
    {
        D3DSURFACE_DESC description;
        mDepthStencil->GetDesc(&description);

        return es2dx::GetStencilSize(description.Format);
    }

    return 0;
}

IDirect3DSurface9 *DepthStencilbuffer::getDepthStencil()
{
    return mDepthStencil;
}

Depthbuffer::Depthbuffer(IDirect3DSurface9 *depthStencil) : DepthStencilbuffer(depthStencil)
{
    if (depthStencil)
    {
        mFormat = GL_DEPTH_COMPONENT16; // If the renderbuffer parameters are queried, the calling function
                                        // will expect one of the valid renderbuffer formats for use in 
                                        // glRenderbufferStorage
    }
}

Depthbuffer::Depthbuffer(int width, int height, GLsizei samples) : DepthStencilbuffer(width, height, samples)
{
    if (getDepthStencil())
    {
        mFormat = GL_DEPTH_COMPONENT16; // If the renderbuffer parameters are queried, the calling function
                                        // will expect one of the valid renderbuffer formats for use in 
                                        // glRenderbufferStorage
    }
}

Depthbuffer::~Depthbuffer()
{
}

bool Depthbuffer::isDepthbuffer() const
{
    return true;
}

bool Depthbuffer::isStencilbuffer() const
{
    return false;
}

Stencilbuffer::Stencilbuffer(IDirect3DSurface9 *depthStencil) : DepthStencilbuffer(depthStencil)
{
    if (depthStencil)
    {
        mFormat = GL_STENCIL_INDEX8; // If the renderbuffer parameters are queried, the calling function
                                     // will expect one of the valid renderbuffer formats for use in 
                                     // glRenderbufferStorage
    }
    else
    {
        mFormat = GL_RGBA4; //default format
    }
}

Stencilbuffer::Stencilbuffer(int width, int height, GLsizei samples) : DepthStencilbuffer(width, height, samples)
{
    if (getDepthStencil())
    {
        mFormat = GL_STENCIL_INDEX8; // If the renderbuffer parameters are queried, the calling function
                                     // will expect one of the valid renderbuffer formats for use in 
                                     // glRenderbufferStorage
    }
}

Stencilbuffer::~Stencilbuffer()
{
}

bool Stencilbuffer::isDepthbuffer() const
{
    return false;
}

bool Stencilbuffer::isStencilbuffer() const
{
    return true;
}
}
