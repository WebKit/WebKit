#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image9.cpp: Implements the rx::Image9 class, which acts as the interface to
// the actual underlying surfaces of a Texture.

#include "libGLESv2/renderer/d3d9/Image9.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/renderer/d3d9/Renderer9.h"
#include "libGLESv2/renderer/d3d9/RenderTarget9.h"
#include "libGLESv2/renderer/d3d9/TextureStorage9.h"

#include "libGLESv2/renderer/d3d9/renderer9_utils.h"
#include "libGLESv2/renderer/d3d9/formatutils9.h"

namespace rx
{

Image9::Image9()
{
    mSurface = NULL;
    mRenderer = NULL;

    mD3DPool = D3DPOOL_SYSTEMMEM;
    mD3DFormat = D3DFMT_UNKNOWN;
}

Image9::~Image9()
{
    SafeRelease(mSurface);
}

void Image9::generateMip(IDirect3DSurface9 *destSurface, IDirect3DSurface9 *sourceSurface)
{
    D3DSURFACE_DESC destDesc;
    HRESULT result = destSurface->GetDesc(&destDesc);
    ASSERT(SUCCEEDED(result));

    D3DSURFACE_DESC sourceDesc;
    result = sourceSurface->GetDesc(&sourceDesc);
    ASSERT(SUCCEEDED(result));

    ASSERT(sourceDesc.Format == destDesc.Format);
    ASSERT(sourceDesc.Width == 1 || sourceDesc.Width / 2 == destDesc.Width);
    ASSERT(sourceDesc.Height == 1 || sourceDesc.Height / 2 == destDesc.Height);

    MipGenerationFunction mipFunction = d3d9::GetMipGenerationFunction(sourceDesc.Format);
    ASSERT(mipFunction != NULL);

    D3DLOCKED_RECT sourceLocked = {0};
    result = sourceSurface->LockRect(&sourceLocked, NULL, D3DLOCK_READONLY);
    ASSERT(SUCCEEDED(result));

    D3DLOCKED_RECT destLocked = {0};
    result = destSurface->LockRect(&destLocked, NULL, 0);
    ASSERT(SUCCEEDED(result));

    const unsigned char *sourceData = reinterpret_cast<const unsigned char*>(sourceLocked.pBits);
    unsigned char *destData = reinterpret_cast<unsigned char*>(destLocked.pBits);

    if (sourceData && destData)
    {
        mipFunction(sourceDesc.Width, sourceDesc.Height, 1, sourceData, sourceLocked.Pitch, 0,
                    destData, destLocked.Pitch, 0);
    }

    destSurface->UnlockRect();
    sourceSurface->UnlockRect();
}

Image9 *Image9::makeImage9(Image *img)
{
    ASSERT(HAS_DYNAMIC_TYPE(rx::Image9*, img));
    return static_cast<rx::Image9*>(img);
}

void Image9::generateMipmap(Image9 *dest, Image9 *source)
{
    IDirect3DSurface9 *sourceSurface = source->getSurface();
    if (sourceSurface == NULL)
        return gl::error(GL_OUT_OF_MEMORY);

    IDirect3DSurface9 *destSurface = dest->getSurface();
    generateMip(destSurface, sourceSurface);

    dest->markDirty();
}

void Image9::copyLockableSurfaces(IDirect3DSurface9 *dest, IDirect3DSurface9 *source)
{
    D3DLOCKED_RECT sourceLock = {0};
    D3DLOCKED_RECT destLock = {0};
    
    source->LockRect(&sourceLock, NULL, 0);
    dest->LockRect(&destLock, NULL, 0);
    
    if (sourceLock.pBits && destLock.pBits)
    {
        D3DSURFACE_DESC desc;
        source->GetDesc(&desc);

        int blockHeight = d3d9::GetBlockHeight(desc.Format);
        int rows = desc.Height / blockHeight;

        int bytes = d3d9::GetBlockSize(desc.Format, desc.Width, blockHeight);
        ASSERT(bytes <= sourceLock.Pitch && bytes <= destLock.Pitch);

        for(int i = 0; i < rows; i++)
        {
            memcpy((char*)destLock.pBits + destLock.Pitch * i, (char*)sourceLock.pBits + sourceLock.Pitch * i, bytes);
        }

        source->UnlockRect();
        dest->UnlockRect();
    }
    else UNREACHABLE();
}

bool Image9::redefine(rx::Renderer *renderer, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, bool forceRelease)
{
    // 3D textures are not supported by the D3D9 backend.
    ASSERT(depth <= 1);

    // Only 2D and cube texture are supported by the D3D9 backend.
    ASSERT(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP);

    if (mWidth != width ||
        mHeight != height ||
        mDepth != depth ||
        mInternalFormat != internalformat ||
        forceRelease)
    {
        mRenderer = Renderer9::makeRenderer9(renderer);

        mWidth = width;
        mHeight = height;
        mDepth = depth;
        mInternalFormat = internalformat;

        // compute the d3d format that will be used
        mD3DFormat = gl_d3d9::GetTextureFormat(internalformat, mRenderer);
        mActualFormat = d3d9_gl::GetInternalFormat(mD3DFormat);
        mRenderable = gl_d3d9::GetRenderFormat(internalformat, mRenderer) != D3DFMT_UNKNOWN;

        SafeRelease(mSurface);
        mDirty = gl_d3d9::RequiresTextureDataInitialization(mInternalFormat);

        return true;
    }

    return false;
}

void Image9::createSurface()
{
    if(mSurface)
    {
        return;
    }

    IDirect3DTexture9 *newTexture = NULL;
    IDirect3DSurface9 *newSurface = NULL;
    const D3DPOOL poolToUse = D3DPOOL_SYSTEMMEM;
    const D3DFORMAT d3dFormat = getD3DFormat();

    if (mWidth != 0 && mHeight != 0)
    {
        int levelToFetch = 0;
        GLsizei requestWidth = mWidth;
        GLsizei requestHeight = mHeight;
        d3d9::MakeValidSize(true, d3dFormat, &requestWidth, &requestHeight, &levelToFetch);

        IDirect3DDevice9 *device = mRenderer->getDevice();

        HRESULT result = device->CreateTexture(requestWidth, requestHeight, levelToFetch + 1, 0, d3dFormat,
                                                    poolToUse, &newTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            ERR("Creating image surface failed.");
            return gl::error(GL_OUT_OF_MEMORY);
        }

        newTexture->GetSurfaceLevel(levelToFetch, &newSurface);
        SafeRelease(newTexture);

        if (gl_d3d9::RequiresTextureDataInitialization(mInternalFormat))
        {
            InitializeTextureDataFunction initializeFunc = gl_d3d9::GetTextureDataInitializationFunction(mInternalFormat);

            RECT entireRect;
            entireRect.left = 0;
            entireRect.right = mWidth;
            entireRect.top = 0;
            entireRect.bottom = mHeight;

            D3DLOCKED_RECT lockedRect;
            result = newSurface->LockRect(&lockedRect, &entireRect, 0);
            ASSERT(SUCCEEDED(result));

            initializeFunc(mWidth, mHeight, 1, lockedRect.pBits, lockedRect.Pitch, 0);

            result = newSurface->UnlockRect();
            ASSERT(SUCCEEDED(result));
        }
    }

    mSurface = newSurface;
    mDirty = false;
    mD3DPool = poolToUse;
}

HRESULT Image9::lock(D3DLOCKED_RECT *lockedRect, const RECT *rect)
{
    createSurface();

    HRESULT result = D3DERR_INVALIDCALL;

    if (mSurface)
    {
        result = mSurface->LockRect(lockedRect, rect, 0);
        ASSERT(SUCCEEDED(result));

        mDirty = true;
    }

    return result;
}

void Image9::unlock()
{
    if (mSurface)
    {
        HRESULT result = mSurface->UnlockRect();
        ASSERT(SUCCEEDED(result));
    }
}

D3DFORMAT Image9::getD3DFormat() const
{
    // this should only happen if the image hasn't been redefined first
    // which would be a bug by the caller
    ASSERT(mD3DFormat != D3DFMT_UNKNOWN);

    return mD3DFormat;
}

bool Image9::isDirty() const
{
    // Make sure to that this image is marked as dirty even if the staging texture hasn't been created yet
    // if initialization is required before use.
    return (mSurface || gl_d3d9::RequiresTextureDataInitialization(mInternalFormat)) && mDirty;
}

IDirect3DSurface9 *Image9::getSurface()
{
    createSurface();

    return mSurface;
}

void Image9::setManagedSurface(TextureStorageInterface2D *storage, int level)
{
    TextureStorage9_2D *storage9 = TextureStorage9_2D::makeTextureStorage9_2D(storage->getStorageInstance());
    setManagedSurface(storage9->getSurfaceLevel(level, false));
}

void Image9::setManagedSurface(TextureStorageInterfaceCube *storage, int face, int level)
{
    TextureStorage9_Cube *storage9 = TextureStorage9_Cube::makeTextureStorage9_Cube(storage->getStorageInstance());
    setManagedSurface(storage9->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, false));
}

