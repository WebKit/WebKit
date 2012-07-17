//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Texture.cpp: Implements the gl::Texture class and its derived classes
// Texture2D and TextureCubeMap. Implements GL texture objects and related
// functionality. [OpenGL ES 2.0.24] section 3.7 page 63.

#include "libGLESv2/Texture.h"

#include <d3dx9tex.h>

#include <algorithm>
#include <intrin.h>

#include "common/debug.h"

#include "libEGL/Display.h"

#include "libGLESv2/main.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"
#include "libGLESv2/Blit.h"
#include "libGLESv2/Framebuffer.h"

namespace gl
{
unsigned int TextureStorage::mCurrentTextureSerial = 1;

static D3DFORMAT ConvertTextureFormatType(GLenum format, GLenum type)
{
    if (IsDepthTexture(format))
    {
        return D3DFMT_INTZ;
    }
    else if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
             format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
    {
        return D3DFMT_DXT1;
    }
    else if (format == GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE)
    {
        return D3DFMT_DXT3;
    }
    else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE)
    {
        return D3DFMT_DXT5;
    }
    else if (type == GL_FLOAT)
    {
        return D3DFMT_A32B32G32R32F;
    }
    else if (type == GL_HALF_FLOAT_OES)
    {
        return D3DFMT_A16B16G16R16F;
    }
    else if (type == GL_UNSIGNED_BYTE)
    {
        if (format == GL_LUMINANCE && getContext()->supportsLuminanceTextures())
        {
            return D3DFMT_L8;
        }
        else if (format == GL_LUMINANCE_ALPHA && getContext()->supportsLuminanceAlphaTextures())
        {
            return D3DFMT_A8L8;
        }
        else if (format == GL_RGB)
        {
            return D3DFMT_X8R8G8B8;
        }

        return D3DFMT_A8R8G8B8;
    }

    return D3DFMT_A8R8G8B8;
}

static bool IsTextureFormatRenderable(D3DFORMAT format)
{
    if (format == D3DFMT_INTZ)
    {
        return true;
    }
    switch(format)
    {
      case D3DFMT_L8:
      case D3DFMT_A8L8:
      case D3DFMT_DXT1:
      case D3DFMT_DXT3:
      case D3DFMT_DXT5:
        return false;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A16B16G16R16F:
      case D3DFMT_A32B32G32R32F:
        return true;
      default:
        UNREACHABLE();
    }

    return false;
}

static inline DWORD GetTextureUsage(D3DFORMAT d3dfmt, GLenum glusage, bool forceRenderable)
{
    DWORD d3dusage = 0;

    if (d3dfmt == D3DFMT_INTZ)
    {
        d3dusage |= D3DUSAGE_DEPTHSTENCIL;
    }
    else if(forceRenderable || (IsTextureFormatRenderable(d3dfmt) && (glusage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE)))
    {
        d3dusage |= D3DUSAGE_RENDERTARGET;
    }
    return d3dusage;
}

Image::Image()
{
    mWidth = 0; 
    mHeight = 0;
    mFormat = GL_NONE;
    mType = GL_UNSIGNED_BYTE;

    mSurface = NULL;

    mDirty = false;

    mD3DPool = D3DPOOL_SYSTEMMEM;
    mD3DFormat = D3DFMT_UNKNOWN;
}

Image::~Image()
{
    if (mSurface)
    {
        mSurface->Release();
    }
}

bool Image::redefine(GLenum format, GLsizei width, GLsizei height, GLenum type, bool forceRelease)
{
    if (mWidth != width ||
        mHeight != height ||
        mFormat != format ||
        mType != type ||
        forceRelease)
    {
        mWidth = width;
        mHeight = height;
        mFormat = format;
        mType = type;
        // compute the d3d format that will be used
        mD3DFormat = ConvertTextureFormatType(mFormat, mType);

        if (mSurface)
        {
            mSurface->Release();
            mSurface = NULL;
        }

        return true;
    }

    return false;
}

void Image::createSurface()
{
    if(mSurface)
    {
        return;
    }

    IDirect3DTexture9 *newTexture = NULL;
    IDirect3DSurface9 *newSurface = NULL;
    const D3DPOOL poolToUse = D3DPOOL_SYSTEMMEM;
    const D3DFORMAT d3dFormat = getD3DFormat();
    ASSERT(d3dFormat != D3DFMT_INTZ); // We should never get here for depth textures

    if (mWidth != 0 && mHeight != 0)
    {
        int levelToFetch = 0;
        GLsizei requestWidth = mWidth;
        GLsizei requestHeight = mHeight;
        if (IsCompressed(mFormat) && (mWidth % 4 != 0 || mHeight % 4 != 0))
        {
            bool isMult4 = false;
            int upsampleCount = 0;
            while (!isMult4)
            {
                requestWidth <<= 1;
                requestHeight <<= 1;
                upsampleCount++;
                if (requestWidth % 4 == 0 && requestHeight % 4 == 0)
                {
                    isMult4 = true;
                }
            }
            levelToFetch = upsampleCount;
        }

        HRESULT result = getDevice()->CreateTexture(requestWidth, requestHeight, levelToFetch + 1, NULL, d3dFormat,
                                                    poolToUse, &newTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            ERR("Creating image surface failed.");
            return error(GL_OUT_OF_MEMORY);
        }

        newTexture->GetSurfaceLevel(levelToFetch, &newSurface);
        newTexture->Release();
    }

    mSurface = newSurface;
    mDirty = false;
    mD3DPool = poolToUse;
}

HRESULT Image::lock(D3DLOCKED_RECT *lockedRect, const RECT *rect)
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

void Image::unlock()
{
    if (mSurface)
    {
        HRESULT result = mSurface->UnlockRect();
        ASSERT(SUCCEEDED(result));
    }
}

bool Image::isRenderableFormat() const
{    
    return IsTextureFormatRenderable(getD3DFormat());
}

D3DFORMAT Image::getD3DFormat() const
{
    // this should only happen if the image hasn't been redefined first
    // which would be a bug by the caller
    ASSERT(mD3DFormat != D3DFMT_UNKNOWN);

    return mD3DFormat;
}

IDirect3DSurface9 *Image::getSurface()
{
    createSurface();

    return mSurface;
}

void Image::setManagedSurface(IDirect3DSurface9 *surface)
{
    if (mSurface)
    {
        D3DXLoadSurfaceFromSurface(surface, NULL, NULL, mSurface, NULL, NULL, D3DX_FILTER_BOX, 0);
        mSurface->Release();
    }

    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);
    ASSERT(desc.Pool == D3DPOOL_MANAGED);

    mSurface = surface;
    mD3DPool = desc.Pool;
}

