#include "precompiled.h"
//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage9.cpp: Implements the abstract rx::TextureStorage9 class and its concrete derived
// classes TextureStorage9_2D and TextureStorage9_Cube, which act as the interface to the
// D3D9 texture.

#include "libGLESv2/main.h"
#include "libGLESv2/renderer/d3d9/Renderer9.h"
#include "libGLESv2/renderer/d3d9/TextureStorage9.h"
#include "libGLESv2/renderer/d3d9/SwapChain9.h"
#include "libGLESv2/renderer/d3d9/RenderTarget9.h"
#include "libGLESv2/renderer/d3d9/renderer9_utils.h"
#include "libGLESv2/renderer/d3d9/formatutils9.h"
#include "libGLESv2/Texture.h"

namespace rx
{
TextureStorage9::TextureStorage9(Renderer *renderer, DWORD usage)
    : mTopLevel(0),
      mRenderer(Renderer9::makeRenderer9(renderer)),
      mD3DUsage(usage),
      mD3DPool(mRenderer->getTexturePool(usage))
{
}

TextureStorage9::~TextureStorage9()
{
}

TextureStorage9 *TextureStorage9::makeTextureStorage9(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage9*, storage));
    return static_cast<TextureStorage9*>(storage);
}

DWORD TextureStorage9::GetTextureUsage(GLenum internalformat, Renderer9 *renderer, bool renderTarget)
{
    GLuint clientVersion = renderer->getCurrentClientVersion();

    DWORD d3dusage = 0;

    if (gl::GetDepthBits(internalformat, clientVersion) > 0 ||
        gl::GetStencilBits(internalformat, clientVersion) > 0)
    {
        d3dusage |= D3DUSAGE_DEPTHSTENCIL;
    }
    else if (renderTarget && (gl_d3d9::GetRenderFormat(internalformat, renderer) != D3DFMT_UNKNOWN))
    {
        d3dusage |= D3DUSAGE_RENDERTARGET;
    }

    return d3dusage;
}


bool TextureStorage9::isRenderTarget() const
{
    return (mD3DUsage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0;
}

bool TextureStorage9::isManaged() const
{
    return (mD3DPool == D3DPOOL_MANAGED);
}

D3DPOOL TextureStorage9::getPool() const
{
    return mD3DPool;
}

DWORD TextureStorage9::getUsage() const
{
    return mD3DUsage;
}

int TextureStorage9::getTopLevel() const
{
    return mTopLevel;
}

int TextureStorage9::getLevelCount() const
{
    return getBaseTexture() ? (getBaseTexture()->GetLevelCount() - getTopLevel()) : 0;
}

TextureStorage9_2D::TextureStorage9_2D(Renderer *renderer, SwapChain9 *swapchain)
    : TextureStorage9(renderer, D3DUSAGE_RENDERTARGET)
{
    IDirect3DTexture9 *surfaceTexture = swapchain->getOffscreenTexture();
    mTexture = surfaceTexture;
    mRenderTarget = NULL;

    initializeRenderTarget();
}

TextureStorage9_2D::TextureStorage9_2D(Renderer *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels)
    : TextureStorage9(renderer, GetTextureUsage(internalformat, Renderer9::makeRenderer9(renderer), renderTarget))
{
    mTexture = NULL;
    mRenderTarget = NULL;
    // if the width or height is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (width > 0 && height > 0)
    {
        IDirect3DDevice9 *device = mRenderer->getDevice();
        D3DFORMAT format = gl_d3d9::GetTextureFormat(internalformat, mRenderer);
        d3d9::MakeValidSize(false, format, &width, &height, &mTopLevel);
        UINT creationLevels = (levels == 0) ? 0 : mTopLevel + levels;

        HRESULT result = device->CreateTexture(width, height, creationLevels, getUsage(), format, getPool(), &mTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            gl::error(GL_OUT_OF_MEMORY);
        }
    }

    initializeRenderTarget();
}

TextureStorage9_2D::~TextureStorage9_2D()
{
    SafeRelease(mTexture);
    SafeDelete(mRenderTarget);
}

TextureStorage9_2D *TextureStorage9_2D::makeTextureStorage9_2D(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage9_2D*, storage));
    return static_cast<TextureStorage9_2D*>(storage);
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *TextureStorage9_2D::getSurfaceLevel(int level, bool dirty)
{
    IDirect3DSurface9 *surface = NULL;

    if (mTexture)
    {
        HRESULT result = mTexture->GetSurfaceLevel(level + mTopLevel, &surface);
        ASSERT(SUCCEEDED(result));

        // With managed textures the driver needs to be informed of updates to the lower mipmap levels
        if (level + mTopLevel != 0 && isManaged() && dirty)
        {
            mTexture->AddDirtyRect(NULL);
        }
    }

    return surface;
}

