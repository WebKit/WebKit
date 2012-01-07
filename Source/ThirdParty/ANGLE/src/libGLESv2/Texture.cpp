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
    if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
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

        HRESULT result = getDevice()->CreateTexture(requestWidth, requestHeight, levelToFetch + 1, NULL, getD3DFormat(),
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

    if (sourceSurface != destSurface)
    {
        RECT rect = transformPixelRect(xoffset, yoffset, width, height, mHeight);

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
// into the target pixel rectangle at output with outputPitch bytes in between each line.
void Image::loadData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum type,
                     GLint unpackAlignment, const void *input, size_t outputPitch, void *output) const
{
    GLsizei inputPitch = -ComputePitch(width, mFormat, type, unpackAlignment);
    input = ((char*)input) - inputPitch * (height - 1);

    switch (type)
    {
      case GL_UNSIGNED_BYTE:
        switch (mFormat)
        {
          case GL_ALPHA:
            loadAlphaData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE:
            loadLuminanceData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output, getD3DFormat() == D3DFMT_L8);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output, getD3DFormat() == D3DFMT_A8L8);
            break;
          case GL_RGB:
            loadRGBUByteData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGBA:
            if (supportsSSE2())
            {
                loadRGBAUByteDataSSE2(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            }
            else
            {
                loadRGBAUByteData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            }
            break;
          case GL_BGRA_EXT:
            loadBGRAData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_5_6_5:
        switch (mFormat)
        {
          case GL_RGB:
            loadRGB565Data(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_4_4_4_4:
        switch (mFormat)
        {
          case GL_RGBA:
            loadRGBA4444Data(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_5_5_5_1:
        switch (mFormat)
        {
          case GL_RGBA:
            loadRGBA5551Data(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT:
        switch (mFormat)
        {
          // float textures are converted to RGBA, not BGRA, as they're stored that way in D3D
          case GL_ALPHA:
            loadAlphaFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE:
            loadLuminanceFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGB:
            loadRGBFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGBA:
            loadRGBAFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_HALF_FLOAT_OES:
        switch (mFormat)
        {
          // float textures are converted to RGBA, not BGRA, as they're stored that way in D3D
          case GL_ALPHA:
            loadAlphaHalfFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE:
            loadLuminanceHalfFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaHalfFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGB:
            loadRGBHalfFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGBA:
            loadRGBAHalfFloatData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      default: UNREACHABLE();
    }
}

void Image::loadAlphaData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                          int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;
    
    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = 0;
            dest[4 * x + 1] = 0;
            dest[4 * x + 2] = 0;
            dest[4 * x + 3] = source[x];
        }
    }
}

void Image::loadAlphaFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 16);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = 0;
            dest[4 * x + 1] = 0;
            dest[4 * x + 2] = 0;
            dest[4 * x + 3] = source[x];
        }
    }
}

void Image::loadAlphaHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 8);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = 0;
            dest[4 * x + 1] = 0;
            dest[4 * x + 2] = 0;
            dest[4 * x + 3] = source[x];
        }
    }
}

void Image::loadLuminanceData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const
{
    const int destBytesPerPixel = native? 1: 4;
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * destBytesPerPixel;

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

void Image::loadLuminanceFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 16);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x];
            dest[4 * x + 1] = source[x];
            dest[4 * x + 2] = source[x];
            dest[4 * x + 3] = 1.0f;
        }
    }
}

void Image::loadLuminanceHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                       int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 8);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x];
            dest[4 * x + 1] = source[x];
            dest[4 * x + 2] = source[x];
            dest[4 * x + 3] = 0x3C00; // SEEEEEMMMMMMMMMM, S = 0, E = 15, M = 0: 16bit flpt representation of 1
        }
    }
}

void Image::loadLuminanceAlphaData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                   int inputPitch, const void *input, size_t outputPitch, void *output, bool native) const
{
    const int destBytesPerPixel = native? 2: 4;
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * destBytesPerPixel;
        
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

void Image::loadLuminanceAlphaFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                        int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 16);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[2*x+0];
            dest[4 * x + 1] = source[2*x+0];
            dest[4 * x + 2] = source[2*x+0];
            dest[4 * x + 3] = source[2*x+1];
        }
    }
}