void Image::updateSurface(IDirect3DSurface9 *destSurface, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    IDirect3DSurface9 *sourceSurface = getSurface();

    if (sourceSurface && sourceSurface != destSurface)
    {
        RECT rect;
        rect.left = xoffset;
        rect.top = yoffset;
        rect.right = xoffset + width;
        rect.bottom = yoffset + height;

        if (mD3DPool == D3DPOOL_MANAGED)
        {
            HRESULT result = D3DXLoadSurfaceFromSurface(destSurface, NULL, &rect, sourceSurface, NULL, &rect, D3DX_FILTER_BOX, 0);
            ASSERT(SUCCEEDED(result));
        }
        else
        {
            // UpdateSurface: source must be SYSTEMMEM, dest must be DEFAULT pools 
            POINT point = {rect.left, rect.top};
            HRESULT result = getDevice()->UpdateSurface(sourceSurface, &rect, destSurface, &point);
            ASSERT(SUCCEEDED(result));
        }
    }
}

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the target pixel rectangle.
void Image::loadData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum type,
                     GLint unpackAlignment, const void *input)
{
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

    GLsizei inputPitch = ComputePitch(width, mFormat, type, unpackAlignment);

    switch (type)
    {
      case GL_UNSIGNED_BYTE:
        switch (mFormat)
        {
          case GL_ALPHA:
            if (supportsSSE2())
            {
                loadAlphaDataSSE2(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            }
            else
            {
                loadAlphaData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            }
            break;
          case GL_LUMINANCE:
            loadLuminanceData(width, height, inputPitch, input, locked.Pitch, locked.pBits, getD3DFormat() == D3DFMT_L8);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaData(width, height, inputPitch, input, locked.Pitch, locked.pBits, getD3DFormat() == D3DFMT_A8L8);
            break;
          case GL_RGB:
            loadRGBUByteData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_RGBA:
            if (supportsSSE2())
            {
                loadRGBAUByteDataSSE2(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            }
            else
            {
                loadRGBAUByteData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            }
            break;
          case GL_BGRA_EXT:
            loadBGRAData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_5_6_5:
        switch (mFormat)
        {
          case GL_RGB:
            loadRGB565Data(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_4_4_4_4:
        switch (mFormat)
        {
          case GL_RGBA:
            loadRGBA4444Data(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_5_5_5_1:
        switch (mFormat)
        {
          case GL_RGBA:
            loadRGBA5551Data(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT:
        switch (mFormat)
        {
          // float textures are converted to RGBA, not BGRA, as they're stored that way in D3D
          case GL_ALPHA:
            loadAlphaFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_LUMINANCE:
            loadLuminanceFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_RGB:
            loadRGBFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_RGBA:
            loadRGBAFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_HALF_FLOAT_OES:
        switch (mFormat)
        {
          // float textures are converted to RGBA, not BGRA, as they're stored that way in D3D
          case GL_ALPHA:
            loadAlphaHalfFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_LUMINANCE:
            loadLuminanceHalfFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaHalfFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_RGB:
            loadRGBHalfFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          case GL_RGBA:
            loadRGBAHalfFloatData(width, height, inputPitch, input, locked.Pitch, locked.pBits);
            break;
          default: UNREACHABLE();
        }
        break;
      default: UNREACHABLE();
    }

    unlock();
}

void Image::loadAlphaData(GLsizei width, GLsizei height,
                          int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;
    
    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = 0;
            dest[4 * x + 1] = 0;
            dest[4 * x + 2] = 0;
            dest[4 * x + 3] = source[x];
        }
    }
}

void Image::loadAlphaDataSSE2(GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned int *dest = NULL;
    __m128i zeroWide = _mm_setzero_si128();

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + y * outputPitch);

        int x;
        // Make output writes aligned
        for (x = 0; ((reinterpret_cast<intptr_t>(&dest[x]) & 0xF) != 0 && x < width); x++)
        {
            dest[x] = static_cast<unsigned int>(source[x]) << 24;
        }

        for (; x + 7 < width; x += 8)
        {
            __m128i sourceData = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&source[x]));
            // Interleave each byte to 16bit, make the lower byte to zero
            sourceData = _mm_unpacklo_epi8(zeroWide, sourceData);
            // Interleave each 16bit to 32bit, make the lower 16bit to zero
            __m128i lo = _mm_unpacklo_epi16(zeroWide, sourceData);
            __m128i hi = _mm_unpackhi_epi16(zeroWide, sourceData);

            _mm_store_si128(reinterpret_cast<__m128i*>(&dest[x]), lo);
            _mm_store_si128(reinterpret_cast<__m128i*>(&dest[x + 4]), hi);
        }

        // Handle the remainder
        for (; x < width; x++)
        {
            dest[x] = static_cast<unsigned int>(source[x]) << 24;
        }
    }
}

void Image::loadAlphaFloatData(GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = 0;
            dest[4 * x + 1] = 0;
            dest[4 * x + 2] = 0;
            dest[4 * x + 3] = source[x];
        }
    }
}

void Image::loadAlphaHalfFloatData(GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = 0;
            dest[4 * x + 1] = 0;
            dest[4 * x + 2] = 0;
            dest[4 * x + 3] = source[x];
        }
    }
}

void Image::loadLuminanceData(GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + y * outputPitch;

        if (!native)   // BGRA8 destination format
        {
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[x];
                dest[4 * x + 1] = source[x];
                dest[4 * x + 2] = source[x];
                dest[4 * x + 3] = 0xFF;
            }
        }
        else   // L8 destination format
        {
            memcpy(dest, source, width);
        }
    }
}

void Image::loadLuminanceFloatData(GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x];
            dest[4 * x + 1] = source[x];
            dest[4 * x + 2] = source[x];
            dest[4 * x + 3] = 1.0f;
        }
    }
}

void Image::loadLuminanceHalfFloatData(GLsizei width, GLsizei height,
                                       int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x];
            dest[4 * x + 1] = source[x];
            dest[4 * x + 2] = source[x];
            dest[4 * x + 3] = 0x3C00; // SEEEEEMMMMMMMMMM, S = 0, E = 15, M = 0: 16bit flpt representation of 1
        }
    }
}

void Image::loadLuminanceAlphaData(GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        
        if (!native)   // BGRA8 destination format
        {
            for (int x = 0; x < width; x++)
            {
                dest[4 * x + 0] = source[2*x+0];
                dest[4 * x + 1] = source[2*x+0];
                dest[4 * x + 2] = source[2*x+0];
                dest[4 * x + 3] = source[2*x+1];
            }
        }
        else
        {
            memcpy(dest, source, width * 2);
        }
    }
}

void Image::loadLuminanceAlphaFloatData(GLsizei width, GLsizei height,
                                        int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[2*x+0];
            dest[4 * x + 1] = source[2*x+0];
            dest[4 * x + 2] = source[2*x+0];
            dest[4 * x + 3] = source[2*x+1];
        }
    }
}

void Image::loadLuminanceAlphaHalfFloatData(GLsizei width, GLsizei height,
                                            int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[2*x+0];
            dest[4 * x + 1] = source[2*x+0];
            dest[4 * x + 2] = source[2*x+0];
            dest[4 * x + 3] = source[2*x+1];
        }
    }
}

void Image::loadRGBUByteData(GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 3 + 2];
            dest[4 * x + 1] = source[x * 3 + 1];
            dest[4 * x + 2] = source[x * 3 + 0];
            dest[4 * x + 3] = 0xFF;
        }
    }
}

void Image::loadRGB565Data(GLsizei width, GLsizei height,
                           int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        for (int x = 0; x < width; x++)
        {
            unsigned short rgba = source[x];
            dest[4 * x + 0] = ((rgba & 0x001F) << 3) | ((rgba & 0x001F) >> 2);
            dest[4 * x + 1] = ((rgba & 0x07E0) >> 3) | ((rgba & 0x07E0) >> 9);
            dest[4 * x + 2] = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
            dest[4 * x + 3] = 0xFF;
        }
    }
}

void Image::loadRGBFloatData(GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 3 + 0];
            dest[4 * x + 1] = source[x * 3 + 1];
            dest[4 * x + 2] = source[x * 3 + 2];
            dest[4 * x + 3] = 1.0f;
        }
    }
}

void Image::loadRGBHalfFloatData(GLsizei width, GLsizei height,
                                 int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + y * outputPitch);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 3 + 0];
            dest[4 * x + 1] = source[x * 3 + 1];
            dest[4 * x + 2] = source[x * 3 + 2];
            dest[4 * x + 3] = 0x3C00; // SEEEEEMMMMMMMMMM, S = 0, E = 15, M = 0: 16bit flpt representation of 1
        }
    }
}

