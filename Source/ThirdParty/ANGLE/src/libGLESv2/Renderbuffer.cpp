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

RenderbufferInterface::RenderbufferInterface()
{
}

GLuint RenderbufferInterface::getRedSize() const
{
    return dx2es::GetRedSize(getD3DFormat());
}

GLuint RenderbufferInterface::getGreenSize() const
{
    return dx2es::GetGreenSize(getD3DFormat());
}

GLuint RenderbufferInterface::getBlueSize() const
{
    return dx2es::GetBlueSize(getD3DFormat());
}

GLuint RenderbufferInterface::getAlphaSize() const
{
    return dx2es::GetAlphaSize(getD3DFormat());
}

GLuint RenderbufferInterface::getDepthSize() const
{
    return dx2es::GetDepthSize(getD3DFormat());
}

GLuint RenderbufferInterface::getStencilSize() const
{
    return dx2es::GetStencilSize(getD3DFormat());
}

RenderbufferTexture::RenderbufferTexture(Texture *texture, GLenum target) : mTexture(texture), mTarget(target)
{
}

RenderbufferTexture::~RenderbufferTexture()
{
}

IDirect3DSurface9 *RenderbufferTexture::getRenderTarget()
{
    return mTexture->getRenderTarget(mTarget);
}

IDirect3DSurface9 *RenderbufferTexture::getDepthStencil()
{
    return NULL;
}

GLsizei RenderbufferTexture::getWidth() const
{
    return mTexture->getWidth(0);
}
 
GLsizei RenderbufferTexture::getHeight() const
{
    return mTexture->getHeight(0);
}
 
GLenum RenderbufferTexture::getInternalFormat() const
{
    return mTexture->getInternalFormat();
}

D3DFORMAT RenderbufferTexture::getD3DFormat() const
{
    return mTexture->getD3DFormat();
}

GLsizei RenderbufferTexture::getSamples() const
{
    return 0;
}

unsigned int RenderbufferTexture::getSerial() const
{
    return mTexture->getRenderTargetSerial(mTarget);
}

Renderbuffer::Renderbuffer(GLuint id, RenderbufferInterface *instance) : RefCountObject(id)
{
    ASSERT(instance != NULL);
    mInstance = instance;
}

Renderbuffer::~Renderbuffer()
{
    delete mInstance;
}

IDirect3DSurface9 *Renderbuffer::getRenderTarget()
{
    return mInstance->getRenderTarget();
}

IDirect3DSurface9 *Renderbuffer::getDepthStencil()
{
    return mInstance->getDepthStencil();
}

GLsizei Renderbuffer::getWidth() const
{
    return mInstance->getWidth();
}

GLsizei Renderbuffer::getHeight() const
{
    return mInstance->getHeight();
}

GLenum Renderbuffer::getInternalFormat() const
{
    return mInstance->getInternalFormat();
}

D3DFORMAT Renderbuffer::getD3DFormat() const
{
    return mInstance->getD3DFormat();
}

GLuint Renderbuffer::getRedSize() const
{
    return mInstance->getRedSize();
}

GLuint Renderbuffer::getGreenSize() const
{
    return mInstance->getGreenSize();
}

GLuint Renderbuffer::getBlueSize() const
{
    return mInstance->getBlueSize();
}

GLuint Renderbuffer::getAlphaSize() const
{
    return mInstance->getAlphaSize();
}

GLuint Renderbuffer::getDepthSize() const
{
    return mInstance->getDepthSize();
}

GLuint Renderbuffer::getStencilSize() const
{
    return mInstance->getStencilSize();
}

GLsizei Renderbuffer::getSamples() const
{
    return mInstance->getSamples();
}

unsigned int Renderbuffer::getSerial() const
{
    return mInstance->getSerial();
}

void Renderbuffer::setStorage(RenderbufferStorage *newStorage)
{
    ASSERT(newStorage != NULL);

    delete mInstance;
    mInstance = newStorage;
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

unsigned int RenderbufferStorage::issueCubeSerials()
{
    unsigned int firstSerial = mCurrentSerial;
    mCurrentSerial += 6;
    return firstSerial;
}

Colorbuffer::Colorbuffer(IDirect3DSurface9 *renderTarget) : mRenderTarget(renderTarget)
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

Colorbuffer::Colorbuffer(int width, int height, GLenum format, GLsizei samples) : mRenderTarget(NULL)
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

IDirect3DSurface9 *Colorbuffer::getRenderTarget()
{
    if (mRenderTarget)
    {
        mRenderTarget->AddRef();
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
    if (mDepthStencil)
    {
        mInternalFormat = GL_DEPTH_COMPONENT16;   // If the renderbuffer parameters are queried, the calling function
                                                  // will expect one of the valid renderbuffer formats for use in 
                                                  // glRenderbufferStorage
    }
}

Depthbuffer::~Depthbuffer()
{
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
    if (mDepthStencil)
    {
        mInternalFormat = GL_STENCIL_INDEX8;   // If the renderbuffer parameters are queried, the calling function
                                               // will expect one of the valid renderbuffer formats for use in 
                                               // glRenderbufferStorage
    }
}

Stencilbuffer::~Stencilbuffer()
{
}

}