void Image::loadLuminanceAlphaHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                            int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 8);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[2*x+0];
            dest[4 * x + 1] = source[2*x+0];
            dest[4 * x + 2] = source[2*x+0];
            dest[4 * x + 3] = source[2*x+1];
        }
    }
}

void Image::loadRGBUByteData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 3 + 2];
            dest[4 * x + 1] = source[x * 3 + 1];
            dest[4 * x + 2] = source[x * 3 + 0];
            dest[4 * x + 3] = 0xFF;
        }
    }
}

void Image::loadRGB565Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                           int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
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

void Image::loadRGBFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 16);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 3 + 0];
            dest[4 * x + 1] = source[x * 3 + 1];
            dest[4 * x + 2] = source[x * 3 + 2];
            dest[4 * x + 3] = 1.0f;
        }
    }
}

void Image::loadRGBHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                 int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned short *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned short*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 8);
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 3 + 0];
            dest[4 * x + 1] = source[x * 3 + 1];
            dest[4 * x + 2] = source[x * 3 + 2];
            dest[4 * x + 3] = 0x3C00; // SEEEEEMMMMMMMMMM, S = 0, E = 15, M = 0: 16bit flpt representation of 1
        }
    }
}

void Image::loadRGBAUByteDataSSE2(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                  int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;
    __m128i brMask = _mm_set1_epi32(0x00ff00ff);

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4);
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

void Image::loadRGBAUByteData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned int *source = NULL;
    unsigned int *dest = NULL;
    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4);

        for (int x = 0; x < width; x++)
        {
            unsigned int rgba = source[x];
            dest[x] = (_rotl(rgba, 16) & 0x00ff00ff) | (rgba & 0xff00ff00);
        }
    }
}

void Image::loadRGBA4444Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
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

void Image::loadRGBA5551Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                             int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned short *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
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

void Image::loadRGBAFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                              int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const float *source = NULL;
    float *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = reinterpret_cast<const float*>(static_cast<const unsigned char*>(input) + y * inputPitch);
        dest = reinterpret_cast<float*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 16);
        memcpy(dest, source, width * 16);
    }
}

void Image::loadRGBAHalfFloatData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                  int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch  + xoffset * 8;
        memcpy(dest, source, width * 8);
    }
}

void Image::loadBGRAData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                         int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
        memcpy(dest, source, width*4);
    }
}

