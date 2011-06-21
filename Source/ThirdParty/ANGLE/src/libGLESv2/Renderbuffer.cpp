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
#include "libGLESv2/Texture.h"
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

GLsizei Renderbuffer::getWidth() const
{
    return mStorage->getWidth();
}

GLsizei Renderbuffer::getHeight() const
{
    return mStorage->getHeight();
}

GLenum Renderbuffer::getInternalFormat() const
{
    return mStorage->getInternalFormat();
}

GLuint Renderbuffer::getRedSize() const
{
    return mStorage->getRedSize();
}

GLuint Renderbuffer::getGreenSize() const
{
    return mStorage->getGreenSize();
}

GLuint Renderbuffer::getBlueSize() const
{
    return mStorage->getBlueSize();
}

GLuint Renderbuffer::getAlphaSize() const
{
    return mStorage->getAlphaSize();
}

GLuint Renderbuffer::getDepthSize() const
{
    return mStorage->getDepthSize();
}

GLuint Renderbuffer::getStencilSize() const
{
    return mStorage->getStencilSize();
}

GLsizei Renderbuffer::getSamples() const
{
    return mStorage->getSamples();
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

RenderbufferStorage::RenderbufferStorage() : mSerial(issueSerial())
{
    mWidth = 0;
    mHeight = 0;
    mInternalFormat = GL_RGBA4;
    mD3DFormat = D3DFMT_A8R8G8B8;
    mSamples = 0;
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

GLsizei RenderbufferStorage::getWidth() const
{
    return mWidth;
}

GLsizei RenderbufferStorage::getHeight() const
{
    return mHeight;
}

GLenum RenderbufferStorage::getInternalFormat() const
{
    return mInternalFormat;
}

GLuint RenderbufferStorage::getRedSize() const
{
    return dx2es::GetRedSize(getD3DFormat());
}

GLuint RenderbufferStorage::getGreenSize() const
{
    return dx2es::GetGreenSize(getD3DFormat());
}

GLuint RenderbufferStorage::getBlueSize() const
{
    return dx2es::GetBlueSize(getD3DFormat());
}

GLuint RenderbufferStorage::getAlphaSize() const
{
    return dx2es::GetAlphaSize(getD3DFormat());
}

GLuint RenderbufferStorage::getDepthSize() const
{
    return dx2es::GetDepthSize(getD3DFormat());
}

GLuint RenderbufferStorage::getStencilSize() const
{
    return dx2es::GetStencilSize(getD3DFormat());
}

GLsizei RenderbufferStorage::getSamples() const
{
    return mSamples;
}

D3DFORMAT RenderbufferStorage::getD3DFormat() const
{
    return mD3DFormat;
}

unsigned int RenderbufferStorage::getSerial() const
{
    return mSerial;
}

unsigned int RenderbufferStorage::issueSerial()
{
    return mCurrentSerial++;
}

Colorbuffer::Colorbuffer(IDirect3DSurface9 *renderTarget) : mRenderTarget(renderTarget), mTexture(NULL)
{
    if (renderTarget)
    {
        renderTarget->AddRef();

        D3DSURFACE_DESC description;
        renderTarget->GetDesc(&description);

        mWidth = description.Width;
        mHeight = description.Height;
        mInternalFormat = dx2es::ConvertBackBufferFormat(description.Format);
        mD3DFormat = description.Format;
        mSamples = dx2es::GetSamplesFromMultisampleType(description.MultiSampleType);
    }
}

Colorbuffer::Colorbuffer(Texture *texture, GLenum target) : mRenderTarget(NULL), mTexture(texture), mTarget(target)
{
    if (texture)
    {
        mWidth = texture->getWidth();
        mHeight = texture->getHeight();
        mInternalFormat = texture->getInternalFormat();
        mD3DFormat = texture->getD3DFormat();
        mSamples = 0;

        mRenderTarget = texture->getRenderTarget(target);
    }
}

Colorbuffer::Colorbuffer(int width, int height, GLenum format, GLsizei samples) : mRenderTarget(NULL), mTexture(NULL)
{
    IDirect3DDevice9 *device = getDevice();

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

    mWidth = width;
    mHeight = height;
    mInternalFormat = format;
    mD3DFormat = requestedFormat;
    mSamples = supportedSamples;
}

Colorbuffer::~Colorbuffer()
{
    if (mRenderTarget)
    {
        mRenderTarget->Release();
    }
}

GLsizei Colorbuffer::getWidth() const
{
    if (mTexture)
    {
        return mTexture->getWidth();
    }

    return mWidth;
}

GLsizei Colorbuffer::getHeight() const
{
    if (mTexture)
    {
        return mTexture->getHeight();
    }

    return mHeight;
}

GLenum Colorbuffer::getInternalFormat() const
{
    if (mTexture)
    {
        return mTexture->getInternalFormat();
    }

    return mInternalFormat;
}

GLenum Colorbuffer::getType() const
{
    if (mTexture)
    {
        return mTexture->getType();
    }

    return GL_UNSIGNED_BYTE;
}

D3DFORMAT Colorbuffer::getD3DFormat() const
{
    if (mTexture)
    {
        return mTexture->getD3DFormat();
    }

    return mD3DFormat;
}

bool Colorbuffer::isColorbuffer() const
{
    return true;
}

IDirect3DSurface9 *Colorbuffer::getRenderTarget()
{
    if (mTexture)
    {
        if (mRenderTarget)
        {
            mRenderTarget->Release();
        }

        mRenderTarget = mTexture->getRenderTarget(mTarget);
    }

    return mRenderTarget;
}

DepthStencilbuffer::DepthStencilbuffer(IDirect3DSurface9 *depthStencil) : mDepthStencil(depthStencil)
{
    if (depthStencil)
    {
        depthStencil->AddRef();

        D3DSURFACE_DESC description;
        depthStencil->GetDesc(&description);

        mWidth = description.Width;
        mHeight = description.Height;
        mInternalFormat = dx2es::ConvertDepthStencilFormat(description.Format);
        mSamples = dx2es::GetSamplesFromMultisampleType(description.MultiSampleType); 
        mD3DFormat = description.Format;
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

    if (width > 0 && height > 0)
    {
        HRESULT result = device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, es2dx::GetMultisampleTypeFromSamples(supportedSamples),
                                                           0, FALSE, &mDepthStencil, 0);

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY)
        {
            error(GL_OUT_OF_MEMORY);

            return;
        }

        ASSERT(SUCCEEDED(result));
    }

    mWidth = width;
    mHeight = height;
    mInternalFormat = GL_DEPTH24_STENCIL8_OES;
    mD3DFormat = D3DFMT_D24S8;
    mSamples = supportedSamples;
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

IDirect3DSurface9 *DepthStencilbuffer::getDepthStencil()
{
    return mDepthStencil;
}

Depthbuffer::Depthbuffer(IDirect3DSurface9 *depthStencil) : DepthStencilbuffer(depthStencil)
{
    if (depthStencil)
    {
        mInternalFormat = GL_DEPTH_COMPONENT16;   // If the renderbuffer parameters are queried, the calling function
                                                  // will expect one of the valid renderbuffer formats for use in 
                                                  // glRenderbufferStorage
    }
}

Depthbuffer::Depthbuffer(int width, int height, GLsizei samples) : DepthStencilbuffer(width, height, samples)
{
    if (getDepthStencil())
    {
        mInternalFormat = GL_DEPTH_COMPONENT16;   // If the renderbuffer parameters are queried, the calling function
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
        mInternalFormat = GL_STENCIL_INDEX8;   // If the renderbuffer parameters are queried, the calling function
                                               // will expect one of the valid renderbuffer formats for use in 
                                               // glRenderbufferStorage
    }
}

Stencilbuffer::Stencilbuffer(int width, int height, GLsizei samples) : DepthStencilbuffer(width, height, samples)
{
    if (getDepthStencil())
    {
        mInternalFormat = GL_STENCIL_INDEX8;   // If the renderbuffer parameters are queried, the calling function
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