void Image::loadRGBAUByteDataSSE2(GLsizei width, GLsizei height,
                                  int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;
    __m128i brMask = _mm_set1_epi32(0x00ff00ff);

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + y * outputPitch);
        int x = 0;

        // Make output writes aligned
        for (x = 0; ((reinterpret_cast<intptr_t>(&dest[x]) & 15) != 0) && x < width; x++)
        {
            unsigned int rgba = source[x];
            dest[x] = (_rotl(rgba, 16) & 0x00ff00ff) | (rgba & 0xff00ff00);
        }

        for (; x + 3 < width; x += 4)
        {
            __m128i sourceData = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&source[x]));
            // Mask out g and a, which don't change
            __m128i gaComponents = _mm_andnot_si128(brMask, sourceData);
            // Mask out b and r
            __m128i brComponents = _mm_and_si128(sourceData, brMask);
            // Swap b and r
            __m128i brSwapped = _mm_shufflehi_epi16(_mm_shufflelo_epi16(brComponents, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1));
            __m128i result = _mm_or_si128(gaComponents, brSwapped);
            _mm_store_si128(reinterpret_cast<__m128i*>(&dest[x]), result);
        }

        // Perform leftover writes
        for (; x < width; x++)
        {
            unsigned int rgba = source[x];
            dest[x] = (_rotl(rgba, 16) & 0x00ff00ff) | (rgba & 0xff00ff00);
        }
    }
}

void Image::loadRGBAUByteData(GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;
    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + y * outputPitch);

        for (int x = 0; x < width; x++)
        {
            unsigned int rgba = source[x];
            dest[x] = (_rotl(rgba, 16) & 0x00ff00ff) | (rgba & 0xff00ff00);
        }
    }
}

void Image::loadRGBA4444Data(GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        for (int x = 0; x < width; x++)
        {
            unsigned short rgba = source[x];
            dest[4 * x + 0] = ((rgba & 0x00F0) << 0) | ((rgba & 0x00F0) >> 4);
            dest[4 * x + 1] = ((rgba & 0x0F00) >> 4) | ((rgba & 0x0F00) >> 8);
            dest[4 * x + 2] = ((rgba & 0xF000) >> 8) | ((rgba & 0xF000) >> 12);
            dest[4 * x + 3] = ((rgba & 0x000F) << 4) | ((rgba & 0x000F) >> 0);
        }
    }
}

void Image::loadRGBA5551Data(GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        for (int x = 0; x < width; x++)
        {
            unsigned short rgba = source[x];
            dest[4 * x + 0] = ((rgba & 0x003E) << 2) | ((rgba & 0x003E) >> 3);
            dest[4 * x + 1] = ((rgba & 0x07C0) >> 3) | ((rgba & 0x07C0) >> 8);
            dest[4 * x + 2] = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
            dest[4 * x + 3] = (rgba & 0x0001) ? 0xFF : 0;
        }
    }
}

void Image::loadRGBAFloatData(GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + y * outputPitch);
        memcpy(dest, source, width * 16);
    }
}

void Image::loadRGBAHalfFloatData(GLsizei width, GLsizei height,
                                  int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        memcpy(dest, source, width * 8);
    }
}

void Image::loadBGRAData(GLsizei width, GLsizei height,
                         int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + y * outputPitch;
        memcpy(dest, source, width*4);
    }
}

void Image::loadCompressedData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               const void *input) {
    ASSERT(xoffset % 4 == 0);
    ASSERT(yoffset % 4 == 0);

    RECT lockRect = {
        xoffset, yoffset,
        xoffset + width, yoffset + height
    };

    D3DLOCKED_RECT locked;
    HRESULT result = lock(&locked, &lockRect);
    if (FAILED(result))
    {
        return;
    }

    GLsizei inputSize = ComputeCompressedSize(width, height, mFormat);
    GLsizei inputPitch = ComputeCompressedPitch(width, mFormat);
    int rows = inputSize / inputPitch;
    for (int i = 0; i < rows; ++i)
    {
        memcpy((void*)((BYTE*)locked.pBits + i * locked.Pitch), (void*)((BYTE*)input + i * inputPitch), inputPitch);
    }

    unlock();
}