void Image9::setManagedSurface(IDirect3DSurface9 *surface)
{
    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);
    ASSERT(desc.Pool == D3DPOOL_MANAGED);

    if ((GLsizei)desc.Width == mWidth && (GLsizei)desc.Height == mHeight)
    {
        if (mSurface)
        {
            copyLockableSurfaces(surface, mSurface);
            SafeRelease(mSurface);
        }

        mSurface = surface;
        mD3DPool = desc.Pool;
    }
}

bool Image9::copyToStorage(TextureStorageInterface2D *storage, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(getSurface() != NULL);
    TextureStorage9_2D *storage9 = TextureStorage9_2D::makeTextureStorage9_2D(storage->getStorageInstance());
    return copyToSurface(storage9->getSurfaceLevel(level, true), xoffset, yoffset, width, height);
}

bool Image9::copyToStorage(TextureStorageInterfaceCube *storage, int face, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(getSurface() != NULL);
    TextureStorage9_Cube *storage9 = TextureStorage9_Cube::makeTextureStorage9_Cube(storage->getStorageInstance());
    return copyToSurface(storage9->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, true), xoffset, yoffset, width, height);
}

bool Image9::copyToStorage(TextureStorageInterface3D *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth)
{
    // 3D textures are not supported by the D3D9 backend.
    UNREACHABLE();
    return false;
}

