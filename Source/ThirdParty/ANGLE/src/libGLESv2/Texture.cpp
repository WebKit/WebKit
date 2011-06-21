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
unsigned int Texture::mCurrentSerial = 1;

Texture::Image::Image()
  : width(0), height(0), dirty(false), surface(NULL), format(GL_NONE), type(GL_UNSIGNED_BYTE)
{
}

Texture::Image::~Image()
{
    if (surface)
    {
        surface->Release();
    }
}

bool Texture::Image::isRenderable() const
{    
    switch(getD3DFormat())
    {
      case D3DFMT_L8:
      case D3DFMT_A8L8:
      case D3DFMT_DXT1:
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

D3DFORMAT Texture::Image::getD3DFormat() const
{
    if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
        format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
    {
        return D3DFMT_DXT1;
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

Texture::Texture(GLuint id) : RefCountObject(id), mSerial(issueSerial())
{
    mMinFilter = GL_NEAREST_MIPMAP_LINEAR;
    mMagFilter = GL_LINEAR;
    mWrapS = GL_REPEAT;
    mWrapT = GL_REPEAT;
    mDirtyParameter = true;
    
    mDirtyImage = true;
    
    mIsRenderable = false;
}

Texture::~Texture()
{
}

Blit *Texture::getBlitter()
{
    Context *context = getContext();
    return context->getBlitter();
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
                mDirtyParameter = true;
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
                mDirtyParameter = true;
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
                mDirtyParameter = true;
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
                mDirtyParameter = true;
            }
            return true;
        }
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

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the target pixel rectangle at output with outputPitch bytes in between each line.
void Texture::loadImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
                            GLint unpackAlignment, const void *input, size_t outputPitch, void *output, D3DSURFACE_DESC *description) const
{
    GLsizei inputPitch = -ComputePitch(width, format, type, unpackAlignment);
    input = ((char*)input) - inputPitch * (height - 1);

    switch (type)
    {
      case GL_UNSIGNED_BYTE:
        switch (format)
        {
          case GL_ALPHA:
            loadAlphaImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE:
            loadLuminanceImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output, description->Format == D3DFMT_L8);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output, description->Format == D3DFMT_A8L8);
            break;
          case GL_RGB:
            loadRGBUByteImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGBA:
            if (supportsSSE2())
            {
                loadRGBAUByteImageDataSSE2(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            }
            else
            {
                loadRGBAUByteImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            }
            break;
          case GL_BGRA_EXT:
            loadBGRAImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_5_6_5:
        switch (format)
        {
          case GL_RGB:
            loadRGB565ImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_4_4_4_4:
        switch (format)
        {
          case GL_RGBA:
            loadRGBA4444ImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_5_5_5_1:
        switch (format)
        {
          case GL_RGBA:
            loadRGBA5551ImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_FLOAT:
        switch (format)
        {
          // float textures are converted to RGBA, not BGRA, as they're stored that way in D3D
          case GL_ALPHA:
            loadAlphaFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE:
            loadLuminanceFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGB:
            loadRGBFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGBA:
            loadRGBAFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      case GL_HALF_FLOAT_OES:
        switch (format)
        {
          // float textures are converted to RGBA, not BGRA, as they're stored that way in D3D
          case GL_ALPHA:
            loadAlphaHalfFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE:
            loadLuminanceHalfFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_LUMINANCE_ALPHA:
            loadLuminanceAlphaHalfFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGB:
            loadRGBHalfFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          case GL_RGBA:
            loadRGBAHalfFloatImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
            break;
          default: UNREACHABLE();
        }
        break;
      default: UNREACHABLE();
    }
}

void Texture::loadAlphaImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadAlphaFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadAlphaHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadLuminanceImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadLuminanceFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadLuminanceHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadLuminanceAlphaImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadLuminanceAlphaFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadLuminanceAlphaHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGB565ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBAUByteImageDataSSE2(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBAUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBA4444ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBA5551ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBAFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadRGBAHalfFloatImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadBGRAImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
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

void Texture::loadCompressedImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                      int inputPitch, const void *input, size_t outputPitch, void *output) const
{
    ASSERT(xoffset % 4 == 0);
    ASSERT(yoffset % 4 == 0);
    ASSERT(width % 4 == 0 || width == 2 || width == 1);
    ASSERT(inputPitch % 8 == 0);
    ASSERT(outputPitch % 8 == 0);

    const unsigned int *source = reinterpret_cast<const unsigned int*>(input);
    unsigned int *dest = reinterpret_cast<unsigned int*>(output);

    switch (height)
    {
        case 1:
            // Round width up in case it is 1.
            for (int x = 0; x < (width + 1) / 2; x += 2)
            {
                // First 32-bits is two RGB565 colors shared by tile and does not need to be modified.
                dest[x] = source[x];

                // Second 32-bits contains 4 rows of 4 2-bit interpolants between the colors, the last 3 rows being unused. No flipping should occur.
                dest[x + 1] = source[x + 1];
            }
            break;
        case 2:
            // Round width up in case it is 1.
            for (int x = 0; x < (width + 1) / 2; x += 2)
            {
                // First 32-bits is two RGB565 colors shared by tile and does not need to be modified.
                dest[x] = source[x];

                // Second 32-bits contains 4 rows of 4 2-bit interpolants between the colors, the last 2 rows being unused. Only the top 2 rows should be flipped.
                dest[x + 1] = ((source[x + 1] << 8) & 0x0000FF00) |
                              ((source[x + 1] >> 8) & 0x000000FF);       
            }
            break;
        default:
            ASSERT(height % 4 == 0);
            for (int y = 0; y < height / 4; ++y)
            {
                const unsigned int *source = reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(input) + y * inputPitch);
                unsigned int *dest = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 8);

                // Round width up in case it is 1.
                for (int x = 0; x < (width + 1) / 2; x += 2)
                {
                    // First 32-bits is two RGB565 colors shared by tile and does not need to be modified.
                    dest[x] = source[x];

                    // Second 32-bits contains 4 rows of 4 2-bit interpolants between the colors. All rows should be flipped.
                    dest[x + 1] = (source[x + 1] >> 24) | 
                                  ((source[x + 1] << 8) & 0x00FF0000) |
                                  ((source[x + 1] >> 8) & 0x0000FF00) |
                                  (source[x + 1] << 24);                    
                }
            }
            break;
    }
}

void Texture::createSurface(Image *image)
{
    IDirect3DTexture9 *newTexture = NULL;
    IDirect3DSurface9 *newSurface = NULL;

    if (image->width != 0 && image->height != 0)
    {
        int levelToFetch = 0;
        GLsizei requestWidth = image->width;
        GLsizei requestHeight = image->height;
        if (IsCompressed(image->format) && (image->width % 4 != 0 || image->height % 4 != 0))
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

        HRESULT result = getDevice()->CreateTexture(requestWidth, requestHeight, levelToFetch + 1, NULL, image->getD3DFormat(),
                                                    D3DPOOL_SYSTEMMEM, &newTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY);
        }

        newTexture->GetSurfaceLevel(levelToFetch, &newSurface);
        newTexture->Release();
    }

    if (image->surface)
    {
        image->surface->Release();
    }

    image->surface = newSurface;
}

void Texture::setImage(GLint unpackAlignment, const void *pixels, Image *image)
{
    createSurface(image);

    if (pixels != NULL && image->surface != NULL)
    {
        D3DSURFACE_DESC description;
        image->surface->GetDesc(&description);

        D3DLOCKED_RECT locked;
        HRESULT result = image->surface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            loadImageData(0, 0, image->width, image->height, image->format, image->type, unpackAlignment, pixels, locked.Pitch, locked.pBits, &description);
            image->surface->UnlockRect();
        }

        image->dirty = true;
        mDirtyImage = true;
    }
}

void Texture::setCompressedImage(GLsizei imageSize, const void *pixels, Image *image)
{
    createSurface(image);

    if (pixels != NULL && image->surface != NULL)
    {
        D3DLOCKED_RECT locked;
        HRESULT result = image->surface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            int inputPitch = ComputeCompressedPitch(image->width, image->format);
            int inputSize = ComputeCompressedSize(image->width, image->height, image->format);
            loadCompressedImageData(0, 0, image->width, image->height, -inputPitch, static_cast<const char*>(pixels) + inputSize - inputPitch, locked.Pitch, locked.pBits);
            image->surface->UnlockRect();
        }

        image->dirty = true;
        mDirtyImage = true;
    }
}

bool Texture::subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *image)
{
    if (width + xoffset > image->width || height + yoffset > image->height)
    {
        error(GL_INVALID_VALUE);
        return false;
    }

    if (IsCompressed(image->format))
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (format != image->format)
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (!image->surface)
    {
        createSurface(image);
    }

    if (pixels != NULL && image->surface != NULL)
    {
        D3DSURFACE_DESC description;
        image->surface->GetDesc(&description);

        D3DLOCKED_RECT locked;
        HRESULT result = image->surface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            loadImageData(xoffset, transformPixelYOffset(yoffset, height, image->height), width, height, format, type, unpackAlignment, pixels, locked.Pitch, locked.pBits, &description);
            image->surface->UnlockRect();
        }

        image->dirty = true;
        mDirtyImage = true;
    }

    return true;
}