void Image::loadCompressedData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                               int inputPitch, const void *input, size_t outputPitch, void *output) const {
    switch (getD3DFormat())
    {
        case D3DFMT_DXT1:
          loadDXT1Data(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
          break;
        case D3DFMT_DXT3:
          loadDXT3Data(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
          break;
        case D3DFMT_DXT5:
          loadDXT5Data(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
          break;
    }
}

static void FlipCopyDXT1BlockFull(const unsigned int* source, unsigned int* dest) {
  // A DXT1 block layout is:
  // [0-1] color0.
  // [2-3] color1.
  // [4-7] color bitmap, 2 bits per pixel.
  // So each of the 4-7 bytes represents one line, flipping a block is just
  // flipping those bytes.

  // First 32-bits is two RGB565 colors shared by tile and does not need to be modified.
  dest[0] = source[0];

  // Second 32-bits contains 4 rows of 4 2-bit interpolants between the colors. All rows should be flipped.
  dest[1] = (source[1] >> 24) |
            ((source[1] << 8) & 0x00FF0000) |
            ((source[1] >> 8) & 0x0000FF00) |
            (source[1] << 24);
}

// Flips the first 2 lines of a DXT1 block in the y direction.
static void FlipCopyDXT1BlockHalf(const unsigned int* source, unsigned int* dest) {
  // See layout above.
  dest[0] = source[0];
  dest[1] = ((source[1] << 8) & 0x0000FF00) |
            ((source[1] >> 8) & 0x000000FF);
}

// Flips a full DXT3 block in the y direction.
static void FlipCopyDXT3BlockFull(const unsigned int* source, unsigned int* dest) {
  // A DXT3 block layout is:
  // [0-7]  alpha bitmap, 4 bits per pixel.
  // [8-15] a DXT1 block.

  // First and Second 32 bits are 4bit per pixel alpha and need to be flipped.
  dest[0] = (source[1] >> 16) | (source[1] << 16);
  dest[1] = (source[0] >> 16) | (source[0] << 16);

  // And flip the DXT1 block using the above function.
  FlipCopyDXT1BlockFull(source + 2, dest + 2);
}

// Flips the first 2 lines of a DXT3 block in the y direction.
static void FlipCopyDXT3BlockHalf(const unsigned int* source, unsigned int* dest) {
  // See layout above.
  dest[0] = (source[1] >> 16) | (source[1] << 16);
  FlipCopyDXT1BlockHalf(source + 2, dest + 2);
}

// Flips a full DXT5 block in the y direction.
static void FlipCopyDXT5BlockFull(const unsigned int* source, unsigned int* dest) {
  // A DXT5 block layout is:
  // [0]    alpha0.
  // [1]    alpha1.
  // [2-7]  alpha bitmap, 3 bits per pixel.
  // [8-15] a DXT1 block.

  // The alpha bitmap doesn't easily map lines to bytes, so we have to
  // interpret it correctly.  Extracted from
  // http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt :
  //
  //   The 6 "bits" bytes of the block are decoded into one 48-bit integer:
  //
  //     bits = bits_0 + 256 * (bits_1 + 256 * (bits_2 + 256 * (bits_3 +
  //                   256 * (bits_4 + 256 * bits_5))))
  //
  //   bits is a 48-bit unsigned integer, from which a three-bit control code
  //   is extracted for a texel at location (x,y) in the block using:
  //
  //       code(x,y) = bits[3*(4*y+x)+1..3*(4*y+x)+0]
  //
  //   where bit 47 is the most significant and bit 0 is the least
  //   significant bit.
  const unsigned char* sourceBytes = static_cast<const unsigned char*>(static_cast<const void*>(source));
  unsigned char* destBytes = static_cast<unsigned char*>(static_cast<void*>(dest));
  unsigned int line_0_1 = sourceBytes[2] + 256 * (sourceBytes[3] + 256 * sourceBytes[4]);
  unsigned int line_2_3 = sourceBytes[5] + 256 * (sourceBytes[6] + 256 * sourceBytes[7]);
  // swap lines 0 and 1 in line_0_1.
  unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
                          ((line_0_1 & 0xfff000) >> 12);
  // swap lines 2 and 3 in line_2_3.
  unsigned int line_3_2 = ((line_2_3 & 0x000fff) << 12) |
                          ((line_2_3 & 0xfff000) >> 12);
  destBytes[0] = sourceBytes[0];
  destBytes[1] = sourceBytes[1];
  destBytes[2] = line_3_2 & 0xff;
  destBytes[3] = (line_3_2 & 0xff00) >> 8;
  destBytes[4] = (line_3_2 & 0xff0000) >> 16;
  destBytes[5] = line_1_0 & 0xff;
  destBytes[6] = (line_1_0 & 0xff00) >> 8;
  destBytes[7] = (line_1_0 & 0xff0000) >> 16;

  // And flip the DXT1 block using the above function.
  FlipCopyDXT1BlockFull(source + 2, dest + 2);
}

// Flips the first 2 lines of a DXT5 block in the y direction.
static void FlipCopyDXT5BlockHalf(const unsigned int* source, unsigned int* dest) {
  // See layout above.
  const unsigned char* sourceBytes = static_cast<const unsigned char*>(static_cast<const void*>(source));
  unsigned char* destBytes = static_cast<unsigned char*>(static_cast<void*>(dest));
  unsigned int line_0_1 = sourceBytes[2] + 256 * (sourceBytes[3] + 256 * sourceBytes[4]);
  unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
                          ((line_0_1 & 0xfff000) >> 12);
  destBytes[0] = sourceBytes[0];
  destBytes[1] = sourceBytes[1];
  destBytes[2] = line_1_0 & 0xff;
  destBytes[3] = (line_1_0 & 0xff00) >> 8;
  destBytes[4] = (line_1_0 & 0xff0000) >> 16;
  FlipCopyDXT1BlockHalf(source + 2, dest + 2);
}

void Image::loadDXT1Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                         int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    ASSERT(xoffset % 4 == 0);
    ASSERT(yoffset % 4 == 0);
    ASSERT(width % 4 == 0 || width == 2 || width == 1);
    ASSERT(inputPitch % 8 == 0);
    ASSERT(outputPitch % 8 == 0);

    const unsigned int *source = reinterpret_cast<const unsigned int*>(input);
    unsigned int *dest = reinterpret_cast<unsigned int*>(output);

    // Round width up in case it is less than 4.
    int blocksAcross = (width + 3) / 4;
    int intsAcross = blocksAcross * 2;

    switch (height)
    {
        case 1:
            for (int x = 0; x < intsAcross; x += 2)
            {
                // just copy the block
                dest[x] = source[x];
                dest[x + 1] = source[x + 1];
            }
            break;
        case 2:
            for (int x = 0; x < intsAcross; x += 2)
            {
                FlipCopyDXT1BlockHalf(source + x, dest + x);
            }
            break;
        default:
            ASSERT(height % 4 == 0);
            for (int y = 0; y < height / 4; ++y)
            {
                const unsigned int *source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
                unsigned int *dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 8);

                for (int x = 0; x < intsAcross; x += 2)
                {
                    FlipCopyDXT1BlockFull(source + x, dest + x);
                }
            }
            break;
    }
}

void Image::loadDXT3Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                         int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    ASSERT(xoffset % 4 == 0);
    ASSERT(yoffset % 4 == 0);
    ASSERT(width % 4 == 0 || width == 2 || width == 1);
    ASSERT(inputPitch % 16 == 0);
    ASSERT(outputPitch % 16 == 0);

    const unsigned int *source = reinterpret_cast<const unsigned int*>(input);
    unsigned int *dest = reinterpret_cast<unsigned int*>(output);

    // Round width up in case it is less than 4.
    int blocksAcross = (width + 3) / 4;
    int intsAcross = blocksAcross * 4;

    switch (height)
    {
        case 1:
            for (int x = 0; x < intsAcross; x += 4)
            {
                // just copy the block
                dest[x] = source[x];
                dest[x + 1] = source[x + 1];
                dest[x + 2] = source[x + 2];
                dest[x + 3] = source[x + 3];
            }
            break;
        case 2:
            for (int x = 0; x < intsAcross; x += 4)
            {
                FlipCopyDXT3BlockHalf(source + x, dest + x);
            }
            break;
        default:
            ASSERT(height % 4 == 0);
            for (int y = 0; y < height / 4; ++y)
            {
                const unsigned int *source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
                unsigned int *dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 16);

                for (int x = 0; x < intsAcross; x += 4)
                {
                  FlipCopyDXT3BlockFull(source + x, dest + x);
                }
            }
            break;
    }
}