// This implements glCopyTex[Sub]Image2D for non-renderable internal texture formats and incomplete textures
void Image::copy(GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, IDirect3DSurface9 *renderTarget)
{
    IDirect3DDevice9 *device = getDevice();
    IDirect3DSurface9 *renderTargetData = NULL;
    D3DSURFACE_DESC description;
    renderTarget->GetDesc(&description);
    
    HRESULT result = device->CreateOffscreenPlainSurface(description.Width, description.Height, description.Format, D3DPOOL_SYSTEMMEM, &renderTargetData, NULL);

    if (FAILED(result))
    {
        ERR("Could not create matching destination surface.");
        return error(GL_OUT_OF_MEMORY);
    }

    result = device->GetRenderTargetData(renderTarget, renderTargetData);

    if (FAILED(result))
    {
        ERR("GetRenderTargetData unexpectedly failed.");
        renderTargetData->Release();
        return error(GL_OUT_OF_MEMORY);
    }

    RECT sourceRect = {x, y, x + width, y + height};
    RECT destRect = {xoffset, yoffset, xoffset + width, yoffset + height};

    if (isRenderableFormat())
    {
        result = D3DXLoadSurfaceFromSurface(getSurface(), NULL, &destRect, renderTargetData, NULL, &sourceRect, D3DX_FILTER_BOX, 0);
        
        if (FAILED(result))
        {
            ERR("Copying surfaces unexpectedly failed.");
            renderTargetData->Release();
            return error(GL_OUT_OF_MEMORY);
        }
    }
    else
    {
        D3DLOCKED_RECT sourceLock = {0};
        result = renderTargetData->LockRect(&sourceLock, &sourceRect, 0);

        if (FAILED(result))
        {
            ERR("Failed to lock the source surface (rectangle might be invalid).");
            renderTargetData->Release();
            return error(GL_OUT_OF_MEMORY);
        }

        D3DLOCKED_RECT destLock = {0};
        result = lock(&destLock, &destRect);
        
        if (FAILED(result))
        {
            ERR("Failed to lock the destination surface (rectangle might be invalid).");
            renderTargetData->UnlockRect();
            renderTargetData->Release();
            return error(GL_OUT_OF_MEMORY);
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
    }

    renderTargetData->Release();

    mDirty = true;
}

TextureStorage::TextureStorage(DWORD usage)
    : mD3DUsage(usage),
      mD3DPool(getDisplay()->getTexturePool(usage)),
      mTextureSerial(issueTextureSerial())
{
}

TextureStorage::~TextureStorage()
{
}

bool TextureStorage::isRenderTarget() const
{
    return (mD3DUsage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) != 0;
}

bool TextureStorage::isManaged() const
{
    return (mD3DPool == D3DPOOL_MANAGED);
}

D3DPOOL TextureStorage::getPool() const
{
    return mD3DPool;
}

DWORD TextureStorage::getUsage() const
{
    return mD3DUsage;
}

unsigned int TextureStorage::getTextureSerial() const
{
    return mTextureSerial;
}

unsigned int TextureStorage::issueTextureSerial()
{
    return mCurrentTextureSerial++;
}

Texture::Texture(GLuint id) : RefCountObject(id)
{
    mMinFilter = GL_NEAREST_MIPMAP_LINEAR;
    mMagFilter = GL_LINEAR;
    mWrapS = GL_REPEAT;
    mWrapT = GL_REPEAT;
    mDirtyParameters = true;
    mUsage = GL_NONE;
    
    mDirtyImages = true;

    mImmutable = false;
}

Texture::~Texture()
{
}

// Returns true on successful filter state update (valid enum parameter)
bool Texture::setMinFilter(GLenum filter)
{
    switch (filter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        {
            if (mMinFilter != filter)
            {
                mMinFilter = filter;
                mDirtyParameters = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful filter state update (valid enum parameter)
bool Texture::setMagFilter(GLenum filter)
{
    switch (filter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        {
            if (mMagFilter != filter)
            {
                mMagFilter = filter;
                mDirtyParameters = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful wrap state update (valid enum parameter)
bool Texture::setWrapS(GLenum wrap)
{
    switch (wrap)
    {
      case GL_REPEAT:
      case GL_CLAMP_TO_EDGE:
      case GL_MIRRORED_REPEAT:
        {
            if (mWrapS != wrap)
            {
                mWrapS = wrap;
                mDirtyParameters = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful wrap state update (valid enum parameter)
bool Texture::setWrapT(GLenum wrap)
{
    switch (wrap)
    {
      case GL_REPEAT:
      case GL_CLAMP_TO_EDGE:
      case GL_MIRRORED_REPEAT:
        {
            if (mWrapT != wrap)
            {
                mWrapT = wrap;
                mDirtyParameters = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful usage state update (valid enum parameter)
bool Texture::setUsage(GLenum usage)
{
    switch (usage)
    {
      case GL_NONE:
      case GL_FRAMEBUFFER_ATTACHMENT_ANGLE:
        mUsage = usage;
        return true;
      default:
        return false;
    }
}

GLenum Texture::getMinFilter() const
{
    return mMinFilter;
}

GLenum Texture::getMagFilter() const
{
    return mMagFilter;
}

GLenum Texture::getWrapS() const
{
    return mWrapS;
}

GLenum Texture::getWrapT() const
{
    return mWrapT;
}

GLenum Texture::getUsage() const
{
    return mUsage;
}

void Texture::setImage(GLint unpackAlignment, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        image->loadData(0, 0, image->getWidth(), image->getHeight(), image->getType(), unpackAlignment, pixels);
        mDirtyImages = true;
    }
}

void Texture::setCompressedImage(GLsizei imageSize, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        image->loadCompressedData(0, 0, image->getWidth(), image->getHeight(), pixels);
        mDirtyImages = true;
    }
}

bool Texture::subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        image->loadData(xoffset, yoffset, width, height, type, unpackAlignment, pixels);
        mDirtyImages = true;
    }

    return true;
}

bool Texture::subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        image->loadCompressedData(xoffset, yoffset, width, height, pixels);
        mDirtyImages = true;
    }

    return true;
}

IDirect3DBaseTexture9 *Texture::getTexture()
{
    if (!isSamplerComplete())
    {
        return NULL;
    }

    // ensure the underlying texture is created
    if (getStorage(false) == NULL)
    {
        return NULL;
    }

    updateTexture();

    return getBaseTexture();
}

bool Texture::hasDirtyParameters() const
{
    return mDirtyParameters;
}

bool Texture::hasDirtyImages() const
{
    return mDirtyImages;
}

void Texture::resetDirty()
{
    mDirtyParameters = false;
    mDirtyImages = false;
}

unsigned int Texture::getTextureSerial()
{
    TextureStorage *texture = getStorage(false);
    return texture ? texture->getTextureSerial() : 0;
}

unsigned int Texture::getRenderTargetSerial(GLenum target)
{
    TextureStorage *texture = getStorage(true);
    return texture ? texture->getRenderTargetSerial(target) : 0;
}

bool Texture::isImmutable() const
{
    return mImmutable;
}

GLint Texture::creationLevels(GLsizei width, GLsizei height) const
{
    if ((isPow2(width) && isPow2(height)) || getContext()->supportsNonPower2Texture())
    {
        return 0;   // Maximum number of levels
    }
    else
    {
        // OpenGL ES 2.0 without GL_OES_texture_npot does not permit NPOT mipmaps.
        return 1;
    }
}

GLint Texture::creationLevels(GLsizei size) const
{
    return creationLevels(size, size);
}

int Texture::levelCount() const
{
    return getBaseTexture() ? getBaseTexture()->GetLevelCount() : 0;
}

Blit *Texture::getBlitter()
{
    Context *context = getContext();
    return context->getBlitter();
}

bool Texture::copyToRenderTarget(IDirect3DSurface9 *dest, IDirect3DSurface9 *source, bool fromManaged)
{
    if (source && dest)
    {
        HRESULT result;

        if (fromManaged)
        {
            result = D3DXLoadSurfaceFromSurface(dest, NULL, NULL, source, NULL, NULL, D3DX_FILTER_BOX, 0);
        }
        else
        {
            egl::Display *display = getDisplay();
            IDirect3DDevice9 *device = display->getDevice();

            display->endScene();
            result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);
        }

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return false;
        }
    }

    return true;
}

TextureStorage2D::TextureStorage2D(IDirect3DTexture9 *surfaceTexture) : TextureStorage(D3DUSAGE_RENDERTARGET), mRenderTargetSerial(RenderbufferStorage::issueSerial())
{
    mTexture = surfaceTexture;
}

TextureStorage2D::TextureStorage2D(int levels, D3DFORMAT format, DWORD usage, int width, int height)
    : TextureStorage(usage), mRenderTargetSerial(RenderbufferStorage::issueSerial())
{
    mTexture = NULL;
    // if the width or height is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (width > 0 && height > 0)
    {
        IDirect3DDevice9 *device = getDevice();
        HRESULT result = device->CreateTexture(width, height, levels, getUsage(), format, getPool(), &mTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            error(GL_OUT_OF_MEMORY);
        }
    }
}

TextureStorage2D::~TextureStorage2D()
{
    if (mTexture)
    {
        mTexture->Release();
    }
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *TextureStorage2D::getSurfaceLevel(int level)
{
    IDirect3DSurface9 *surface = NULL;

    if (mTexture)
    {
        HRESULT result = mTexture->GetSurfaceLevel(level, &surface);
        ASSERT(SUCCEEDED(result));
    }

    return surface;
}

IDirect3DBaseTexture9 *TextureStorage2D::getBaseTexture() const
{
    return mTexture;
}

unsigned int TextureStorage2D::getRenderTargetSerial(GLenum target) const
{
    return mRenderTargetSerial;
}

Texture2D::Texture2D(GLuint id) : Texture(id)
{
    mTexStorage = NULL;
    mSurface = NULL;
    mColorbufferProxy = NULL;
    mProxyRefs = 0;
}

Texture2D::~Texture2D()
{
    mColorbufferProxy = NULL;

    delete mTexStorage;
    mTexStorage = NULL;
    
    if (mSurface)
    {
        mSurface->setBoundTexture(NULL);
        mSurface = NULL;
    }
}

// We need to maintain a count of references to renderbuffers acting as 
// proxies for this texture, so that we do not attempt to use a pointer 
// to a renderbuffer proxy which has been deleted.
void Texture2D::addProxyRef(const Renderbuffer *proxy)
{
    mProxyRefs++;
}

void Texture2D::releaseProxy(const Renderbuffer *proxy)
{
    if (mProxyRefs > 0)
        mProxyRefs--;

    if (mProxyRefs == 0)
        mColorbufferProxy = NULL;
}

GLenum Texture2D::getTarget() const
{
    return GL_TEXTURE_2D;
}

GLsizei Texture2D::getWidth(GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level].getWidth();
    else
        return 0;
}

GLsizei Texture2D::getHeight(GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level].getHeight();
    else
        return 0;
}

GLenum Texture2D::getInternalFormat(GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level].getFormat();
    else
        return GL_NONE;
}

D3DFORMAT Texture2D::getD3DFormat(GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level].getD3DFormat();
    else
        return D3DFMT_UNKNOWN;
}

void Texture2D::redefineImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type)
{
    releaseTexImage();

    bool redefined = mImageArray[level].redefine(format, width, height, type, false);

    if (mTexStorage && redefined)
    {
        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            mImageArray[i].markDirty();
        }

        delete mTexStorage;
        mTexStorage = NULL;
        mDirtyImages = true;
    }
}

void Texture2D::setImage(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineImage(level, format, width, height, type);

    Texture::setImage(unpackAlignment, pixels, &mImageArray[level]);
}

void Texture2D::bindTexImage(egl::Surface *surface)
{
    releaseTexImage();

    GLenum format;

    switch(surface->getFormat())
    {
      case D3DFMT_A8R8G8B8:
        format = GL_RGBA;
        break;
      case D3DFMT_X8R8G8B8:
        format = GL_RGB;
        break;
      default:
        UNIMPLEMENTED();
        return;
    }

    mImageArray[0].redefine(format, surface->getWidth(), surface->getHeight(), GL_UNSIGNED_BYTE, true);

    delete mTexStorage;
    mTexStorage = new TextureStorage2D(surface->getOffscreenTexture());

    mDirtyImages = true;
    mSurface = surface;
    mSurface->setBoundTexture(this);
}

void Texture2D::releaseTexImage()
{
    if (mSurface)
    {
        mSurface->setBoundTexture(NULL);
        mSurface = NULL;

        if (mTexStorage)
        {
            delete mTexStorage;
            mTexStorage = NULL;
        }

        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            mImageArray[i].redefine(GL_RGBA, 0, 0, GL_UNSIGNED_BYTE, true);
        }
    }
}

void Texture2D::setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels)
{
    redefineImage(level, format, width, height, GL_UNSIGNED_BYTE);

    Texture::setCompressedImage(imageSize, pixels, &mImageArray[level]);
}

void Texture2D::commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(mImageArray[level].getSurface() != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = mTexStorage->getSurfaceLevel(level);

        if (destLevel)
        {
            Image *image = &mImageArray[level];
            image->updateSurface(destLevel, xoffset, yoffset, width, height);

            destLevel->Release();
            image->markClean();
        }
    }
}

void Texture2D::subImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    if (Texture::subImage(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, &mImageArray[level]))
    {
        commitRect(level, xoffset, yoffset, width, height);
    }
}