bool Texture::subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *image)
{
    if (width + xoffset > image->width || height + yoffset > image->height)
    {
        error(GL_INVALID_VALUE);
        return false;
    }

    if (format != getInternalFormat())
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (!image->surface)
    {
        createSurface(image);
    }

    if (pixels != NULL && image->surface != NULL)
    {
        RECT updateRegion;
        updateRegion.left = xoffset;
        updateRegion.right = xoffset + width;
        updateRegion.bottom = yoffset + height;
        updateRegion.top = yoffset;

        D3DLOCKED_RECT locked;
        HRESULT result = image->surface->LockRect(&locked, &updateRegion, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            int inputPitch = ComputeCompressedPitch(width, format);
            int inputSize = ComputeCompressedSize(width, height, format);
            loadCompressedImageData(xoffset, transformPixelYOffset(yoffset, height, image->height), width, height, -inputPitch, static_cast<const char*>(pixels) + inputSize - inputPitch, locked.Pitch, locked.pBits);
            image->surface->UnlockRect();
        }

        image->dirty = true;
        mDirtyImage = true;
    }

    return true;
}

// This implements glCopyTex[Sub]Image2D for non-renderable internal texture formats and incomplete textures
void Texture::copyToImage(Image *image, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, IDirect3DSurface9 *renderTarget)
{
    if (!image->surface)
    {
        createSurface(image);

        if (!image->surface)
        {
            ERR("Failed to create an image surface.");
            return error(GL_OUT_OF_MEMORY);
        }
    }

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
    int destYOffset = transformPixelYOffset(yoffset, height, image->height);
    RECT destRect = {xoffset, destYOffset, xoffset + width, destYOffset + height};

    if (image->isRenderable())
    {
        result = D3DXLoadSurfaceFromSurface(image->surface, NULL, &destRect, renderTargetData, NULL, &sourceRect, D3DX_FILTER_BOX, 0);
        
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
        result = image->surface->LockRect(&destLock, &destRect, 0);
        
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
                switch(image->getD3DFormat())
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
                switch(image->getD3DFormat())
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
                switch(image->getD3DFormat())
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

        image->surface->UnlockRect();
        renderTargetData->UnlockRect();
    }

    renderTargetData->Release();

    image->dirty = true;
    mDirtyImage = true;
}