bool Image9::copyToStorage(TextureStorageInterface2DArray *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height)
{
    // 2D array textures are not supported by the D3D9 backend.
    UNREACHABLE();
    return false;
}

bool Image9::copyToSurface(IDirect3DSurface9 *destSurface, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(width > 0 && height > 0);

    if (!destSurface)
        return false;

    IDirect3DSurface9 *sourceSurface = getSurface();

    if (sourceSurface && sourceSurface != destSurface)
    {
        RECT rect;
        rect.left = xoffset;
        rect.top = yoffset;
        rect.right = xoffset + width;
        rect.bottom = yoffset + height;

        POINT point = {rect.left, rect.top};

        IDirect3DDevice9 *device = mRenderer->getDevice();

        if (mD3DPool == D3DPOOL_MANAGED)
        {
            D3DSURFACE_DESC desc;
            sourceSurface->GetDesc(&desc);

            IDirect3DSurface9 *surf = 0;
            HRESULT result = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &surf, NULL);

            if (SUCCEEDED(result))
            {
                copyLockableSurfaces(surf, sourceSurface);
                result = device->UpdateSurface(surf, &rect, destSurface, &point);
                ASSERT(SUCCEEDED(result));
                SafeRelease(surf);
            }
        }
        else
        {
            // UpdateSurface: source must be SYSTEMMEM, dest must be DEFAULT pools 
            HRESULT result = device->UpdateSurface(sourceSurface, &rect, destSurface, &point);
            ASSERT(SUCCEEDED(result));
        }
    }

    SafeRelease(destSurface);
    return true;
}

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the target pixel rectangle.
void Image9::loadData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                      GLint unpackAlignment, GLenum type, const void *input)
{
    // 3D textures are not supported by the D3D9 backend.
    ASSERT(zoffset == 0 && depth == 1);

    GLuint clientVersion = mRenderer->getCurrentClientVersion();
    GLsizei inputRowPitch = gl::GetRowPitch(mInternalFormat, type, clientVersion, width, unpackAlignment);

    LoadImageFunction loadFunction = d3d9::GetImageLoadFunction(mInternalFormat, mRenderer);
    ASSERT(loadFunction != NULL);

    RECT lockRect =
    {
        xoffset, yoffset,
        xoffset + width, yoffset + height
    };

    D3DLOCKED_RECT locked;
    HRESULT result = lock(&locked, &lockRect);
    if (FAILED(result))
    {
        return;
    }

    loadFunction(width, height, depth, input, inputRowPitch, 0, locked.pBits, locked.Pitch, 0);

    unlock();
}