void Texture2D::subImageCompressed(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels)
{
    if (Texture::subImageCompressed(xoffset, yoffset, width, height, format, imageSize, pixels, &mImageArray[level]))
    {
        commitRect(level, xoffset, yoffset, width, height);
    }
}

void Texture2D::copyImage(GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source)
{
    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    redefineImage(level, format, width, height, GL_UNSIGNED_BYTE);
   
    if (!mImageArray[level].isRenderableFormat())
    {
        mImageArray[level].copy(0, 0, x, y, width, height, renderTarget);
        mDirtyImages = true;
    }
    else
    {
        if (!mTexStorage || !mTexStorage->isRenderTarget())
        {
            convertToRenderTarget();
        }
        
        mImageArray[level].markClean();

        if (width != 0 && height != 0 && level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;
            
            IDirect3DSurface9 *dest = mTexStorage->getSurfaceLevel(level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, format, 0, 0, dest);
                dest->Release();
            }
        }
    }

    renderTarget->Release();
}

void Texture2D::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source)
{
    if (xoffset + width > mImageArray[level].getWidth() || yoffset + height > mImageArray[level].getHeight())
    {
        return error(GL_INVALID_VALUE);
    }

    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    if (!mImageArray[level].isRenderableFormat() || (!mTexStorage && !isSamplerComplete()))
    {
        mImageArray[level].copy(xoffset, yoffset, x, y, width, height, renderTarget);
        mDirtyImages = true;
    }
    else
    {
        if (!mTexStorage || !mTexStorage->isRenderTarget())
        {
            convertToRenderTarget();
        }
        
        updateTexture();

        if (level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;


            IDirect3DSurface9 *dest = mTexStorage->getSurfaceLevel(level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, mImageArray[0].getFormat(), xoffset, yoffset, dest);
                dest->Release();
            }
        }
    }

    renderTarget->Release();
}

void Texture2D::storage(GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    GLenum format = gl::ExtractFormat(internalformat);
    GLenum type = gl::ExtractType(internalformat);
    D3DFORMAT d3dfmt = ConvertTextureFormatType(format, type);
    DWORD d3dusage = GetTextureUsage(d3dfmt, mUsage, false);

    delete mTexStorage;
    mTexStorage = new TextureStorage2D(levels, d3dfmt, d3dusage, width, height);
    mImmutable = true;

    for (int level = 0; level < levels; level++)
    {
        mImageArray[level].redefine(format, width, height, type, true);
        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);
    }

    for (int level = levels; level < IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        mImageArray[level].redefine(GL_NONE, 0, 0, GL_UNSIGNED_BYTE, true);
    }

    if (mTexStorage->isManaged())
    {
        int levels = levelCount();

        for (int level = 0; level < levels; level++)
        {
            IDirect3DSurface9 *surface = mTexStorage->getSurfaceLevel(level);
            mImageArray[level].setManagedSurface(surface);
        }
    }
}

// Tests for 2D texture sampling completeness. [OpenGL ES 2.0.24] section 3.8.2 page 85.
bool Texture2D::isSamplerComplete() const
{
    GLsizei width = mImageArray[0].getWidth();
    GLsizei height = mImageArray[0].getHeight();

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    bool mipmapping = false;

    switch (mMinFilter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        mipmapping = false;
        break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        mipmapping = true;
        break;
      default: UNREACHABLE();
    }

    if ((getInternalFormat(0) == GL_FLOAT && !getContext()->supportsFloat32LinearFilter()) ||
        (getInternalFormat(0) == GL_HALF_FLOAT_OES && !getContext()->supportsFloat16LinearFilter()))
    {
        if (mMagFilter != GL_NEAREST || (mMinFilter != GL_NEAREST && mMinFilter != GL_NEAREST_MIPMAP_NEAREST))
        {
            return false;
        }
    }

    bool npotSupport = getContext()->supportsNonPower2Texture();

    if (!npotSupport)
    {
        if ((getWrapS() != GL_CLAMP_TO_EDGE && !isPow2(width)) ||
            (getWrapT() != GL_CLAMP_TO_EDGE && !isPow2(height)))
        {
            return false;
        }
    }

    if (mipmapping)
    {
        if (!npotSupport)
        {
            if (!isPow2(width) || !isPow2(height))
            {
                return false;
            }
        }

        if (!isMipmapComplete())
        {
            return false;
        }
    }

    return true;
}

// Tests for 2D texture (mipmap) completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool Texture2D::isMipmapComplete() const
{
    if (isImmutable())
    {
        return true;
    }

    GLsizei width = mImageArray[0].getWidth();
    GLsizei height = mImageArray[0].getHeight();

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    int q = log2(std::max(width, height));

    for (int level = 1; level <= q; level++)
    {
        if (mImageArray[level].getFormat() != mImageArray[0].getFormat())
        {
            return false;
        }

        if (mImageArray[level].getType() != mImageArray[0].getType())
        {
            return false;
        }

        if (mImageArray[level].getWidth() != std::max(1, width >> level))
        {
            return false;
        }

        if (mImageArray[level].getHeight() != std::max(1, height >> level))
        {
            return false;
        }
    }

    return true;
}

bool Texture2D::isCompressed(GLint level) const
{
    return IsCompressed(getInternalFormat(level));
}

bool Texture2D::isDepth(GLint level) const
{
    return IsDepthTexture(getInternalFormat(level));
}

IDirect3DBaseTexture9 *Texture2D::getBaseTexture() const
{
    return mTexStorage ? mTexStorage->getBaseTexture() : NULL;
}