IDirect3DBaseTexture9 *Texture::getTexture()
{
    if (!isComplete())
    {
        return NULL;
    }

    if (!getBaseTexture())
    {
        createTexture();
    }

    updateTexture();

    return getBaseTexture();
}

bool Texture::isDirtyParameter() const
{
    return mDirtyParameter;
}

bool Texture::isDirtyImage() const
{
    return mDirtyImage;
}

void Texture::resetDirty()
{
    mDirtyParameter = false;
    mDirtyImage = false;
}

unsigned int Texture::getSerial() const
{
    return mSerial;
}

GLint Texture::creationLevels(GLsizei width, GLsizei height, GLint maxlevel) const
{
    if ((isPow2(width) && isPow2(height)) || getContext()->supportsNonPower2Texture())
    {
        return maxlevel;
    }
    else
    {
        // OpenGL ES 2.0 without GL_OES_texture_npot does not permit NPOT mipmaps.
        return 1;
    }
}

GLint Texture::creationLevels(GLsizei size, GLint maxlevel) const
{
    return creationLevels(size, size, maxlevel);
}

int Texture::levelCount() const
{
    return getBaseTexture() ? getBaseTexture()->GetLevelCount() : 0;
}

unsigned int Texture::issueSerial()
{
    return mCurrentSerial++;
}

Texture2D::Texture2D(GLuint id) : Texture(id)
{
    mTexture = NULL;
    mSurface = NULL;
}

Texture2D::~Texture2D()
{
    mColorbufferProxy.set(NULL);

    if (mTexture)
    {
        mTexture->Release();
        mTexture = NULL;
    }

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

GLsizei Texture2D::getWidth() const
{
    return mImageArray[0].width;
}

GLsizei Texture2D::getHeight() const
{
    return mImageArray[0].height;
}

GLenum Texture2D::getInternalFormat() const
{
    return mImageArray[0].format;
}

GLenum Texture2D::getType() const
{
    return mImageArray[0].type;
}

D3DFORMAT Texture2D::getD3DFormat() const
{
    return mImageArray[0].getD3DFormat();
}

void Texture2D::redefineTexture(GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type, bool forceRedefine)
{
    GLsizei textureWidth = mImageArray[0].width;
    GLsizei textureHeight = mImageArray[0].height;
    GLenum textureFormat = mImageArray[0].format;
    GLenum textureType = mImageArray[0].type;

    mImageArray[level].width = width;
    mImageArray[level].height = height;
    mImageArray[level].format = format;
    mImageArray[level].type = type;

    if (!mTexture)
    {
        return;
    }

    bool widthOkay = (textureWidth >> level == width) || (textureWidth >> level == 0 && width == 1);
    bool heightOkay = (textureHeight >> level == height) || (textureHeight >> level == 0 && height == 1);
    bool textureOkay = (widthOkay && heightOkay && textureFormat == format && textureType == type);

    if (!textureOkay || forceRedefine || mSurface)   // Purge all the levels and the texture.
    {
        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            if (mImageArray[i].surface != NULL)
            {
                mImageArray[i].surface->Release();
                mImageArray[i].surface = NULL;
                mImageArray[i].dirty = true;
            }
        }

        if (mTexture != NULL)
        {
            mTexture->Release();
            mTexture = NULL;
            mDirtyImage = true;
            mIsRenderable = false;
        }

        if (mSurface)
        {
            mSurface->setBoundTexture(NULL);
            mSurface = NULL;
        }

        mColorbufferProxy.set(NULL);
    }
}