void Image9::loadCompressedData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                const void *input)
{
    // 3D textures are not supported by the D3D9 backend.
    ASSERT(zoffset == 0 && depth == 1);

    GLuint clientVersion = mRenderer->getCurrentClientVersion();
    GLsizei inputRowPitch = gl::GetRowPitch(mInternalFormat, GL_UNSIGNED_BYTE, clientVersion, width, 1);
    GLsizei inputDepthPitch = gl::GetDepthPitch(mInternalFormat, GL_UNSIGNED_BYTE, clientVersion, width, height, 1);

    GLuint outputBlockWidth = d3d9::GetBlockWidth(mD3DFormat);
    GLuint outputBlockHeight = d3d9::GetBlockHeight(mD3DFormat);

    ASSERT(xoffset % outputBlockWidth == 0);
    ASSERT(yoffset % outputBlockHeight == 0);

    LoadImageFunction loadFunction = d3d9::GetImageLoadFunction(mInternalFormat, mRenderer);
    ASSERT(loadFunction != NULL);

    RECT lockRect =
    {
        xoffset, yoffset,
        xoffset + width, yoffset + height
    };

    D3DLOCKED_RECT locked;
    HRESULT result = lock(&locked, &lockRect);
    if (FAILED(result))
    {
        return;
    }

    loadFunction(width, height, depth, input, inputRowPitch, inputDepthPitch,
                 locked.pBits, locked.Pitch, 0);

    unlock();
}