// Constructs a Direct3D 9 texture resource from the texture images
void Texture2D::createTexture()
{
    GLsizei width = mImageArray[0].getWidth();
    GLsizei height = mImageArray[0].getHeight();
    GLint levels = creationLevels(width, height);
    D3DFORMAT d3dfmt = mImageArray[0].getD3DFormat();
    DWORD d3dusage = GetTextureUsage(d3dfmt, mUsage, false);

    delete mTexStorage;
    mTexStorage = new TextureStorage2D(levels, d3dfmt, d3dusage, width, height);
    
    if (mTexStorage->isManaged())
    {
        int levels = levelCount();

        for (int level = 0; level < levels; level++)
        {
            IDirect3DSurface9 *surface = mTexStorage->getSurfaceLevel(level);
            mImageArray[level].setManagedSurface(surface);
        }
    }

    mDirtyImages = true;
}

void Texture2D::updateTexture()
{
    int levels = levelCount();

    for (int level = 0; level < levels; level++)
    {
        Image *image = &mImageArray[level];

        if (image->isDirty())
        {
            commitRect(level, 0, 0, mImageArray[level].getWidth(), mImageArray[level].getHeight());
        }
    }
}

void Texture2D::convertToRenderTarget()
{
    TextureStorage2D *newTexStorage = NULL;

    if (mImageArray[0].getWidth() != 0 && mImageArray[0].getHeight() != 0)
    {
        GLsizei width = mImageArray[0].getWidth();
        GLsizei height = mImageArray[0].getHeight();
        GLint levels = creationLevels(width, height);
        D3DFORMAT d3dfmt = mImageArray[0].getD3DFormat();
        DWORD d3dusage = GetTextureUsage(d3dfmt, GL_FRAMEBUFFER_ATTACHMENT_ANGLE, true);

        newTexStorage = new TextureStorage2D(levels, d3dfmt, d3dusage, width, height);

        if (mTexStorage != NULL)
        {
            int levels = levelCount();
            for (int i = 0; i < levels; i++)
            {
                IDirect3DSurface9 *source = mTexStorage->getSurfaceLevel(i);
                IDirect3DSurface9 *dest = newTexStorage->getSurfaceLevel(i);

                if (!copyToRenderTarget(dest, source, mTexStorage->isManaged()))
                {   
                   delete newTexStorage;
                   if (source) source->Release();
                   if (dest) dest->Release();
                   return error(GL_OUT_OF_MEMORY);
                }

                if (source) source->Release();
                if (dest) dest->Release();
            }
        }
    }

    delete mTexStorage;
    mTexStorage = newTexStorage;

    mDirtyImages = true;
}

void Texture2D::generateMipmaps()
{
    if (!getContext()->supportsNonPower2Texture())
    {
        if (!isPow2(mImageArray[0].getWidth()) || !isPow2(mImageArray[0].getHeight()))
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(std::max(mImageArray[0].getWidth(), mImageArray[0].getHeight()));
    for (unsigned int i = 1; i <= q; i++)
    {
        redefineImage(i, mImageArray[0].getFormat(), 
                         std::max(mImageArray[0].getWidth() >> i, 1),
                         std::max(mImageArray[0].getHeight() >> i, 1),
                         mImageArray[0].getType());
    }

    if (mTexStorage && mTexStorage->isRenderTarget())
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            IDirect3DSurface9 *upper = mTexStorage->getSurfaceLevel(i - 1);
            IDirect3DSurface9 *lower = mTexStorage->getSurfaceLevel(i);

            if (upper != NULL && lower != NULL)
            {
                getBlitter()->boxFilter(upper, lower);
            }

            if (upper != NULL) upper->Release();
            if (lower != NULL) lower->Release();

            mImageArray[i].markClean();
        }
    }
    else
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            if (mImageArray[i].getSurface() == NULL)
            {
                return error(GL_OUT_OF_MEMORY);
            }

            if (FAILED(D3DXLoadSurfaceFromSurface(mImageArray[i].getSurface(), NULL, NULL, mImageArray[i - 1].getSurface(), NULL, NULL, D3DX_FILTER_BOX, 0)))
            {
                ERR(" failed to load filter %d to %d.", i - 1, i);
            }

            mImageArray[i].markDirty();
        }
    }
}