RenderTarget *TextureStorage9_2D::getRenderTarget(int level)
{
    return mRenderTarget;
}

void TextureStorage9_2D::generateMipmap(int level)
{
    IDirect3DSurface9 *upper = getSurfaceLevel(level - 1, false);
    IDirect3DSurface9 *lower = getSurfaceLevel(level, true);

    if (upper != NULL && lower != NULL)
    {
        mRenderer->boxFilter(upper, lower);
    }

    SafeRelease(upper);
    SafeRelease(lower);
}

IDirect3DBaseTexture9 *TextureStorage9_2D::getBaseTexture() const
{
    return mTexture;
}

void TextureStorage9_2D::initializeRenderTarget()
{
    ASSERT(mRenderTarget == NULL);

    if (mTexture != NULL && isRenderTarget())
    {
        IDirect3DSurface9 *surface = getSurfaceLevel(0, false);

        mRenderTarget = new RenderTarget9(mRenderer, surface);
    }
}

TextureStorage9_Cube::TextureStorage9_Cube(Renderer *renderer, GLenum internalformat, bool renderTarget, int size, int levels)
    : TextureStorage9(renderer, GetTextureUsage(internalformat, Renderer9::makeRenderer9(renderer), renderTarget))
{
    mTexture = NULL;
    for (int i = 0; i < 6; ++i)
    {
        mRenderTarget[i] = NULL;
    }

    // if the size is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (size > 0)
    {
        IDirect3DDevice9 *device = mRenderer->getDevice();
        int height = size;
        D3DFORMAT format = gl_d3d9::GetTextureFormat(internalformat, mRenderer);
        d3d9::MakeValidSize(false, format, &size, &height, &mTopLevel);
        UINT creationLevels = (levels == 0) ? 0 : mTopLevel + levels;

        HRESULT result = device->CreateCubeTexture(size, creationLevels, getUsage(), format, getPool(), &mTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            gl::error(GL_OUT_OF_MEMORY);
        }
    }

    initializeRenderTarget();
}

TextureStorage9_Cube::~TextureStorage9_Cube()
{
    SafeRelease(mTexture);

    for (int i = 0; i < 6; ++i)
    {
        SafeDelete(mRenderTarget[i]);
    }
}

TextureStorage9_Cube *TextureStorage9_Cube::makeTextureStorage9_Cube(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage9_Cube*, storage));
    return static_cast<TextureStorage9_Cube*>(storage);
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *TextureStorage9_Cube::getCubeMapSurface(GLenum faceTarget, int level, bool dirty)
{
    IDirect3DSurface9 *surface = NULL;

    if (mTexture)
    {
        D3DCUBEMAP_FACES face = gl_d3d9::ConvertCubeFace(faceTarget);
        HRESULT result = mTexture->GetCubeMapSurface(face, level + mTopLevel, &surface);
        ASSERT(SUCCEEDED(result));

        // With managed textures the driver needs to be informed of updates to the lower mipmap levels
        if (level != 0 && isManaged() && dirty)
        {
            mTexture->AddDirtyRect(face, NULL);
        }
    }

    return surface;
}

RenderTarget *TextureStorage9_Cube::getRenderTargetFace(GLenum faceTarget, int level)
{
    return mRenderTarget[gl::TextureCubeMap::targetToIndex(faceTarget)];
}

void TextureStorage9_Cube::generateMipmap(int faceIndex, int level)
{
    IDirect3DSurface9 *upper = getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, level - 1, false);
    IDirect3DSurface9 *lower = getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, level, true);

    if (upper != NULL && lower != NULL)
    {
        mRenderer->boxFilter(upper, lower);
    }

    SafeRelease(upper);
    SafeRelease(lower);
}

IDirect3DBaseTexture9 *TextureStorage9_Cube::getBaseTexture() const
{
    return mTexture;
}

void TextureStorage9_Cube::initializeRenderTarget()
{
    if (mTexture != NULL && isRenderTarget())
    {
        IDirect3DSurface9 *surface = NULL;

        for (int i = 0; i < 6; ++i)
        {
            ASSERT(mRenderTarget[i] == NULL);

            surface = getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, false);

            mRenderTarget[i] = new RenderTarget9(mRenderer, surface);
        }
    }
}

}