#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderbuffer.cpp: the gl::Renderbuffer class and its derived classes
// Colorbuffer, Depthbuffer and Stencilbuffer. Implements GL renderbuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4.3 page 108.

#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/renderer/RenderTarget.h"

#include "libGLESv2/Texture.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/TextureStorage.h"
#include "common/utilities.h"
#include "libGLESv2/formatutils.h"

namespace gl
{
unsigned int RenderbufferStorage::mCurrentSerial = 1;

RenderbufferInterface::RenderbufferInterface()
{
}

// The default case for classes inherited from RenderbufferInterface is not to
// need to do anything upon the reference count to the parent Renderbuffer incrementing
// or decrementing. 
void RenderbufferInterface::addProxyRef(const Renderbuffer *proxy)
{
}

void RenderbufferInterface::releaseProxy(const Renderbuffer *proxy)
{
}

///// RenderbufferTexture2D Implementation ////////

RenderbufferTexture2D::RenderbufferTexture2D(Texture2D *texture, GLint level) : mLevel(level)
{
    mTexture2D.set(texture);
}

RenderbufferTexture2D::~RenderbufferTexture2D()
{
    mTexture2D.set(NULL);
}

// Textures need to maintain their own reference count for references via
// Renderbuffers acting as proxies. Here, we notify the texture of a reference.
void RenderbufferTexture2D::addProxyRef(const Renderbuffer *proxy)
{
    mTexture2D->addProxyRef(proxy);
}

void RenderbufferTexture2D::releaseProxy(const Renderbuffer *proxy)
{
    mTexture2D->releaseProxy(proxy);
}

rx::RenderTarget *RenderbufferTexture2D::getRenderTarget()
{
    return mTexture2D->getRenderTarget(mLevel);
}

rx::RenderTarget *RenderbufferTexture2D::getDepthStencil()
{
    return mTexture2D->getDepthSencil(mLevel);
}

rx::TextureStorage *RenderbufferTexture2D::getTextureStorage()
{
    return mTexture2D->getNativeTexture()->getStorageInstance();
}

GLsizei RenderbufferTexture2D::getWidth() const
{
    return mTexture2D->getWidth(mLevel);
}

GLsizei RenderbufferTexture2D::getHeight() const
{
    return mTexture2D->getHeight(mLevel);
}

GLenum RenderbufferTexture2D::getInternalFormat() const
{
    return mTexture2D->getInternalFormat(mLevel);
}

GLenum RenderbufferTexture2D::getActualFormat() const
{
    return mTexture2D->getActualFormat(mLevel);
}

GLsizei RenderbufferTexture2D::getSamples() const
{
    return 0;
}

unsigned int RenderbufferTexture2D::getSerial() const
{
    return mTexture2D->getRenderTargetSerial(mLevel);
}

unsigned int RenderbufferTexture2D::getTextureSerial() const
{
    return mTexture2D->getTextureSerial();
}

///// RenderbufferTextureCubeMap Implementation ////////

RenderbufferTextureCubeMap::RenderbufferTextureCubeMap(TextureCubeMap *texture, GLenum faceTarget, GLint level)
    : mFaceTarget(faceTarget), mLevel(level)
{
    mTextureCubeMap.set(texture);
}

RenderbufferTextureCubeMap::~RenderbufferTextureCubeMap()
{
    mTextureCubeMap.set(NULL);
}

// Textures need to maintain their own reference count for references via
// Renderbuffers acting as proxies. Here, we notify the texture of a reference.
void RenderbufferTextureCubeMap::addProxyRef(const Renderbuffer *proxy)
{
    mTextureCubeMap->addProxyRef(proxy);
}

void RenderbufferTextureCubeMap::releaseProxy(const Renderbuffer *proxy)
{
    mTextureCubeMap->releaseProxy(proxy);
}

rx::RenderTarget *RenderbufferTextureCubeMap::getRenderTarget()
{
    return mTextureCubeMap->getRenderTarget(mFaceTarget, mLevel);
}

rx::RenderTarget *RenderbufferTextureCubeMap::getDepthStencil()
{
    return mTextureCubeMap->getDepthStencil(mFaceTarget, mLevel);
}

rx::TextureStorage *RenderbufferTextureCubeMap::getTextureStorage()
{
    return mTextureCubeMap->getNativeTexture()->getStorageInstance();
}

GLsizei RenderbufferTextureCubeMap::getWidth() const
{
    return mTextureCubeMap->getWidth(mFaceTarget, mLevel);
}

GLsizei RenderbufferTextureCubeMap::getHeight() const
{
    return mTextureCubeMap->getHeight(mFaceTarget, mLevel);
}

GLenum RenderbufferTextureCubeMap::getInternalFormat() const
{
    return mTextureCubeMap->getInternalFormat(mFaceTarget, mLevel);
}

GLenum RenderbufferTextureCubeMap::getActualFormat() const
{
    return mTextureCubeMap->getActualFormat(mFaceTarget, mLevel);
}

GLsizei RenderbufferTextureCubeMap::getSamples() const
{
    return 0;
}

unsigned int RenderbufferTextureCubeMap::getSerial() const
{
    return mTextureCubeMap->getRenderTargetSerial(mFaceTarget, mLevel);
}

unsigned int RenderbufferTextureCubeMap::getTextureSerial() const
{
    return mTextureCubeMap->getTextureSerial();
}

///// RenderbufferTexture3DLayer Implementation ////////

RenderbufferTexture3DLayer::RenderbufferTexture3DLayer(Texture3D *texture, GLint level, GLint layer)
    : mLevel(level), mLayer(layer)
{
    mTexture3D.set(texture);
}

RenderbufferTexture3DLayer::~RenderbufferTexture3DLayer()
{
    mTexture3D.set(NULL);
}

// Textures need to maintain their own reference count for references via
// Renderbuffers acting as proxies. Here, we notify the texture of a reference.
void RenderbufferTexture3DLayer::addProxyRef(const Renderbuffer *proxy)
{
    mTexture3D->addProxyRef(proxy);
}

void RenderbufferTexture3DLayer::releaseProxy(const Renderbuffer *proxy)
{
    mTexture3D->releaseProxy(proxy);
}

rx::RenderTarget *RenderbufferTexture3DLayer::getRenderTarget()
{
    return mTexture3D->getRenderTarget(mLevel, mLayer);
}

rx::RenderTarget *RenderbufferTexture3DLayer::getDepthStencil()
{
    return mTexture3D->getDepthStencil(mLevel, mLayer);
}

rx::TextureStorage *RenderbufferTexture3DLayer::getTextureStorage()
{
    return mTexture3D->getNativeTexture()->getStorageInstance();
}

GLsizei RenderbufferTexture3DLayer::getWidth() const
{
    return mTexture3D->getWidth(mLevel);
}

GLsizei RenderbufferTexture3DLayer::getHeight() const
{
    return mTexture3D->getHeight(mLevel);
}

GLenum RenderbufferTexture3DLayer::getInternalFormat() const
{
    return mTexture3D->getInternalFormat(mLevel);
}

GLenum RenderbufferTexture3DLayer::getActualFormat() const
{
    return mTexture3D->getActualFormat(mLevel);
}

GLsizei RenderbufferTexture3DLayer::getSamples() const
{
    return 0;
}

unsigned int RenderbufferTexture3DLayer::getSerial() const
{
    return mTexture3D->getRenderTargetSerial(mLevel, mLayer);
}

unsigned int RenderbufferTexture3DLayer::getTextureSerial() const
{
    return mTexture3D->getTextureSerial();
}

////// RenderbufferTexture2DArrayLayer Implementation //////

RenderbufferTexture2DArrayLayer::RenderbufferTexture2DArrayLayer(Texture2DArray *texture, GLint level, GLint layer)
    : mLevel(level), mLayer(layer)
{
    mTexture2DArray.set(texture);
}

RenderbufferTexture2DArrayLayer::~RenderbufferTexture2DArrayLayer()
{
    mTexture2DArray.set(NULL);
}

void RenderbufferTexture2DArrayLayer::addProxyRef(const Renderbuffer *proxy)
{
    mTexture2DArray->addProxyRef(proxy);
}

void RenderbufferTexture2DArrayLayer::releaseProxy(const Renderbuffer *proxy)
{
    mTexture2DArray->releaseProxy(proxy);
}

rx::RenderTarget *RenderbufferTexture2DArrayLayer::getRenderTarget()
{
    return mTexture2DArray->getRenderTarget(mLevel, mLayer);
}

rx::RenderTarget *RenderbufferTexture2DArrayLayer::getDepthStencil()
{
    return mTexture2DArray->getDepthStencil(mLevel, mLayer);
}

rx::TextureStorage *RenderbufferTexture2DArrayLayer::getTextureStorage()
{
    return mTexture2DArray->getNativeTexture()->getStorageInstance();
}

GLsizei RenderbufferTexture2DArrayLayer::getWidth() const
{
    return mTexture2DArray->getWidth(mLevel);
}

GLsizei RenderbufferTexture2DArrayLayer::getHeight() const
{
    return mTexture2DArray->getHeight(mLevel);
}

GLenum RenderbufferTexture2DArrayLayer::getInternalFormat() const
{
    return mTexture2DArray->getInternalFormat(mLevel);
}

GLenum RenderbufferTexture2DArrayLayer::getActualFormat() const
{
    return mTexture2DArray->getActualFormat(mLevel);
}

GLsizei RenderbufferTexture2DArrayLayer::getSamples() const
{
    return 0;
}

unsigned int RenderbufferTexture2DArrayLayer::getSerial() const
{
    return mTexture2DArray->getRenderTargetSerial(mLevel, mLayer);
}

unsigned int RenderbufferTexture2DArrayLayer::getTextureSerial() const
{
    return mTexture2DArray->getTextureSerial();
}

////// Renderbuffer Implementation //////

Renderbuffer::Renderbuffer(rx::Renderer *renderer, GLuint id, RenderbufferInterface *instance) : RefCountObject(id)
{
    ASSERT(instance != NULL);
    mInstance = instance;

    ASSERT(renderer != NULL);
    mRenderer = renderer;
}

Renderbuffer::~Renderbuffer()
{
    delete mInstance;
}

// The RenderbufferInterface contained in this Renderbuffer may need to maintain
// its own reference count, so we pass it on here.
void Renderbuffer::addRef() const
{
    mInstance->addProxyRef(this);

    RefCountObject::addRef();
}

void Renderbuffer::release() const
{
    mInstance->releaseProxy(this);

    RefCountObject::release();
}

rx::RenderTarget *Renderbuffer::getRenderTarget()
{
    return mInstance->getRenderTarget();
}

rx::RenderTarget *Renderbuffer::getDepthStencil()
{
    return mInstance->getDepthStencil();
}

rx::TextureStorage *Renderbuffer::getTextureStorage()
{
    return mInstance->getTextureStorage();
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

GLenum Renderbuffer::getActualFormat() const
{
    return mInstance->getActualFormat();
}

GLuint Renderbuffer::getRedSize() const
{
    return gl::GetRedBits(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLuint Renderbuffer::getGreenSize() const
{
    return gl::GetGreenBits(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLuint Renderbuffer::getBlueSize() const
{
    return gl::GetBlueBits(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLuint Renderbuffer::getAlphaSize() const
{
    return gl::GetAlphaBits(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLuint Renderbuffer::getDepthSize() const
{
    return gl::GetDepthBits(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLuint Renderbuffer::getStencilSize() const
{
    return gl::GetStencilBits(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLenum Renderbuffer::getComponentType() const
{
    return gl::GetComponentType(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLenum Renderbuffer::getColorEncoding() const
{
    return gl::GetColorEncoding(getActualFormat(), mRenderer->getCurrentClientVersion());
}

GLsizei Renderbuffer::getSamples() const
{
    return mInstance->getSamples();
}

unsigned int Renderbuffer::getSerial() const
{
    return mInstance->getSerial();
}

unsigned int Renderbuffer::getTextureSerial() const
{
    return mInstance->getTextureSerial();
}

void Renderbuffer::setStorage(RenderbufferStorage *newStorage)
{
    ASSERT(newStorage != NULL);

    delete mInstance;
    mInstance = newStorage;
}

RenderbufferStorage::RenderbufferStorage() : mSerial(issueSerials(1))
{
    mWidth = 0;
    mHeight = 0;
    mInternalFormat = GL_RGBA4;
    mActualFormat = GL_RGBA8_OES;
    mSamples = 0;
}

RenderbufferStorage::~RenderbufferStorage()
{
}

rx::RenderTarget *RenderbufferStorage::getRenderTarget()
{
    return NULL;
}

rx::RenderTarget *RenderbufferStorage::getDepthStencil()
{
    return NULL;
}

rx::TextureStorage *RenderbufferStorage::getTextureStorage()
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

GLenum RenderbufferStorage::getActualFormat() const
{
    return mActualFormat;
}

GLsizei RenderbufferStorage::getSamples() const
{
    return mSamples;
}

unsigned int RenderbufferStorage::getSerial() const
{
    return mSerial;
}

unsigned int RenderbufferStorage::issueSerials(GLuint count)
{
    unsigned int firstSerial = mCurrentSerial;
    mCurrentSerial += count;
    return firstSerial;
}

Colorbuffer::Colorbuffer(rx::Renderer *renderer, rx::SwapChain *swapChain)
{
    mRenderTarget = renderer->createRenderTarget(swapChain, false); 

    if (mRenderTarget)
    {
        mWidth = mRenderTarget->getWidth();
        mHeight = mRenderTarget->getHeight();
        mInternalFormat = mRenderTarget->getInternalFormat();
        mActualFormat = mRenderTarget->getActualFormat();
        mSamples = mRenderTarget->getSamples();
    }
}

Colorbuffer::Colorbuffer(rx::Renderer *renderer, int width, int height, GLenum format, GLsizei samples) : mRenderTarget(NULL)
{
    mRenderTarget = renderer->createRenderTarget(width, height, format, samples);

    if (mRenderTarget)
    {
        mWidth = width;
        mHeight = height;
        mInternalFormat = format;
        mActualFormat = mRenderTarget->getActualFormat();
        mSamples = mRenderTarget->getSamples();
    }
}

Colorbuffer::~Colorbuffer()
{
    if (mRenderTarget)
    {
        delete mRenderTarget;
    }
}

rx::RenderTarget *Colorbuffer::getRenderTarget()
{
    return mRenderTarget;
}

DepthStencilbuffer::DepthStencilbuffer(rx::Renderer *renderer, rx::SwapChain *swapChain)
{
    mDepthStencil = renderer->createRenderTarget(swapChain, true);
    if (mDepthStencil)
    {
        mWidth = mDepthStencil->getWidth();
        mHeight = mDepthStencil->getHeight();
        mInternalFormat = mDepthStencil->getInternalFormat();
        mSamples = mDepthStencil->getSamples();
        mActualFormat = mDepthStencil->getActualFormat();
    }
}

DepthStencilbuffer::DepthStencilbuffer(rx::Renderer *renderer, int width, int height, GLsizei samples)
{

    mDepthStencil = renderer->createRenderTarget(width, height, GL_DEPTH24_STENCIL8_OES, samples);

    mWidth = mDepthStencil->getWidth();
    mHeight = mDepthStencil->getHeight();
    mInternalFormat = GL_DEPTH24_STENCIL8_OES;
    mActualFormat = mDepthStencil->getActualFormat();
    mSamples = mDepthStencil->getSamples();
}

DepthStencilbuffer::~DepthStencilbuffer()
{
    if (mDepthStencil)
    {
        delete mDepthStencil;
    }
}

rx::RenderTarget *DepthStencilbuffer::getDepthStencil()
{
    return mDepthStencil;
}

Depthbuffer::Depthbuffer(rx::Renderer *renderer, int width, int height, GLsizei samples) : DepthStencilbuffer(renderer, width, height, samples)
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

Stencilbuffer::Stencilbuffer(rx::Renderer *renderer, int width, int height, GLsizei samples) : DepthStencilbuffer(renderer, width, height, samples)
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