void Image::loadDXT5Data(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                         int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    ASSERT(xoffset % 4 == 0);
    ASSERT(yoffset % 4 == 0);
    ASSERT(width % 4 == 0 || width == 2 || width == 1);
    ASSERT(inputPitch % 16 == 0);
    ASSERT(outputPitch % 16 == 0);

    const unsigned int *source = reinterpret_cast<const unsigned int*>(input);
    unsigned int *dest = reinterpret_cast<unsigned int*>(output);

    // Round width up in case it is less than 4.
    int blocksAcross = (width + 3) / 4;
    int intsAcross = blocksAcross * 4;

    switch (height)
    {
        case 1:
            for (int x = 0; x < intsAcross; x += 4)
            {
                // just copy the block
                dest[x] = source[x];
                dest[x + 1] = source[x + 1];
                dest[x + 2] = source[x + 2];
                dest[x + 3] = source[x + 3];
            }
            break;
        case 2:
            for (int x = 0; x < intsAcross; x += 4)
            {
                FlipCopyDXT5BlockHalf(source + x, dest + x);
            }
            break;
        default:
            ASSERT(height % 4 == 0);
            for (int y = 0; y < height / 4; ++y)
            {
                const unsigned int *source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
                unsigned int *dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 16);

                for (int x = 0; x < intsAcross; x += 4)
                {
                    FlipCopyDXT5BlockFull(source + x, dest + x);
                }
            }
            break;
    }
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

    RECT sourceRect = transformPixelRect(x, y, width, height, description.Height);
    int destYOffset = transformPixelYOffset(yoffset, height, mHeight);
    RECT destRect = {xoffset, destYOffset, xoffset + width, destYOffset + height};

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