Renderbuffer *Texture2D::getRenderbuffer(GLenum target)
{
    if (target != GL_TEXTURE_2D)
    {
        return error(GL_INVALID_OPERATION, (Renderbuffer *)NULL);
    }

    if (mColorbufferProxy == NULL)
    {
        mColorbufferProxy = new Renderbuffer(id(), new RenderbufferTexture2D(this, target));
    }

    return mColorbufferProxy;
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *Texture2D::getRenderTarget(GLenum target)
{
    ASSERT(target == GL_TEXTURE_2D);

    // ensure the underlying texture is created
    if (getStorage(true) == NULL)
    {
        return NULL;
    }

    updateTexture();
    
    // ensure this is NOT a depth texture
    if (isDepth(0))
    {
        return NULL;
    }
    return mTexStorage->getSurfaceLevel(0);
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *Texture2D::getDepthStencil(GLenum target)
{
    ASSERT(target == GL_TEXTURE_2D);

    // ensure the underlying texture is created
    if (getStorage(true) == NULL)
    {
        return NULL;
    }

    updateTexture();

    // ensure this is actually a depth texture
    if (!isDepth(0))
    {
        return NULL;
    }
    return mTexStorage->getSurfaceLevel(0);
}

TextureStorage *Texture2D::getStorage(bool renderTarget)
{
    if (!mTexStorage || (renderTarget && !mTexStorage->isRenderTarget()))
    {
        if (renderTarget)
        {
            convertToRenderTarget();
        }
        else
        {
            createTexture();
        }
    }

    return mTexStorage;
}

TextureStorageCubeMap::TextureStorageCubeMap(int levels, D3DFORMAT format, DWORD usage, int size)
    : TextureStorage(usage), mFirstRenderTargetSerial(RenderbufferStorage::issueCubeSerials())
{
    mTexture = NULL;
    // if the size is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (size > 0)
    {
        IDirect3DDevice9 *device = getDevice();
        HRESULT result = device->CreateCubeTexture(size, levels, getUsage(), format, getPool(), &mTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            error(GL_OUT_OF_MEMORY);
        }
    }
}

TextureStorageCubeMap::~TextureStorageCubeMap()
{
    if (mTexture)
    {
        mTexture->Release();
    }
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *TextureStorageCubeMap::getCubeMapSurface(GLenum faceTarget, int level)
{
    IDirect3DSurface9 *surface = NULL;

    if (mTexture)
    {
        HRESULT result = mTexture->GetCubeMapSurface(es2dx::ConvertCubeFace(faceTarget), level, &surface);
        ASSERT(SUCCEEDED(result));
    }

    return surface;
}

IDirect3DBaseTexture9 *TextureStorageCubeMap::getBaseTexture() const
{
    return mTexture;
}

unsigned int TextureStorageCubeMap::getRenderTargetSerial(GLenum target) const
{
    return mFirstRenderTargetSerial + TextureCubeMap::faceIndex(target);
}

TextureCubeMap::TextureCubeMap(GLuint id) : Texture(id)
{
    mTexStorage = NULL;
    for (int i = 0; i < 6; i++)
    {
        mFaceProxies[i] = NULL;
        mFaceProxyRefs[i] = 0;
    }
}

TextureCubeMap::~TextureCubeMap()
{
    for (int i = 0; i < 6; i++)
    {
        mFaceProxies[i] = NULL;
    }

    delete mTexStorage;
    mTexStorage = NULL;
}

// We need to maintain a count of references to renderbuffers acting as 
// proxies for this texture, so that the texture is not deleted while 
// proxy references still exist. If the reference count drops to zero,
// we set our proxy pointer NULL, so that a new attempt at referencing
// will cause recreation.
void TextureCubeMap::addProxyRef(const Renderbuffer *proxy)
{
    for (int i = 0; i < 6; i++)
    {
        if (mFaceProxies[i] == proxy)
            mFaceProxyRefs[i]++;
    }
}

void TextureCubeMap::releaseProxy(const Renderbuffer *proxy)
{
    for (int i = 0; i < 6; i++)
    {
        if (mFaceProxies[i] == proxy)
        {
            if (mFaceProxyRefs[i] > 0)
                mFaceProxyRefs[i]--;

            if (mFaceProxyRefs[i] == 0)
                mFaceProxies[i] = NULL;
        }
    }
}

GLenum TextureCubeMap::getTarget() const
{
    return GL_TEXTURE_CUBE_MAP;
}

GLsizei TextureCubeMap::getWidth(GLenum target, GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[faceIndex(target)][level].getWidth();
    else
        return 0;
}

GLsizei TextureCubeMap::getHeight(GLenum target, GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[faceIndex(target)][level].getHeight();
    else
        return 0;
}

GLenum TextureCubeMap::getInternalFormat(GLenum target, GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[faceIndex(target)][level].getFormat();
    else
        return GL_NONE;
}

D3DFORMAT TextureCubeMap::getD3DFormat(GLenum target, GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[faceIndex(target)][level].getD3DFormat();
    else
        return D3DFMT_UNKNOWN;
}

void TextureCubeMap::setImagePosX(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(0, level, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegX(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(1, level, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImagePosY(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(2, level, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegY(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(3, level, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImagePosZ(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(4, level, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegZ(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(5, level, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setCompressedImage(GLenum face, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels)
{
    redefineImage(faceIndex(face), level, format, width, height, GL_UNSIGNED_BYTE);

    Texture::setCompressedImage(imageSize, pixels, &mImageArray[faceIndex(face)][level]);
}

void TextureCubeMap::commitRect(int face, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(mImageArray[face][level].getSurface() != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = mTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level);
        ASSERT(destLevel != NULL);

        if (destLevel != NULL)
        {
            Image *image = &mImageArray[face][level];
            image->updateSurface(destLevel, xoffset, yoffset, width, height);

            destLevel->Release();
            image->markClean();
        }
    }
}

void TextureCubeMap::subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    if (Texture::subImage(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, &mImageArray[faceIndex(target)][level]))
    {
        commitRect(faceIndex(target), level, xoffset, yoffset, width, height);
    }
}

void TextureCubeMap::subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels)
{
    if (Texture::subImageCompressed(xoffset, yoffset, width, height, format, imageSize, pixels, &mImageArray[faceIndex(target)][level]))
    {
        commitRect(faceIndex(target), level, xoffset, yoffset, width, height);
    }
}

// Tests for cube map sampling completeness. [OpenGL ES 2.0.24] section 3.8.2 page 86.
bool TextureCubeMap::isSamplerComplete() const
{
    int size = mImageArray[0][0].getWidth();

    bool mipmapping;

    switch (mMinFilter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        mipmapping = false;
        break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        mipmapping = true;
        break;
      default:
        UNREACHABLE();
        return false;
    }

    if ((getInternalFormat(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0) == GL_FLOAT && !getContext()->supportsFloat32LinearFilter()) ||
        (getInternalFormat(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0) == GL_HALF_FLOAT_OES && !getContext()->supportsFloat16LinearFilter()))
    {
        if (mMagFilter != GL_NEAREST || (mMinFilter != GL_NEAREST && mMinFilter != GL_NEAREST_MIPMAP_NEAREST))
        {
            return false;
        }
    }

    if (!isPow2(size) && !getContext()->supportsNonPower2Texture())
    {
        if (getWrapS() != GL_CLAMP_TO_EDGE || getWrapT() != GL_CLAMP_TO_EDGE || mipmapping)
        {
            return false;
        }
    }

    if (!mipmapping)
    {
        if (!isCubeComplete())
        {
            return false;
        }
    }
    else
    {
        if (!isMipmapCubeComplete())   // Also tests for isCubeComplete()
        {
            return false;
        }
    }

    return true;
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool TextureCubeMap::isCubeComplete() const
{
    if (mImageArray[0][0].getWidth() <= 0 || mImageArray[0][0].getHeight() != mImageArray[0][0].getWidth())
    {
        return false;
    }

    for (unsigned int face = 1; face < 6; face++)
    {
        if (mImageArray[face][0].getWidth() != mImageArray[0][0].getWidth() ||
            mImageArray[face][0].getWidth() != mImageArray[0][0].getHeight() ||
            mImageArray[face][0].getFormat() != mImageArray[0][0].getFormat() ||
            mImageArray[face][0].getType() != mImageArray[0][0].getType())
        {
            return false;
        }
    }

    return true;
}

bool TextureCubeMap::isMipmapCubeComplete() const
{
    if (isImmutable())
    {
        return true;
    }

    if (!isCubeComplete())
    {
        return false;
    }

    GLsizei size = mImageArray[0][0].getWidth();

    int q = log2(size);

    for (int face = 0; face < 6; face++)
    {
        for (int level = 1; level <= q; level++)
        {
            if (mImageArray[face][level].getFormat() != mImageArray[0][0].getFormat())
            {
                return false;
            }

            if (mImageArray[face][level].getType() != mImageArray[0][0].getType())
            {
                return false;
            }

            if (mImageArray[face][level].getWidth() != std::max(1, size >> level))
            {
                return false;
            }
        }
    }

    return true;
}

bool TextureCubeMap::isCompressed(GLenum target, GLint level) const
{
    return IsCompressed(getInternalFormat(target, level));
}

IDirect3DBaseTexture9 *TextureCubeMap::getBaseTexture() const
{
    return mTexStorage ? mTexStorage->getBaseTexture() : NULL;
}

// Constructs a Direct3D 9 texture resource from the texture images, or returns an existing one
void TextureCubeMap::createTexture()
{
    GLsizei size = mImageArray[0][0].getWidth();
    GLint levels = creationLevels(size, 0);
    D3DFORMAT d3dfmt = mImageArray[0][0].getD3DFormat();
    DWORD d3dusage = GetTextureUsage(d3dfmt, mUsage, false);

    delete mTexStorage;
    mTexStorage = new TextureStorageCubeMap(levels, d3dfmt, d3dusage, size);

    if (mTexStorage->isManaged())
    {
        int levels = levelCount();

        for (int face = 0; face < 6; face++)
        {
            for (int level = 0; level < levels; level++)
            {
                IDirect3DSurface9 *surface = mTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level);
                mImageArray[face][level].setManagedSurface(surface);
            }
        }
    }

    mDirtyImages = true;
}

void TextureCubeMap::updateTexture()
{
    for (int face = 0; face < 6; face++)
    {
        int levels = levelCount();
        for (int level = 0; level < levels; level++)
        {
            Image *image = &mImageArray[face][level];

            if (image->isDirty())
            {
                commitRect(face, level, 0, 0, image->getWidth(), image->getHeight());
            }
        }
    }
}

void TextureCubeMap::convertToRenderTarget()
{
    TextureStorageCubeMap *newTexStorage = NULL;

    if (mImageArray[0][0].getWidth() != 0)
    {
        GLsizei size = mImageArray[0][0].getWidth();
        GLint levels = creationLevels(size, 0);
        D3DFORMAT d3dfmt = mImageArray[0][0].getD3DFormat();
        DWORD d3dusage = GetTextureUsage(d3dfmt, GL_FRAMEBUFFER_ATTACHMENT_ANGLE, true);

        newTexStorage = new TextureStorageCubeMap(levels, d3dfmt, d3dusage, size);

        if (mTexStorage != NULL)
        {
            int levels = levelCount();
            for (int f = 0; f < 6; f++)
            {
                for (int i = 0; i < levels; i++)
                {
                    IDirect3DSurface9 *source = mTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, i);
                    IDirect3DSurface9 *dest = newTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, i);

                    if (!copyToRenderTarget(dest, source, mTexStorage->isManaged()))
                    {
                       delete newTexStorage;
                       if (source) source->Release();
                       if (dest) dest->Release();
                       return error(GL_OUT_OF_MEMORY);
                    }

                    if (source) source->Release();
                    if (dest) dest->Release();
                }
            }
        }
    }

    delete mTexStorage;
    mTexStorage = newTexStorage;

    mDirtyImages = true;
}

void TextureCubeMap::setImage(int faceIndex, GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineImage(faceIndex, level, format, width, height, type);

    Texture::setImage(unpackAlignment, pixels, &mImageArray[faceIndex][level]);
}

unsigned int TextureCubeMap::faceIndex(GLenum face)
{
    META_ASSERT(GL_TEXTURE_CUBE_MAP_NEGATIVE_X - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 1);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_Y - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 2);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 3);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 4);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 5);

    return face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
}

void TextureCubeMap::redefineImage(int face, GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type)
{
    bool redefined = mImageArray[face][level].redefine(format, width, height, type, false);

    if (mTexStorage && redefined)
    {
        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            for (int f = 0; f < 6; f++)
            {
                mImageArray[f][i].markDirty();
            }
        }

        delete mTexStorage;
        mTexStorage = NULL;

        mDirtyImages = true;
    }
}

void TextureCubeMap::copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source)
{
    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    unsigned int faceindex = faceIndex(target);
    redefineImage(faceindex, level, format, width, height, GL_UNSIGNED_BYTE);

    if (!mImageArray[faceindex][level].isRenderableFormat())
    {
        mImageArray[faceindex][level].copy(0, 0, x, y, width, height, renderTarget);
        mDirtyImages = true;
    }
    else
    {
        if (!mTexStorage || !mTexStorage->isRenderTarget())
        {
            convertToRenderTarget();
        }
        
        mImageArray[faceindex][level].markClean();

        ASSERT(width == height);

        if (width > 0 && level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;

            IDirect3DSurface9 *dest = mTexStorage->getCubeMapSurface(target, level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, format, 0, 0, dest);
                dest->Release();
            }
        }
    }

    renderTarget->Release();
}

void TextureCubeMap::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source)
{
    GLsizei size = mImageArray[faceIndex(target)][level].getWidth();

    if (xoffset + width > size || yoffset + height > size)
    {
        return error(GL_INVALID_VALUE);
    }

    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    unsigned int faceindex = faceIndex(target);

    if (!mImageArray[faceindex][level].isRenderableFormat() || (!mTexStorage && !isSamplerComplete()))
    {
        mImageArray[faceindex][level].copy(0, 0, x, y, width, height, renderTarget);
        mDirtyImages = true;
    }
    else
    {
        if (!mTexStorage || !mTexStorage->isRenderTarget())
        {
            convertToRenderTarget();
        }
        
        updateTexture();

        if (level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;

            IDirect3DSurface9 *dest = mTexStorage->getCubeMapSurface(target, level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, mImageArray[0][0].getFormat(), xoffset, yoffset, dest);
                dest->Release();
            }
        }
    }

    renderTarget->Release();
}

void TextureCubeMap::storage(GLsizei levels, GLenum internalformat, GLsizei size)
{
    GLenum format = gl::ExtractFormat(internalformat);
    GLenum type = gl::ExtractType(internalformat);
    D3DFORMAT d3dfmt = ConvertTextureFormatType(format, type);
    DWORD d3dusage = GetTextureUsage(d3dfmt, mUsage, false);

    delete mTexStorage;
    mTexStorage = new TextureStorageCubeMap(levels, d3dfmt, d3dusage, size);
    mImmutable = true;

    for (int level = 0; level < levels; level++)
    {
        for (int face = 0; face < 6; face++)
        {
            mImageArray[face][level].redefine(format, size, size, type, true);
            size = std::max(1, size >> 1);
        }
    }

    for (int level = levels; level < IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        for (int face = 0; face < 6; face++)
        {
            mImageArray[face][level].redefine(GL_NONE, 0, 0, GL_UNSIGNED_BYTE, true);
        }
    }

    if (mTexStorage->isManaged())
    {
        int levels = levelCount();

        for (int face = 0; face < 6; face++)
        {
            for (int level = 0; level < levels; level++)
            {
                IDirect3DSurface9 *surface = mTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level);
                mImageArray[face][level].setManagedSurface(surface);
            }
        }
    }
}