void Texture2D::setImage(GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineTexture(level, format, width, height, type, false);

    Texture::setImage(unpackAlignment, pixels, &mImageArray[level]);
}

void Texture2D::bindTexImage(egl::Surface *surface)
{
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

    redefineTexture(0, format, surface->getWidth(), surface->getHeight(), GL_UNSIGNED_BYTE, true);

    IDirect3DTexture9 *texture = surface->getOffscreenTexture();

    mTexture = texture;
    mDirtyImage = true;
    mIsRenderable = true;
    mSurface = surface;
    mSurface->setBoundTexture(this);
}

void Texture2D::releaseTexImage()
{
    redefineTexture(0, GL_RGB, 0, 0, GL_UNSIGNED_BYTE, true);
}

void Texture2D::setCompressedImage(GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels)
{
    redefineTexture(level, format, width, height, GL_UNSIGNED_BYTE, false);

    Texture::setCompressedImage(imageSize, pixels, &mImageArray[level]);
}

void Texture2D::commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(mImageArray[level].surface != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = NULL;
        HRESULT result = mTexture->GetSurfaceLevel(level, &destLevel);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            Image *image = &mImageArray[level];

            RECT sourceRect = transformPixelRect(xoffset, yoffset, width, height, image->height);;

            POINT destPoint;
            destPoint.x = sourceRect.left;
            destPoint.y = sourceRect.top;

            result = getDevice()->UpdateSurface(image->surface, &sourceRect, destLevel, &destPoint);
            ASSERT(SUCCEEDED(result));

            destLevel->Release();

            image->dirty = false;
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

    redefineTexture(level, format, width, height, GL_UNSIGNED_BYTE, false);
   
    if (!mImageArray[level].isRenderable())
    {
        copyToImage(&mImageArray[level], 0, 0, x, y, width, height, renderTarget);
    }
    else
    {
        if (!mTexture || !mIsRenderable)
        {
            convertToRenderTarget();
        }
        
        updateTexture();

        if (width != 0 && height != 0 && level < levelCount())
        {
            RECT sourceRect = transformPixelRect(x, y, width, height, source->getColorbuffer()->getHeight());
            sourceRect.left = clamp(sourceRect.left, 0, source->getColorbuffer()->getWidth());
            sourceRect.top = clamp(sourceRect.top, 0, source->getColorbuffer()->getHeight());
            sourceRect.right = clamp(sourceRect.right, 0, source->getColorbuffer()->getWidth());
            sourceRect.bottom = clamp(sourceRect.bottom, 0, source->getColorbuffer()->getHeight());

            GLint destYOffset = transformPixelYOffset(0, height, mImageArray[level].height);
            
            IDirect3DSurface9 *dest;
            HRESULT hr = mTexture->GetSurfaceLevel(level, &dest);

            getBlitter()->copy(source->getRenderTarget(), sourceRect, format, 0, destYOffset, dest);
            dest->Release();
        }
    }
}

void Texture2D::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source)
{
    if (xoffset + width > mImageArray[level].width || yoffset + height > mImageArray[level].height)
    {
        return error(GL_INVALID_VALUE);
    }

    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    redefineTexture(level, mImageArray[level].format, mImageArray[level].width, mImageArray[level].height, GL_UNSIGNED_BYTE, false);
   
    if (!mImageArray[level].isRenderable() || (!mTexture && !isComplete()))
    {
        copyToImage(&mImageArray[level], xoffset, yoffset, x, y, width, height, renderTarget);
    }
    else
    {
        if (!mTexture || !mIsRenderable)
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

            GLint destYOffset = transformPixelYOffset(yoffset, height, mImageArray[level].height);

            IDirect3DSurface9 *dest;
            HRESULT hr = mTexture->GetSurfaceLevel(level, &dest);

            getBlitter()->copy(source->getRenderTarget(), sourceRect, mImageArray[0].format, xoffset, destYOffset, dest);
            dest->Release();
        }
    }
}