TextureStorage::TextureStorage(bool renderTarget)
    : mRenderTarget(renderTarget),
      mD3DPool(getDisplay()->getTexturePool(mRenderTarget)),
      mTextureSerial(issueTextureSerial())
{
}

TextureStorage::~TextureStorage()
{
}

bool TextureStorage::isRenderTarget() const
{
    return mRenderTarget;
}

bool TextureStorage::isManaged() const
{
    return (mD3DPool == D3DPOOL_MANAGED);
}

D3DPOOL TextureStorage::getPool() const
{
    return mD3DPool;
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
        D3DLOCKED_RECT locked;
        HRESULT result = image->lock(&locked, NULL);

        if (SUCCEEDED(result))
        {
            image->loadData(0, 0, image->getWidth(), image->getHeight(), image->getType(), unpackAlignment, pixels, locked.Pitch, locked.pBits);
            image->unlock();
        }

        mDirtyImages = true;
    }
}

void Texture::setCompressedImage(GLsizei imageSize, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        D3DLOCKED_RECT locked;
        HRESULT result = image->lock(&locked, NULL);

        if (SUCCEEDED(result))
        {
            int inputPitch = ComputeCompressedPitch(image->getWidth(), image->getFormat());
            int inputSize = ComputeCompressedSize(image->getWidth(), image->getHeight(), image->getFormat());
            image->loadCompressedData(0, 0, image->getWidth(), image->getHeight(), -inputPitch, static_cast<const char*>(pixels) + inputSize - inputPitch, locked.Pitch, locked.pBits);
            image->unlock();
        }

        mDirtyImages = true;
    }
}

bool Texture::subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *image)
{
    if (width + xoffset > image->getWidth() || height + yoffset > image->getHeight())
    {
        error(GL_INVALID_VALUE);
        return false;
    }

    if (IsCompressed(image->getFormat()))
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (format != image->getFormat())
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (pixels != NULL)
    {
        D3DLOCKED_RECT locked;
        HRESULT result = image->lock(&locked, NULL);

        if (SUCCEEDED(result))
        {
            image->loadData(xoffset, transformPixelYOffset(yoffset, height, image->getHeight()), width, height, type, unpackAlignment, pixels, locked.Pitch, locked.pBits);
            image->unlock();
        }

        mDirtyImages = true;
    }

    return true;
}