void TextureCubeMap::generateMipmaps()
{
    if (!isCubeComplete())
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!getContext()->supportsNonPower2Texture())
    {
        if (!isPow2(mImageArray[0][0].getWidth()))
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(mImageArray[0][0].getWidth());
    for (unsigned int f = 0; f < 6; f++)
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            redefineImage(f, i, mImageArray[f][0].getFormat(),
                                std::max(mImageArray[f][0].getWidth() >> i, 1),
                                std::max(mImageArray[f][0].getWidth() >> i, 1),
                                mImageArray[f][0].getType());
        }
    }

    if (mTexStorage && mTexStorage->isRenderTarget())
    {
        for (unsigned int f = 0; f < 6; f++)
        {
            for (unsigned int i = 1; i <= q; i++)
            {
                IDirect3DSurface9 *upper = mTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, i-1);
                IDirect3DSurface9 *lower = mTexStorage->getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, i);

                if (upper != NULL && lower != NULL)
                {
                    getBlitter()->boxFilter(upper, lower);
                }

                if (upper != NULL) upper->Release();
                if (lower != NULL) lower->Release();

                mImageArray[f][i].markClean();
            }
        }
    }
    else
    {
        for (unsigned int f = 0; f < 6; f++)
        {
            for (unsigned int i = 1; i <= q; i++)
            {
                if (mImageArray[f][i].getSurface() == NULL)
                {
                    return error(GL_OUT_OF_MEMORY);
                }

                if (FAILED(D3DXLoadSurfaceFromSurface(mImageArray[f][i].getSurface(), NULL, NULL, mImageArray[f][i - 1].getSurface(), NULL, NULL, D3DX_FILTER_BOX, 0)))
                {
                    ERR(" failed to load filter %d to %d.", i - 1, i);
                }

                mImageArray[f][i].markDirty();
            }
        }
    }
}

Renderbuffer *TextureCubeMap::getRenderbuffer(GLenum target)
{
    if (!IsCubemapTextureTarget(target))
    {
        return error(GL_INVALID_OPERATION, (Renderbuffer *)NULL);
    }

    unsigned int face = faceIndex(target);

    if (mFaceProxies[face] == NULL)
    {
        mFaceProxies[face] = new Renderbuffer(id(), new RenderbufferTextureCubeMap(this, target));
    }

    return mFaceProxies[face];
}

// Increments refcount on surface.
// caller must Release() the returned surface
IDirect3DSurface9 *TextureCubeMap::getRenderTarget(GLenum target)
{
    ASSERT(IsCubemapTextureTarget(target));

    // ensure the underlying texture is created
    if (getStorage(true) == NULL)
    {
        return NULL;
    }

    updateTexture();
    
    return mTexStorage->getCubeMapSurface(target, 0);
}

TextureStorage *TextureCubeMap::getStorage(bool renderTarget)
{
    if (!mTexStorage || (renderTarget && !mTexStorage->isRenderTarget()))
    {
        if (renderTarget)
        {
            convertToRenderTarget();
        }
        else
        {
            createTexture();
        }
    }

    return mTexStorage;
}

}