// Tests for GL texture object completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool Texture2D::isComplete() const
{
    GLsizei width = mImageArray[0].width;
    GLsizei height = mImageArray[0].height;

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

    if ((getInternalFormat() == GL_FLOAT && !getContext()->supportsFloatLinearFilter()) ||
        (getInternalFormat() == GL_HALF_FLOAT_OES && !getContext()->supportsHalfFloatLinearFilter()))
    {
        if (mMagFilter != GL_NEAREST || (mMinFilter != GL_NEAREST && mMinFilter != GL_NEAREST_MIPMAP_NEAREST))
        {
            return false;
        }
    }

    bool npot = getContext()->supportsNonPower2Texture();

    if (!npot)
    {
        if ((getWrapS() != GL_CLAMP_TO_EDGE && !isPow2(width)) ||
            (getWrapT() != GL_CLAMP_TO_EDGE && !isPow2(height)))
        {
            return false;
        }
    }

    if (mipmapping)
    {
        if (!npot)
        {
            if (!isPow2(width) || !isPow2(height))
            {
                return false;
            }
        }

        int q = log2(std::max(width, height));

        for (int level = 1; level <= q; level++)
        {
            if (mImageArray[level].format != mImageArray[0].format)
            {
                return false;
            }

            if (mImageArray[level].type != mImageArray[0].type)
            {
                return false;
            }

            if (mImageArray[level].width != std::max(1, width >> level))
            {
                return false;
            }

            if (mImageArray[level].height != std::max(1, height >> level))
            {
                return false;
            }
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
    return mTexture;
}

// Constructs a Direct3D 9 texture resource from the texture images
void Texture2D::createTexture()
{
    IDirect3DDevice9 *device = getDevice();
    D3DFORMAT format = mImageArray[0].getD3DFormat();
    GLint levels = creationLevels(mImageArray[0].width, mImageArray[0].height, 0);

    IDirect3DTexture9 *texture = NULL;
    HRESULT result = device->CreateTexture(mImageArray[0].width, mImageArray[0].height, levels, 0, format, D3DPOOL_DEFAULT, &texture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return error(GL_OUT_OF_MEMORY);
    }

    if (mTexture)
    {
        mTexture->Release();
    }

    mTexture = texture;
    mDirtyImage = true;
    mIsRenderable = false;
}

void Texture2D::updateTexture()
{
    IDirect3DDevice9 *device = getDevice();

    int levels = levelCount();

    for (int level = 0; level < levels; level++)
    {
        if (mImageArray[level].surface && mImageArray[level].dirty)
        {
            IDirect3DSurface9 *levelSurface = NULL;
            HRESULT result = mTexture->GetSurfaceLevel(level, &levelSurface);

            ASSERT(SUCCEEDED(result));

            if (SUCCEEDED(result))
            {
                result = device->UpdateSurface(mImageArray[level].surface, NULL, levelSurface, NULL);
                ASSERT(SUCCEEDED(result));

                levelSurface->Release();

                mImageArray[level].dirty = false;
            }
        }
    }
}

void Texture2D::convertToRenderTarget()
{
    IDirect3DTexture9 *texture = NULL;

    if (mImageArray[0].width != 0 && mImageArray[0].height != 0)
    {
        egl::Display *display = getDisplay();
        IDirect3DDevice9 *device = getDevice();
        D3DFORMAT format = mImageArray[0].getD3DFormat();
        GLint levels = creationLevels(mImageArray[0].width, mImageArray[0].height, 0);

        HRESULT result = device->CreateTexture(mImageArray[0].width, mImageArray[0].height, levels, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY);
        }

        if (mTexture != NULL)
        {
            int levels = levelCount();
            for (int i = 0; i < levels; i++)
            {
                IDirect3DSurface9 *source;
                result = mTexture->GetSurfaceLevel(i, &source);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();

                    return error(GL_OUT_OF_MEMORY);
                }

                IDirect3DSurface9 *dest;
                result = texture->GetSurfaceLevel(i, &dest);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();
                    source->Release();

                    return error(GL_OUT_OF_MEMORY);
                }

                display->endScene();
                result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();
                    source->Release();
                    dest->Release();

                    return error(GL_OUT_OF_MEMORY);
                }

                source->Release();
                dest->Release();
            }
        }
    }

    if (mTexture != NULL)
    {
        mTexture->Release();
    }

    mTexture = texture;
    mDirtyImage = true;
    mIsRenderable = true;
}

void Texture2D::generateMipmaps()
{
    if (!getContext()->supportsNonPower2Texture())
    {
        if (!isPow2(mImageArray[0].width) || !isPow2(mImageArray[0].height))
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(std::max(mImageArray[0].width, mImageArray[0].height));
    for (unsigned int i = 1; i <= q; i++)
    {
        if (mImageArray[i].surface != NULL)
        {
            mImageArray[i].surface->Release();
            mImageArray[i].surface = NULL;
        }

        mImageArray[i].width = std::max(mImageArray[0].width >> i, 1);
        mImageArray[i].height = std::max(mImageArray[0].height >> i, 1);
        mImageArray[i].format = mImageArray[0].format;
        mImageArray[i].type = mImageArray[0].type;
    }

    if (mIsRenderable)
    {
        if (mTexture == NULL)
        {
            ERR(" failed because mTexture was null.");
            return;
        }

        for (unsigned int i = 1; i <= q; i++)
        {
            IDirect3DSurface9 *upper = NULL;
            IDirect3DSurface9 *lower = NULL;

            mTexture->GetSurfaceLevel(i-1, &upper);
            mTexture->GetSurfaceLevel(i, &lower);

            if (upper != NULL && lower != NULL)
            {
                getBlitter()->boxFilter(upper, lower);
            }

            if (upper != NULL) upper->Release();
            if (lower != NULL) lower->Release();

            mImageArray[i].dirty = false;
        }
    }
    else
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            createSurface(&mImageArray[i]);
            
            if (mImageArray[i].surface == NULL)
            {
                return error(GL_OUT_OF_MEMORY);
            }

            if (FAILED(D3DXLoadSurfaceFromSurface(mImageArray[i].surface, NULL, NULL, mImageArray[i - 1].surface, NULL, NULL, D3DX_FILTER_BOX, 0)))
            {
                ERR(" failed to load filter %d to %d.", i - 1, i);
            }

            mImageArray[i].dirty = true;
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
        mColorbufferProxy.set(new Renderbuffer(id(), new Colorbuffer(this, target)));
    }

    return mColorbufferProxy.get();
}