bool Texture::subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *image)
{
    if (width + xoffset > image->getWidth() || height + yoffset > image->getHeight())
    {
        error(GL_INVALID_VALUE);
        return false;
    }

    if (format != getInternalFormat())
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (pixels != NULL)
    {
        RECT updateRegion;
        updateRegion.left = xoffset;
        updateRegion.right = xoffset + width;
        updateRegion.bottom = yoffset + height;
        updateRegion.top = yoffset;

        D3DLOCKED_RECT locked;
        HRESULT result = image->lock(&locked, &updateRegion);

        if (SUCCEEDED(result))
        {
            int inputPitch = ComputeCompressedPitch(width, format);
            int inputSize = ComputeCompressedSize(width, height, format);
            image->loadCompressedData(xoffset, transformPixelYOffset(yoffset, height, image->getHeight()), width, height, -inputPitch, static_cast<const char*>(pixels) + inputSize - inputPitch, locked.Pitch, locked.pBits);
            image->unlock();
        }

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

TextureStorage2D::TextureStorage2D(IDirect3DTexture9 *surfaceTexture) : TextureStorage(true), mRenderTargetSerial(RenderbufferStorage::issueSerial())
{
    mTexture = surfaceTexture;
}

TextureStorage2D::TextureStorage2D(int levels, D3DFORMAT format, int width, int height, bool renderTarget)
    : TextureStorage(renderTarget), mRenderTargetSerial(RenderbufferStorage::issueSerial())
{
    IDirect3DDevice9 *device = getDevice();

    mTexture = NULL;
    HRESULT result = device->CreateTexture(width, height, levels, isRenderTarget() ? D3DUSAGE_RENDERTARGET : 0, format, getPool(), &mTexture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        error(GL_OUT_OF_MEMORY);
    }
}

TextureStorage2D::~TextureStorage2D()
{
    if (mTexture)
    {
        mTexture->Release();
    }
}

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
}

Texture2D::~Texture2D()
{
    mColorbufferProxy.set(NULL);

    delete mTexStorage;
    mTexStorage = NULL;
    
    if (mSurface)
    {
        mSurface->setBoundTexture(NULL);
        mSurface = NULL;
    }
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

GLenum Texture2D::getInternalFormat() const
{
    return mImageArray[0].getFormat();
}

GLenum Texture2D::getType() const
{
    return mImageArray[0].getType();
}

D3DFORMAT Texture2D::getD3DFormat() const
{
    return mImageArray[0].getD3DFormat();
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
            RECT sourceRect = transformPixelRect(x, y, width, height, source->getColorbuffer()->getHeight());
            sourceRect.left = clamp(sourceRect.left, 0, source->getColorbuffer()->getWidth());
            sourceRect.top = clamp(sourceRect.top, 0, source->getColorbuffer()->getHeight());
            sourceRect.right = clamp(sourceRect.right, 0, source->getColorbuffer()->getWidth());
            sourceRect.bottom = clamp(sourceRect.bottom, 0, source->getColorbuffer()->getHeight());

            GLint destYOffset = transformPixelYOffset(0, height, mImageArray[level].getHeight());
            
            IDirect3DSurface9 *dest = mTexStorage->getSurfaceLevel(level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, format, 0, destYOffset, dest);
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
            RECT sourceRect = transformPixelRect(x, y, width, height, source->getColorbuffer()->getHeight());
            sourceRect.left = clamp(sourceRect.left, 0, source->getColorbuffer()->getWidth());
            sourceRect.top = clamp(sourceRect.top, 0, source->getColorbuffer()->getHeight());
            sourceRect.right = clamp(sourceRect.right, 0, source->getColorbuffer()->getWidth());
            sourceRect.bottom = clamp(sourceRect.bottom, 0, source->getColorbuffer()->getHeight());

            GLint destYOffset = transformPixelYOffset(yoffset, height, mImageArray[level].getHeight());

            IDirect3DSurface9 *dest = mTexStorage->getSurfaceLevel(level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, mImageArray[0].getFormat(), xoffset, destYOffset, dest);
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
    const bool renderTarget = IsTextureFormatRenderable(d3dfmt) && (mUsage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);

    delete mTexStorage;
    mTexStorage = new TextureStorage2D(levels, d3dfmt, width, height, renderTarget);
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

    if ((getInternalFormat() == GL_FLOAT && !getContext()->supportsFloat32LinearFilter()) ||
        (getInternalFormat() == GL_HALF_FLOAT_OES && !getContext()->supportsFloat16LinearFilter()))
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

bool Texture2D::isCompressed() const
{
    return IsCompressed(getInternalFormat());
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
    D3DFORMAT format = mImageArray[0].getD3DFormat();
    const bool renderTarget = IsTextureFormatRenderable(format) && (mUsage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);

    delete mTexStorage;
    mTexStorage = new TextureStorage2D(levels, format, width, height, renderTarget);
    
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
        D3DFORMAT format = mImageArray[0].getD3DFormat();

        newTexStorage = new TextureStorage2D(levels, format, width, height, true);

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
                   source->Release();
                   dest->Release();
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

    if (mColorbufferProxy.get() == NULL)
    {
        mColorbufferProxy.set(new Renderbuffer(id(), new RenderbufferTexture(this, target)));
    }

    return mColorbufferProxy.get();
}

IDirect3DSurface9 *Texture2D::getRenderTarget(GLenum target)
{
    ASSERT(target == GL_TEXTURE_2D);

    // ensure the underlying texture is created
    if (getStorage(true) == NULL)
    {
        return NULL;
    }

    updateTexture();
    
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

TextureStorageCubeMap::TextureStorageCubeMap(int levels, D3DFORMAT format, int size, bool renderTarget)
    : TextureStorage(renderTarget), mFirstRenderTargetSerial(RenderbufferStorage::issueCubeSerials())
{
    IDirect3DDevice9 *device = getDevice();

    mTexture = NULL;
    HRESULT result = device->CreateCubeTexture(size, levels, isRenderTarget() ? D3DUSAGE_RENDERTARGET : 0, format, getPool(), &mTexture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        error(GL_OUT_OF_MEMORY);
    }
}

TextureStorageCubeMap::~TextureStorageCubeMap()
{
    if (mTexture)
    {
        mTexture->Release();
    }
}

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
}

TextureCubeMap::~TextureCubeMap()
{
    for (int i = 0; i < 6; i++)
    {
        mFaceProxies[i].set(NULL);
    }

    delete mTexStorage;
    mTexStorage = NULL;
}

GLenum TextureCubeMap::getTarget() const
{
    return GL_TEXTURE_CUBE_MAP;
}

GLsizei TextureCubeMap::getWidth(GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[0][level].getWidth();
    else
        return 0;
}

GLsizei TextureCubeMap::getHeight(GLint level) const
{
    if (level < IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[0][level].getHeight();
    else
        return 0;
}

GLenum TextureCubeMap::getInternalFormat() const
{
    return mImageArray[0][0].getFormat();
}

GLenum TextureCubeMap::getType() const
{
    return mImageArray[0][0].getType();
}

D3DFORMAT TextureCubeMap::getD3DFormat() const
{
    return mImageArray[0][0].getD3DFormat();
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
      default: UNREACHABLE();
    }

    if ((getInternalFormat() == GL_FLOAT && !getContext()->supportsFloat32LinearFilter()) ||
        (getInternalFormat() == GL_HALF_FLOAT_OES && !getContext()->supportsFloat16LinearFilter()))
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

bool TextureCubeMap::isCompressed() const
{
    return IsCompressed(getInternalFormat());
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
    D3DFORMAT format = mImageArray[0][0].getD3DFormat();
    const bool renderTarget = IsTextureFormatRenderable(format) && (mUsage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);

    delete mTexStorage;
    mTexStorage = new TextureStorageCubeMap(levels, format, size, renderTarget);

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
        D3DFORMAT format = mImageArray[0][0].getD3DFormat();

        newTexStorage = new TextureStorageCubeMap(levels, format, size, true);

        if (mTexStorage != NULL)
        {
            egl::Display *display = getDisplay();
            IDirect3DDevice9 *device = display->getDevice();

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
                       source->Release();
                       dest->Release();
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
            RECT sourceRect = transformPixelRect(x, y, width, height, source->getColorbuffer()->getHeight());
            sourceRect.left = clamp(sourceRect.left, 0, source->getColorbuffer()->getWidth());
            sourceRect.top = clamp(sourceRect.top, 0, source->getColorbuffer()->getHeight());
            sourceRect.right = clamp(sourceRect.right, 0, source->getColorbuffer()->getWidth());
            sourceRect.bottom = clamp(sourceRect.bottom, 0, source->getColorbuffer()->getHeight());

            GLint destYOffset = transformPixelYOffset(0, height, mImageArray[faceindex][level].getWidth());

            IDirect3DSurface9 *dest = mTexStorage->getCubeMapSurface(target, level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, format, 0, destYOffset, dest);
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
            RECT sourceRect = transformPixelRect(x, y, width, height, source->getColorbuffer()->getHeight());
            sourceRect.left = clamp(sourceRect.left, 0, source->getColorbuffer()->getWidth());
            sourceRect.top = clamp(sourceRect.top, 0, source->getColorbuffer()->getHeight());
            sourceRect.right = clamp(sourceRect.right, 0, source->getColorbuffer()->getWidth());
            sourceRect.bottom = clamp(sourceRect.bottom, 0, source->getColorbuffer()->getHeight());

            GLint destYOffset = transformPixelYOffset(yoffset, height, mImageArray[faceindex][level].getWidth());

            IDirect3DSurface9 *dest = mTexStorage->getCubeMapSurface(target, level);

            if (dest)
            {
                getBlitter()->copy(renderTarget, sourceRect, mImageArray[0][0].getFormat(), xoffset, destYOffset, dest);
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
    const bool renderTarget = IsTextureFormatRenderable(d3dfmt) && (mUsage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);

    delete mTexStorage;
    mTexStorage = new TextureStorageCubeMap(levels, d3dfmt, size, renderTarget);
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

    if (mFaceProxies[face].get() == NULL)
    {
        mFaceProxies[face].set(new Renderbuffer(id(), new RenderbufferTexture(this, target)));
    }

    return mFaceProxies[face].get();
}

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