// This implements glCopyTex[Sub]Image2D for non-renderable internal texture formats and incomplete textures
void Image9::copy(GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    // ES3.0 only behaviour to copy into a 3d texture
    ASSERT(zoffset == 0);

    RenderTarget9 *renderTarget = NULL;
    IDirect3DSurface9 *surface = NULL;
    gl::Renderbuffer *colorbuffer = source->getColorbuffer(0);

    if (colorbuffer)
    {
        renderTarget = RenderTarget9::makeRenderTarget9(colorbuffer->getRenderTarget());
    }
    
    if (renderTarget)
    {
        surface = renderTarget->getSurface();
    }

    if (!surface)
    {
        ERR("Failed to retrieve the render target.");
        return gl::error(GL_OUT_OF_MEMORY);
    }

    IDirect3DDevice9 *device = mRenderer->getDevice();

    IDirect3DSurface9 *renderTargetData = NULL;
    D3DSURFACE_DESC description;
    surface->GetDesc(&description);
    
    HRESULT result = device->CreateOffscreenPlainSurface(description.Width, description.Height, description.Format, D3DPOOL_SYSTEMMEM, &renderTargetData, NULL);

    if (FAILED(result))
    {
        ERR("Could not create matching destination surface.");
        SafeRelease(surface);
        return gl::error(GL_OUT_OF_MEMORY);
    }

    result = device->GetRenderTargetData(surface, renderTargetData);

    if (FAILED(result))
    {
        ERR("GetRenderTargetData unexpectedly failed.");
        SafeRelease(renderTargetData);
        SafeRelease(surface);
        return gl::error(GL_OUT_OF_MEMORY);
    }

    RECT sourceRect = {x, y, x + width, y + height};
    RECT destRect = {xoffset, yoffset, xoffset + width, yoffset + height};

    D3DLOCKED_RECT sourceLock = {0};
    result = renderTargetData->LockRect(&sourceLock, &sourceRect, 0);

    if (FAILED(result))
    {
        ERR("Failed to lock the source surface (rectangle might be invalid).");
        SafeRelease(renderTargetData);
        SafeRelease(surface);
        return gl::error(GL_OUT_OF_MEMORY);
    }

    D3DLOCKED_RECT destLock = {0};
    result = lock(&destLock, &destRect);
    
    if (FAILED(result))
    {
        ERR("Failed to lock the destination surface (rectangle might be invalid).");
        renderTargetData->UnlockRect();
        SafeRelease(renderTargetData);
        SafeRelease(surface);
        return gl::error(GL_OUT_OF_MEMORY);
    }

    if (destLock.pBits && sourceLock.pBits)
    {
        unsigned char *source = (unsigned char*)sourceLock.pBits;
        unsigned char *dest = (unsigned char*)destLock.pBits;

        switch (description.Format)
        {
          case D3DFMT_X8R8G8B8:
          case D3DFMT_A8R8G8B8:
            switch(getD3DFormat())
            {
              case D3DFMT_X8R8G8B8:
              case D3DFMT_A8R8G8B8:
                for(int y = 0; y < height; y++)
                {
                    memcpy(dest, source, 4 * width);

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              case D3DFMT_L8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        dest[x] = source[x * 4 + 2];
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              case D3DFMT_A8L8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        dest[x * 2 + 0] = source[x * 4 + 2];
                        dest[x * 2 + 1] = source[x * 4 + 3];
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              default:
                UNREACHABLE();
            }
            break;
          case D3DFMT_R5G6B5:
            switch(getD3DFormat())
            {
              case D3DFMT_X8R8G8B8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        unsigned short rgb = ((unsigned short*)source)[x];
                        unsigned char red = (rgb & 0xF800) >> 8;
                        unsigned char green = (rgb & 0x07E0) >> 3;
                        unsigned char blue = (rgb & 0x001F) << 3;
                        dest[x + 0] = blue | (blue >> 5);
                        dest[x + 1] = green | (green >> 6);
                        dest[x + 2] = red | (red >> 5);
                        dest[x + 3] = 0xFF;
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              case D3DFMT_L8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        unsigned char red = source[x * 2 + 1] & 0xF8;
                        dest[x] = red | (red >> 5);
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              default:
                UNREACHABLE();
            }
            break;
          case D3DFMT_A1R5G5B5:
            switch(getD3DFormat())
            {
              case D3DFMT_X8R8G8B8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        unsigned short argb = ((unsigned short*)source)[x];
                        unsigned char red = (argb & 0x7C00) >> 7;
                        unsigned char green = (argb & 0x03E0) >> 2;
                        unsigned char blue = (argb & 0x001F) << 3;
                        dest[x + 0] = blue | (blue >> 5);
                        dest[x + 1] = green | (green >> 5);
                        dest[x + 2] = red | (red >> 5);
                        dest[x + 3] = 0xFF;
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              case D3DFMT_A8R8G8B8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        unsigned short argb = ((unsigned short*)source)[x];
                        unsigned char red = (argb & 0x7C00) >> 7;
                        unsigned char green = (argb & 0x03E0) >> 2;
                        unsigned char blue = (argb & 0x001F) << 3;
                        unsigned char alpha = (signed short)argb >> 15;
                        dest[x + 0] = blue | (blue >> 5);
                        dest[x + 1] = green | (green >> 5);
                        dest[x + 2] = red | (red >> 5);
                        dest[x + 3] = alpha;
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              case D3DFMT_L8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        unsigned char red = source[x * 2 + 1] & 0x7C;
                        dest[x] = (red << 1) | (red >> 4);
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              case D3DFMT_A8L8:
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x < width; x++)
                    {
                        unsigned char red = source[x * 2 + 1] & 0x7C;
                        dest[x * 2 + 0] = (red << 1) | (red >> 4);
                        dest[x * 2 + 1] = (signed char)source[x * 2 + 1] >> 7;
                    }

                    source += sourceLock.Pitch;
                    dest += destLock.Pitch;
                }
                break;
              default:
                UNREACHABLE();
            }
            break;
          default:
            UNREACHABLE();
        }
    }

    unlock();
    renderTargetData->UnlockRect();

    SafeRelease(renderTargetData);
    SafeRelease(surface);

    mDirty = true;
}

}