IDirect3DSurface9 *Texture2D::getRenderTarget(GLenum target)
{
    ASSERT(target == GL_TEXTURE_2D);

    if (!mIsRenderable)
    {
        convertToRenderTarget();
    }

    if (mTexture == NULL)
    {
        return NULL;
    }

    updateTexture();
    
    IDirect3DSurface9 *renderTarget = NULL;
    mTexture->GetSurfaceLevel(0, &renderTarget);

    return renderTarget;
}

TextureCubeMap::TextureCubeMap(GLuint id) : Texture(id)
{
    mTexture = NULL;
}

TextureCubeMap::~TextureCubeMap()
{
    for (int i = 0; i < 6; i++)
    {
        mFaceProxies[i].set(NULL);
    }

    if (mTexture)
    {
        mTexture->Release();
        mTexture = NULL;
    }
}

GLenum TextureCubeMap::getTarget() const
{
    return GL_TEXTURE_CUBE_MAP;
}

GLsizei TextureCubeMap::getWidth() const
{
    return mImageArray[0][0].width;
}

GLsizei TextureCubeMap::getHeight() const
{
    return mImageArray[0][0].height;
}

GLenum TextureCubeMap::getInternalFormat() const
{
    return mImageArray[0][0].format;
}

GLenum TextureCubeMap::getType() const
{
    return mImageArray[0][0].type;
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
    redefineTexture(faceIndex(face), level, format, width, height, GL_UNSIGNED_BYTE);

    Texture::setCompressedImage(imageSize, pixels, &mImageArray[faceIndex(face)][level]);
}

void TextureCubeMap::commitRect(GLenum faceTarget, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    int face = faceIndex(faceTarget);
    ASSERT(mImageArray[face][level].surface != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = getCubeMapSurface(faceTarget, level);
        ASSERT(destLevel != NULL);

        if (destLevel != NULL)
        {
            Image *image = &mImageArray[face][level];

            RECT sourceRect = transformPixelRect(xoffset, yoffset, width, height, image->height);;

            POINT destPoint;
            destPoint.x = sourceRect.left;
            destPoint.y = sourceRect.top;

            HRESULT result = getDevice()->UpdateSurface(image->surface, &sourceRect, destLevel, &destPoint);
            ASSERT(SUCCEEDED(result));

            destLevel->Release();

            image->dirty = false;
        }
    }
}

void TextureCubeMap::subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    if (Texture::subImage(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, &mImageArray[faceIndex(target)][level]))
    {
        commitRect(target, level, xoffset, yoffset, width, height);
    }
}

void TextureCubeMap::subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels)
{
    if (Texture::subImageCompressed(xoffset, yoffset, width, height, format, imageSize, pixels, &mImageArray[faceIndex(target)][level]))
    {
        commitRect(target, level, xoffset, yoffset, width, height);
    }
}

// Tests for GL texture object completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool TextureCubeMap::isComplete() const
{
    int size = mImageArray[0][0].width;

    if (size <= 0)
    {
        return false;
    }

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

    for (int face = 0; face < 6; face++)
    {
        if (mImageArray[face][0].width != size || mImageArray[face][0].height != size)
        {
            return false;
        }
    }

    if ((getInternalFormat() == GL_FLOAT && !getContext()->supportsFloatLinearFilter()) ||
        (getInternalFormat() == GL_HALF_FLOAT_OES && !getContext()->supportsHalfFloatLinearFilter()))
    {
        if (mMagFilter != GL_NEAREST || (mMinFilter != GL_NEAREST && mMinFilter != GL_NEAREST_MIPMAP_NEAREST))
        {
            return false;
        }
    }

    bool npot = getContext()->supportsNonPower2Texture();

    if (!npot)
    {
        if ((getWrapS() != GL_CLAMP_TO_EDGE || getWrapT() != GL_CLAMP_TO_EDGE) && !isPow2(size))
        {
            return false;
        }
    }

    if (mipmapping)
    {
        if (!npot)
        {
            if (!isPow2(size))
            {
                return false;
            }
        }

        int q = log2(size);

        for (int face = 0; face < 6; face++)
        {
            for (int level = 1; level <= q; level++)
            {
                if (mImageArray[face][level].format != mImageArray[0][0].format)
                {
                    return false;
                }

                if (mImageArray[face][level].type != mImageArray[0][0].type)
                {
                    return false;
                }

                if (mImageArray[face][level].width != std::max(1, size >> level))
                {
                    return false;
                }

                ASSERT(mImageArray[face][level].height == mImageArray[face][level].width);
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
    return mTexture;
}

// Constructs a Direct3D 9 texture resource from the texture images, or returns an existing one
void TextureCubeMap::createTexture()
{
    IDirect3DDevice9 *device = getDevice();
    D3DFORMAT format = mImageArray[0][0].getD3DFormat();
    GLint levels = creationLevels(mImageArray[0][0].width, 0);

    IDirect3DCubeTexture9 *texture = NULL;
    HRESULT result = device->CreateCubeTexture(mImageArray[0][0].width, levels, 0, format, D3DPOOL_DEFAULT, &texture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return error(GL_OUT_OF_MEMORY);
    }

    if (mTexture)
    {
        mTexture->Release();
    }

    mTexture = texture;
    mDirtyImage = true;
    mIsRenderable = false;
}

void TextureCubeMap::updateTexture()
{
    IDirect3DDevice9 *device = getDevice();

    for (int face = 0; face < 6; face++)
    {
        int levels = levelCount();
        for (int level = 0; level < levels; level++)
        {
            Image *image = &mImageArray[face][level];

            if (image->surface && image->dirty)
            {
                IDirect3DSurface9 *levelSurface = getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level);
                ASSERT(levelSurface != NULL);

                if (levelSurface != NULL)
                {
                    HRESULT result = device->UpdateSurface(image->surface, NULL, levelSurface, NULL);
                    ASSERT(SUCCEEDED(result));

                    levelSurface->Release();

                    image->dirty = false;
                }
            }
        }
    }
}

void TextureCubeMap::convertToRenderTarget()
{
    IDirect3DCubeTexture9 *texture = NULL;

    if (mImageArray[0][0].width != 0)
    {
        egl::Display *display = getDisplay();
        IDirect3DDevice9 *device = getDevice();
        D3DFORMAT format = mImageArray[0][0].getD3DFormat();
        GLint levels = creationLevels(mImageArray[0][0].width, 0);

        HRESULT result = device->CreateCubeTexture(mImageArray[0][0].width, levels, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY);
        }

        if (mTexture != NULL)
        {
            int levels = levelCount();
            for (int f = 0; f < 6; f++)
            {
                for (int i = 0; i < levels; i++)
                {
                    IDirect3DSurface9 *source;
                    result = mTexture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(f), i, &source);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();

                        return error(GL_OUT_OF_MEMORY);
                    }

                    IDirect3DSurface9 *dest;
                    result = texture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(f), i, &dest);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();
                        source->Release();

                        return error(GL_OUT_OF_MEMORY);
                    }

                    display->endScene();
                    result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();
                        source->Release();
                        dest->Release();

                        return error(GL_OUT_OF_MEMORY);
                    }
                }
            }
        }
    }

    if (mTexture != NULL)
    {
        mTexture->Release();
    }

    mTexture = texture;
    mDirtyImage = true;
    mIsRenderable = true;
}

void TextureCubeMap::setImage(int faceIndex, GLint level, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineTexture(faceIndex, level, format, width, height, type);

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

void TextureCubeMap::redefineTexture(int face, GLint level, GLenum format, GLsizei width, GLsizei height, GLenum type)
{
    GLsizei textureWidth = mImageArray[0][0].width;
    GLsizei textureHeight = mImageArray[0][0].height;
    GLenum textureFormat = mImageArray[0][0].format;
    GLenum textureType = mImageArray[0][0].type;

    mImageArray[face][level].width = width;
    mImageArray[face][level].height = height;
    mImageArray[face][level].format = format;
    mImageArray[face][level].type = type;

    if (!mTexture)
    {
        return;
    }

    bool sizeOkay = (textureWidth >> level == width);
    bool textureOkay = (sizeOkay && textureFormat == format && textureType == type);

    if (!textureOkay)   // Purge all the levels and the texture.
    {
        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            for (int f = 0; f < 6; f++)
            {
                if (mImageArray[f][i].surface != NULL)
                {
                    mImageArray[f][i].surface->Release();
                    mImageArray[f][i].surface = NULL;
                    mImageArray[f][i].dirty = true;
                }
            }
        }

        if (mTexture != NULL)
        {
            mTexture->Release();
            mTexture = NULL;
            mDirtyImage = true;
            mIsRenderable = false;
        }
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
    redefineTexture(faceindex, level, format, width, height, GL_UNSIGNED_BYTE);

    if (!mImageArray[faceindex][level].isRenderable())
    {
        copyToImage(&mImageArray[faceindex][level], 0, 0, x, y, width, height, renderTarget);
    }
    else
    {
        if (!mTexture || !mIsRenderable)
        {
            convertToRenderTarget();
        }
        
        updateTexture();

        ASSERT(width == height);

        if (width > 0 && level < levelCount())
        {
            RECT sourceRect = transformPixelRect(x, y, width, height, source->getColorbuffer()->getHeight());
            sourceRect.left = clamp(sourceRect.left, 0, source->getColorbuffer()->getWidth());
            sourceRect.top = clamp(sourceRect.top, 0, source->getColorbuffer()->getHeight());
            sourceRect.right = clamp(sourceRect.right, 0, source->getColorbuffer()->getWidth());
            sourceRect.bottom = clamp(sourceRect.bottom, 0, source->getColorbuffer()->getHeight());

            GLint destYOffset = transformPixelYOffset(0, height, mImageArray[faceindex][level].width);

            IDirect3DSurface9 *dest = getCubeMapSurface(target, level);

            getBlitter()->copy(source->getRenderTarget(), sourceRect, format, 0, destYOffset, dest);
            dest->Release();
        }
    }
}

IDirect3DSurface9 *TextureCubeMap::getCubeMapSurface(GLenum face, unsigned int level)
{
    if (mTexture == NULL)
    {
        UNREACHABLE();
        return NULL;
    }

    IDirect3DSurface9 *surface = NULL;

    HRESULT hr = mTexture->GetCubeMapSurface(es2dx::ConvertCubeFace(face), level, &surface);

    return (SUCCEEDED(hr)) ? surface : NULL;
}

void TextureCubeMap::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Framebuffer *source)
{
    GLsizei size = mImageArray[faceIndex(target)][level].width;

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
    redefineTexture(faceindex, level, mImageArray[faceindex][level].format, mImageArray[faceindex][level].width, mImageArray[faceindex][level].height, GL_UNSIGNED_BYTE);
   
    if (!mImageArray[faceindex][level].isRenderable() || (!mTexture && !isComplete()))
    {
        copyToImage(&mImageArray[faceindex][level], 0, 0, x, y, width, height, renderTarget);
    }
    else
    {
        if (!mTexture || !mIsRenderable)
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

            GLint destYOffset = transformPixelYOffset(yoffset, height, mImageArray[faceindex][level].width);

            IDirect3DSurface9 *dest = getCubeMapSurface(target, level);

            getBlitter()->copy(source->getRenderTarget(), sourceRect, mImageArray[0][0].format, xoffset, destYOffset, dest);
            dest->Release();
        }
    }
}

bool TextureCubeMap::isCubeComplete() const
{
    if (mImageArray[0][0].width == 0)
    {
        return false;
    }

    for (unsigned int f = 1; f < 6; f++)
    {
        if (mImageArray[f][0].width != mImageArray[0][0].width
            || mImageArray[f][0].format != mImageArray[0][0].format)
        {
            return false;
        }
    }

    return true;
}

void TextureCubeMap::generateMipmaps()
{
    if (!isCubeComplete())
    {
        return error(GL_INVALID_OPERATION);
    }

    if (!getContext()->supportsNonPower2Texture())
    {
        if (!isPow2(mImageArray[0][0].width))
        {
            return error(GL_INVALID_OPERATION);
        }
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(mImageArray[0][0].width);
    for (unsigned int f = 0; f < 6; f++)
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            if (mImageArray[f][i].surface != NULL)
            {
                mImageArray[f][i].surface->Release();
                mImageArray[f][i].surface = NULL;
            }

            mImageArray[f][i].width = std::max(mImageArray[f][0].width >> i, 1);
            mImageArray[f][i].height = mImageArray[f][i].width;
            mImageArray[f][i].format = mImageArray[f][0].format;
            mImageArray[f][i].type = mImageArray[f][0].type;
        }
    }

    if (mIsRenderable)
    {
        if (mTexture == NULL)
        {
            return;
        }

        for (unsigned int f = 0; f < 6; f++)
        {
            for (unsigned int i = 1; i <= q; i++)
            {
                IDirect3DSurface9 *upper = getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, i-1);
                IDirect3DSurface9 *lower = getCubeMapSurface(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, i);

                if (upper != NULL && lower != NULL)
                {
                    getBlitter()->boxFilter(upper, lower);
                }

                if (upper != NULL) upper->Release();
                if (lower != NULL) lower->Release();

                mImageArray[f][i].dirty = false;
            }
        }
    }
    else
    {
        for (unsigned int f = 0; f < 6; f++)
        {
            for (unsigned int i = 1; i <= q; i++)
            {
                createSurface(&mImageArray[f][i]);
                if (mImageArray[f][i].surface == NULL)
                {
                    return error(GL_OUT_OF_MEMORY);
                }

                if (FAILED(D3DXLoadSurfaceFromSurface(mImageArray[f][i].surface, NULL, NULL, mImageArray[f][i - 1].surface, NULL, NULL, D3DX_FILTER_BOX, 0)))
                {
                    ERR(" failed to load filter %d to %d.", i - 1, i);
                }

                mImageArray[f][i].dirty = true;
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
        mFaceProxies[face].set(new Renderbuffer(id(), new Colorbuffer(this, target)));
    }

    return mFaceProxies[face].get();
}

IDirect3DSurface9 *TextureCubeMap::getRenderTarget(GLenum target)
{
    ASSERT(IsCubemapTextureTarget(target));

    if (!mIsRenderable)
    {
        convertToRenderTarget();
    }

    if (mTexture == NULL)
    {
        return NULL;
    }

    updateTexture();
    
    IDirect3DSurface9 *renderTarget = NULL;
    mTexture->GetCubeMapSurface(es2dx::ConvertCubeFace(target), 0, &renderTarget);

    return renderTarget;
}

}
