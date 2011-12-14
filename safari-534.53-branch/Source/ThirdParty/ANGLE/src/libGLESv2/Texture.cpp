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

#include "common/debug.h"

#include "libGLESv2/main.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"
#include "libGLESv2/Blit.h"

namespace gl
{

Texture::Image::Image()
  : width(0), height(0), dirty(false), surface(NULL), format(GL_NONE)
{
}

Texture::Image::~Image()
{
  if (surface) surface->Release();
}

Texture::Texture(GLuint id) : RefCountObject(id)
{
    mMinFilter = GL_NEAREST_MIPMAP_LINEAR;
    mMagFilter = GL_LINEAR;
    mWrapS = GL_REPEAT;
    mWrapT = GL_REPEAT;

    mWidth = 0;
    mHeight = 0;

    mDirtyMetaData = true;
    mDirty = true;
    mIsRenderable = false;
    mType = GL_UNSIGNED_BYTE;
    mBaseTexture = NULL;
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
                mDirty = true;
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
                mDirty = true;
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
                mDirty = true;
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
                mDirty = true;
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

GLuint Texture::getWidth() const
{
    return mWidth;
}

GLuint Texture::getHeight() const
{
    return mHeight;
}

bool Texture::isFloatingPoint() const
{
    return (mType == GL_FLOAT || mType == GL_HALF_FLOAT_OES);
}

bool Texture::isRenderableFormat() const
{
    D3DFORMAT format = getD3DFormat();
    
    switch(format)
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

// Selects an internal Direct3D 9 format for storing an Image
D3DFORMAT Texture::selectFormat(GLenum format, GLenum type)
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

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the target pixel rectangle at output with outputPitch bytes in between each line.
void Texture::loadImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
                            GLint unpackAlignment, const void *input, size_t outputPitch, void *output, D3DSURFACE_DESC *description) const
{
    GLsizei inputPitch = ComputePitch(width, format, type, unpackAlignment);

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
            loadRGBAUByteImageData(xoffset, yoffset, width, height, inputPitch, input, outputPitch, output);
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
                                 size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                      size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                          size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                     size_t inputPitch, const void *input, size_t outputPitch, void *output, bool native) const
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
                                          size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                                   size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                          size_t inputPitch, const void *input, size_t outputPitch, void *output, bool native) const
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
                                               size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                                   size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                    size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                  size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                    size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                        size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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

void Texture::loadRGBAUByteImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     size_t inputPitch, const void *input, size_t outputPitch, void *output) const
{
    const unsigned char *source = NULL;
    unsigned char *dest = NULL;

    for (int y = 0; y < height; y++)
    {
        source = static_cast<const unsigned char*>(input) + y * inputPitch;
        dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;
        for (int x = 0; x < width; x++)
        {
            dest[4 * x + 0] = source[x * 4 + 2];
            dest[4 * x + 1] = source[x * 4 + 1];
            dest[4 * x + 2] = source[x * 4 + 0];
            dest[4 * x + 3] = source[x * 4 + 3];
        }
    }
}

void Texture::loadRGBA4444ImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                    size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                    size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                     size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                        size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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
                                size_t inputPitch, const void *input, size_t outputPitch, void *output) const
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

void Texture::createSurface(GLsizei width, GLsizei height, GLenum format, GLenum type, Image *img)
{
    IDirect3DTexture9 *newTexture = NULL;
    IDirect3DSurface9 *newSurface = NULL;

    if (width != 0 && height != 0)
    {
        int levelToFetch = 0;
        GLsizei requestWidth = width;
        GLsizei requestHeight = height;
        if (IsCompressed(format) && (width % 4 != 0 || height % 4 != 0))
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

        HRESULT result = getDevice()->CreateTexture(requestWidth, requestHeight, levelToFetch + 1, NULL, selectFormat(format, type),
                                                    D3DPOOL_SYSTEMMEM, &newTexture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY);
        }

        newTexture->GetSurfaceLevel(levelToFetch, &newSurface);
        newTexture->Release();
    }

    if (img->surface) img->surface->Release();
    img->surface = newSurface;

    img->width = width;
    img->height = height;
    img->format = format;
}

void Texture::setImage(GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *img)
{
    createSurface(width, height, format, type, img);

    if (pixels != NULL && img->surface != NULL)
    {
        D3DSURFACE_DESC description;
        img->surface->GetDesc(&description);

        D3DLOCKED_RECT locked;
        HRESULT result = img->surface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            loadImageData(0, 0, width, height, format, type, unpackAlignment, pixels, locked.Pitch, locked.pBits, &description);
            img->surface->UnlockRect();
        }

        img->dirty = true;
    }

    mDirtyMetaData = true;
}

void Texture::setCompressedImage(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *img)
{
    createSurface(width, height, format, GL_UNSIGNED_BYTE, img);

    if (pixels != NULL && img->surface != NULL)
    {
        D3DLOCKED_RECT locked;
        HRESULT result = img->surface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            memcpy(locked.pBits, pixels, imageSize);
            img->surface->UnlockRect();
        }

        img->dirty = true;
    }

    mDirtyMetaData = true;
}

bool Texture::subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *img)
{
    if (width + xoffset > img->width || height + yoffset > img->height)
    {
        error(GL_INVALID_VALUE);
        return false;
    }

    if (!img->surface)
    {
        createSurface(img->width, img->height, format, type, img);
    }

    if (pixels != NULL && img->surface != NULL)
    {
        D3DSURFACE_DESC description;
        img->surface->GetDesc(&description);

        D3DLOCKED_RECT locked;
        HRESULT result = img->surface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            loadImageData(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, locked.Pitch, locked.pBits, &description);
            img->surface->UnlockRect();
        }

        img->dirty = true;
    }

    return true;
}

bool Texture::subImageCompressed(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels, Image *img)
{
    if (width + xoffset > img->width || height + yoffset > img->height)
    {
        error(GL_INVALID_VALUE);
        return false;
    }

    if (format != getFormat())
    {
        error(GL_INVALID_OPERATION);
        return false;
    }

    if (!img->surface)
    {
        createSurface(img->width, img->height, format, GL_UNSIGNED_BYTE, img);
    }

    if (pixels != NULL && img->surface != NULL)
    {
        RECT updateRegion;
        updateRegion.left = xoffset;
        updateRegion.right = xoffset + width;
        updateRegion.bottom = yoffset + height;
        updateRegion.top = yoffset;

        D3DLOCKED_RECT locked;
        HRESULT result = img->surface->LockRect(&locked, &updateRegion, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            GLsizei inputPitch = ComputeCompressedPitch(width, format);
            int rows = imageSize / inputPitch;
            for (int i = 0; i < rows; ++i)
            {
                memcpy((void*)((BYTE*)locked.pBits + i * locked.Pitch), (void*)((BYTE*)pixels + i * inputPitch), inputPitch);
            }
            img->surface->UnlockRect();
        }

        img->dirty = true;
    }

    return true;
}

// This implements glCopyTex[Sub]Image2D for non-renderable internal texture formats
void Texture::copyNonRenderable(Image *image, GLenum internalFormat, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, IDirect3DSurface9 *renderTarget)
{
    IDirect3DDevice9 *device = getDevice();
    IDirect3DSurface9 *surface = NULL;
    D3DSURFACE_DESC description;
    renderTarget->GetDesc(&description);
    
    HRESULT result = device->CreateOffscreenPlainSurface(description.Width, description.Height, description.Format, D3DPOOL_SYSTEMMEM, &surface, NULL);

    if (!SUCCEEDED(result))
    {
        ERR("Could not create matching destination surface.");
        return error(GL_OUT_OF_MEMORY);
    }

    result = device->GetRenderTargetData(renderTarget, surface);

    if (!SUCCEEDED(result))
    {
        ERR("GetRenderTargetData unexpectedly failed.");
        surface->Release();
        return error(GL_OUT_OF_MEMORY);
    }

    D3DLOCKED_RECT sourceLock = {0};
    RECT sourceRect = {x, y, x + width, y + height};
    result = surface->LockRect(&sourceLock, &sourceRect, 0);

    if (FAILED(result))
    {
        ERR("Failed to lock the source surface (rectangle might be invalid).");
        surface->UnlockRect();
        surface->Release();
        return error(GL_OUT_OF_MEMORY);
    }

    if (!image->surface)
    {
        createSurface(width, height, internalFormat, mType, image);
    }

    if (image->surface == NULL)
    {
        ERR("Failed to create an image surface.");
        surface->UnlockRect();
        surface->Release();
        return error(GL_OUT_OF_MEMORY); 
    }

    D3DLOCKED_RECT destLock = {0};
    RECT destRect = {xoffset, yoffset, xoffset + width, yoffset + height};
    result = image->surface->LockRect(&destLock, &destRect, 0);
    
    if (FAILED(result))
    {
        ERR("Failed to lock the destination surface (rectangle might be invalid).");
        surface->UnlockRect();
        surface->Release();
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

        image->dirty = true;
        mDirtyMetaData = true;    
    }

    image->surface->UnlockRect();
    surface->UnlockRect();
    surface->Release();
}

D3DFORMAT Texture::getD3DFormat() const
{
    return selectFormat(getFormat(), mType);
}

IDirect3DBaseTexture9 *Texture::getTexture()
{
    if (!isComplete())
    {
        return NULL;
    }

    if (mDirtyMetaData)
    {
        mBaseTexture = createTexture();
        mIsRenderable = false;
    }

    if (mDirtyMetaData || dirtyImageData())
    {
        updateTexture();
    }

    mDirtyMetaData = false;
    ASSERT(!dirtyImageData());

    return mBaseTexture;
}

bool Texture::isDirty() const
{
    return (mDirty || mDirtyMetaData || dirtyImageData());
}

// Returns the top-level texture surface as a render target
void Texture::needRenderTarget()
{
    if (!mIsRenderable)
    {
        mBaseTexture = convertToRenderTarget();
        mIsRenderable = true;
    }

    if (dirtyImageData())
    {
        updateTexture();
    }

    mDirtyMetaData = false;
}

void Texture::dropTexture()
{
    if (mBaseTexture)
    {
        mBaseTexture = NULL;
    }

    mIsRenderable = false;
}

void Texture::pushTexture(IDirect3DBaseTexture9 *newTexture, bool renderable)
{
    mBaseTexture = newTexture;
    mDirtyMetaData = false;
    mIsRenderable = renderable;
    mDirty = true;
}


GLint Texture::creationLevels(GLsizei width, GLsizei height, GLint maxlevel) const
{
    if (isPow2(width) && isPow2(height))
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
    return mBaseTexture ? mBaseTexture->GetLevelCount() : 0;
}

bool Texture::isRenderable() const
{
    return mIsRenderable;
}

Texture2D::Texture2D(GLuint id) : Texture(id)
{
    mTexture = NULL;
}

Texture2D::~Texture2D()
{
    mColorbufferProxy.set(NULL);

    if (mTexture)
    {
        mTexture->Release();
        mTexture = NULL;
    }
}

GLenum Texture2D::getTarget() const
{
    return GL_TEXTURE_2D;
}

GLenum Texture2D::getFormat() const
{
    return mImageArray[0].format;
}

// While OpenGL doesn't check texture consistency until draw-time, D3D9 requires a complete texture
// for render-to-texture (such as CopyTexImage). We have no way of keeping individual inconsistent levels.
// Call this when a particular level of the texture must be defined with a specific format, width and height.
//
// Returns true if the existing texture was unsuitable and had to be destroyed. If so, it will also set
// a new height and width for the texture by working backwards from the given width and height.
bool Texture2D::redefineTexture(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum type)
{
    bool widthOkay = (mWidth >> level == width);
    bool heightOkay = (mHeight >> level == height);

    bool sizeOkay = ((widthOkay && heightOkay)
                     || (widthOkay && mHeight >> level == 0 && height == 1)
                     || (heightOkay && mWidth >> level == 0 && width == 1));

    bool typeOkay = (type == mType);

    bool textureOkay = (sizeOkay && typeOkay && internalFormat == mImageArray[0].format);

    if (!textureOkay)
    {
        TRACE("Redefining 2D texture (%d, 0x%04X, %d, %d => 0x%04X, %d, %d).", level,
              mImageArray[0].format, mWidth, mHeight,
              internalFormat, width, height);

        // Purge all the levels and the texture.

        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            if (mImageArray[i].surface != NULL)
            {
                mImageArray[i].dirty = false;

                mImageArray[i].surface->Release();
                mImageArray[i].surface = NULL;
            }
        }

        if (mTexture != NULL)
        {
            mTexture->Release();
            mTexture = NULL;
            dropTexture();
        }

        mWidth = width << level;
        mHeight = height << level;
        mImageArray[0].format = internalFormat;
        mType = type;
    }

    return !textureOkay;
}

void Texture2D::setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineTexture(level, internalFormat, width, height, type);

    Texture::setImage(width, height, format, type, unpackAlignment, pixels, &mImageArray[level]);
}

void Texture2D::setCompressedImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels)
{
    redefineTexture(level, internalFormat, width, height, GL_UNSIGNED_BYTE);

    Texture::setCompressedImage(width, height, internalFormat, imageSize, pixels, &mImageArray[level]);
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
            Image *img = &mImageArray[level];

            RECT sourceRect;
            sourceRect.left = xoffset;
            sourceRect.top = yoffset;
            sourceRect.right = xoffset + width;
            sourceRect.bottom = yoffset + height;

            POINT destPoint;
            destPoint.x = xoffset;
            destPoint.y = yoffset;

            result = getDevice()->UpdateSurface(img->surface, &sourceRect, destLevel, &destPoint);
            ASSERT(SUCCEEDED(result));

            destLevel->Release();

            img->dirty = false;
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

void Texture2D::copyImage(GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source)
{
    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    bool redefined = redefineTexture(level, internalFormat, width, height, mType);
   
    if (!isRenderableFormat())
    {
        copyNonRenderable(&mImageArray[level], internalFormat, 0, 0, x, y, width, height, renderTarget);
    }
    else
    {
        if (redefined)
        {
            convertToRenderTarget();
            pushTexture(mTexture, true);
        }
        else
        {
            needRenderTarget();
        }

        if (width != 0 && height != 0 && level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;

            IDirect3DSurface9 *dest;
            HRESULT hr = mTexture->GetSurfaceLevel(level, &dest);

            getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, internalFormat, 0, 0, dest);
            dest->Release();
        }
    }

    mImageArray[level].width = width;
    mImageArray[level].height = height;
    mImageArray[level].format = internalFormat;
}

void Texture2D::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source)
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

    bool redefined = redefineTexture(0, mImageArray[0].format, mImageArray[0].width, mImageArray[0].height, mType);
   
    if (!isRenderableFormat())
    {
        copyNonRenderable(&mImageArray[level], getFormat(), xoffset, yoffset, x, y, width, height, renderTarget);
    }
    else
    {
        if (redefined)
        {
            convertToRenderTarget();
            pushTexture(mTexture, true);
        }
        else
        {
            needRenderTarget();
        }

        if (level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;

            IDirect3DSurface9 *dest;
            HRESULT hr = mTexture->GetSurfaceLevel(level, &dest);

            getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, mImageArray[0].format, xoffset, yoffset, dest);
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

    if ((getFormat() == GL_FLOAT && !getContext()->supportsFloatLinearFilter()) ||
        (getFormat() == GL_HALF_FLOAT_OES && !getContext()->supportsHalfFloatLinearFilter()))
    {
        if (mMagFilter != GL_NEAREST || (mMinFilter != GL_NEAREST && mMinFilter != GL_NEAREST_MIPMAP_NEAREST))
        {
            return false;
        }
    }


    if ((getWrapS() != GL_CLAMP_TO_EDGE && !isPow2(width))
        || (getWrapT() != GL_CLAMP_TO_EDGE && !isPow2(height)))
    {
        return false;
    }

    if (mipmapping)
    {
        if (!isPow2(width) || !isPow2(height))
        {
            return false;
        }

        int q = log2(std::max(width, height));

        for (int level = 1; level <= q; level++)
        {
            if (mImageArray[level].format != mImageArray[0].format)
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
    return IsCompressed(getFormat());
}

// Constructs a Direct3D 9 texture resource from the texture images, or returns an existing one
IDirect3DBaseTexture9 *Texture2D::createTexture()
{
    IDirect3DTexture9 *texture;

    IDirect3DDevice9 *device = getDevice();
    D3DFORMAT format = selectFormat(mImageArray[0].format, mType);

    HRESULT result = device->CreateTexture(mWidth, mHeight, creationLevels(mWidth, mHeight, 0), 0, format, D3DPOOL_DEFAULT, &texture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
    }

    if (mTexture) mTexture->Release();
    mTexture = texture;
    return texture;
}

void Texture2D::updateTexture()
{
    IDirect3DDevice9 *device = getDevice();

    int levels = levelCount();

    for (int level = 0; level < levels; level++)
    {
        if (mImageArray[level].dirty)
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

IDirect3DBaseTexture9 *Texture2D::convertToRenderTarget()
{
    IDirect3DTexture9 *texture = NULL;

    if (mWidth != 0 && mHeight != 0)
    {
        egl::Display *display = getDisplay();
        IDirect3DDevice9 *device = getDevice();
        D3DFORMAT format = selectFormat(mImageArray[0].format, mType);

        HRESULT result = device->CreateTexture(mWidth, mHeight, creationLevels(mWidth, mHeight, 0), D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
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

                    return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                }

                IDirect3DSurface9 *dest;
                result = texture->GetSurfaceLevel(i, &dest);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();
                    source->Release();

                    return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                }

                display->endScene();
                result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();
                    source->Release();
                    dest->Release();

                    return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
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
    return mTexture;
}

bool Texture2D::dirtyImageData() const
{
    int q = log2(std::max(mWidth, mHeight));

    for (int i = 0; i <= q; i++)
    {
        if (mImageArray[i].dirty) return true;
    }

    return false;
}

void Texture2D::generateMipmaps()
{
    if (!isPow2(mImageArray[0].width) || !isPow2(mImageArray[0].height))
    {
        return error(GL_INVALID_OPERATION);
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(std::max(mWidth, mHeight));
    for (unsigned int i = 1; i <= q; i++)
    {
        if (mImageArray[i].surface != NULL)
        {
            mImageArray[i].surface->Release();
            mImageArray[i].surface = NULL;
        }

        mImageArray[i].dirty = false;

        mImageArray[i].format = mImageArray[0].format;
        mImageArray[i].width = std::max(mImageArray[0].width >> i, 1);
        mImageArray[i].height = std::max(mImageArray[0].height >> i, 1);
    }

    if (isRenderable())
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
        }
    }
    else
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            createSurface(mImageArray[i].width, mImageArray[i].height, mImageArray[i].format, mType, &mImageArray[i]);
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

        mDirtyMetaData = true;
    }
}

Renderbuffer *Texture2D::getColorbuffer(GLenum target)
{
    if (target != GL_TEXTURE_2D)
    {
        return error(GL_INVALID_OPERATION, (Renderbuffer *)NULL);
    }

    if (mColorbufferProxy.get() == NULL)
    {
        mColorbufferProxy.set(new Renderbuffer(id(), new TextureColorbufferProxy(this, target)));
    }

    return mColorbufferProxy.get();
}

IDirect3DSurface9 *Texture2D::getRenderTarget(GLenum target)
{
    ASSERT(target == GL_TEXTURE_2D);

    needRenderTarget();

    if (mTexture == NULL)
    {
        return NULL;
    }
    
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

GLenum TextureCubeMap::getFormat() const
{
    return mImageArray[0][0].format;
}

void TextureCubeMap::setImagePosX(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(0, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegX(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(1, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImagePosY(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(2, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegY(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(3, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImagePosZ(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(4, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegZ(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(5, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setCompressedImage(GLenum face, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei imageSize, const void *pixels)
{
    redefineTexture(level, internalFormat, width);

    Texture::setCompressedImage(width, height, internalFormat, imageSize, pixels, &mImageArray[faceIndex(face)][level]);
}

void TextureCubeMap::commitRect(GLenum faceTarget, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    int face = faceIndex(faceTarget);

    ASSERT(mImageArray[face][level].surface != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = getCubeMapSurface(face, level);
        ASSERT(destLevel != NULL);

        if (destLevel != NULL)
        {
            Image *img = &mImageArray[face][level];

            RECT sourceRect;
            sourceRect.left = xoffset;
            sourceRect.top = yoffset;
            sourceRect.right = xoffset + width;
            sourceRect.bottom = yoffset + height;

            POINT destPoint;
            destPoint.x = xoffset;
            destPoint.y = yoffset;

            HRESULT result = getDevice()->UpdateSurface(img->surface, &sourceRect, destLevel, &destPoint);
            ASSERT(SUCCEEDED(result));

            destLevel->Release();

            img->dirty = false;
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

    if ((getFormat() == GL_FLOAT && !getContext()->supportsFloatLinearFilter()) ||
        (getFormat() == GL_HALF_FLOAT_OES && !getContext()->supportsHalfFloatLinearFilter()))
    {
        if (mMagFilter != GL_NEAREST || (mMinFilter != GL_NEAREST && mMinFilter != GL_NEAREST_MIPMAP_NEAREST))
        {
            return false;
        }
    }

    if (mipmapping)
    {
        if (!isPow2(size) && (getWrapS() != GL_CLAMP_TO_EDGE || getWrapT() != GL_CLAMP_TO_EDGE))
        {
            return false;
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
    return IsCompressed(getFormat());
}

// Constructs a Direct3D 9 texture resource from the texture images, or returns an existing one
IDirect3DBaseTexture9 *TextureCubeMap::createTexture()
{
    IDirect3DDevice9 *device = getDevice();
    D3DFORMAT format = selectFormat(mImageArray[0][0].format, mType);

    IDirect3DCubeTexture9 *texture;

    HRESULT result = device->CreateCubeTexture(mWidth, creationLevels(mWidth, 0), 0, format, D3DPOOL_DEFAULT, &texture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
    }

    if (mTexture) mTexture->Release();

    mTexture = texture;
    return mTexture;
}

void TextureCubeMap::updateTexture()
{
    IDirect3DDevice9 *device = getDevice();

    for (int face = 0; face < 6; face++)
    {
        int levels = levelCount();
        for (int level = 0; level < levels; level++)
        {
            Image *img = &mImageArray[face][level];

            if (img->dirty)
            {
                IDirect3DSurface9 *levelSurface = getCubeMapSurface(face, level);
                ASSERT(levelSurface != NULL);

                if (levelSurface != NULL)
                {
                    HRESULT result = device->UpdateSurface(img->surface, NULL, levelSurface, NULL);
                    ASSERT(SUCCEEDED(result));

                    levelSurface->Release();

                    img->dirty = false;
                }
            }
        }
    }
}

IDirect3DBaseTexture9 *TextureCubeMap::convertToRenderTarget()
{
    IDirect3DCubeTexture9 *texture = NULL;

    if (mWidth != 0)
    {
        egl::Display *display = getDisplay();
        IDirect3DDevice9 *device = getDevice();
        D3DFORMAT format = selectFormat(mImageArray[0][0].format, mType);

        HRESULT result = device->CreateCubeTexture(mWidth, creationLevels(mWidth, 0), D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
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

                        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                    }

                    IDirect3DSurface9 *dest;
                    result = texture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(f), i, &dest);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();
                        source->Release();

                        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                    }

                    display->endScene();
                    result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();
                        source->Release();
                        dest->Release();

                        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
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
    return mTexture;
}

void TextureCubeMap::setImage(int face, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineTexture(level, internalFormat, width);

    Texture::setImage(width, height, format, type, unpackAlignment, pixels, &mImageArray[face][level]);
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

bool TextureCubeMap::dirtyImageData() const
{
    int q = log2(mWidth);

    for (int f = 0; f < 6; f++)
    {
        for (int i = 0; i <= q; i++)
        {
            if (mImageArray[f][i].dirty) return true;
        }
    }

    return false;
}

// While OpenGL doesn't check texture consistency until draw-time, D3D9 requires a complete texture
// for render-to-texture (such as CopyTexImage). We have no way of keeping individual inconsistent levels & faces.
// Call this when a particular level of the texture must be defined with a specific format, width and height.
//
// Returns true if the existing texture was unsuitable had to be destroyed. If so, it will also set
// a new size for the texture by working backwards from the given size.
bool TextureCubeMap::redefineTexture(GLint level, GLenum internalFormat, GLsizei width)
{
    // Are these settings compatible with level 0?
    bool sizeOkay = (mImageArray[0][0].width >> level == width);

    bool textureOkay = (sizeOkay && internalFormat == mImageArray[0][0].format);

    if (!textureOkay)
    {
        TRACE("Redefining cube texture (%d, 0x%04X, %d => 0x%04X, %d).", level,
              mImageArray[0][0].format, mImageArray[0][0].width,
              internalFormat, width);

        // Purge all the levels and the texture.
        for (int i = 0; i < IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
        {
            for (int f = 0; f < 6; f++)
            {
                if (mImageArray[f][i].surface != NULL)
                {
                    mImageArray[f][i].dirty = false;

                    mImageArray[f][i].surface->Release();
                    mImageArray[f][i].surface = NULL;
                }
            }
        }

        if (mTexture != NULL)
        {
            mTexture->Release();
            mTexture = NULL;
            dropTexture();
        }

        mWidth = width << level;
        mImageArray[0][0].width = width << level;
        mHeight = width << level;
        mImageArray[0][0].height = width << level;

        mImageArray[0][0].format = internalFormat;
    }

    return !textureOkay;
}

void TextureCubeMap::copyImage(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source)
{
    IDirect3DSurface9 *renderTarget = source->getRenderTarget();

    if (!renderTarget)
    {
        ERR("Failed to retrieve the render target.");
        return error(GL_OUT_OF_MEMORY);
    }

    unsigned int faceindex = faceIndex(target);
    bool redefined = redefineTexture(level, internalFormat, width);

    if (!isRenderableFormat())
    {
        copyNonRenderable(&mImageArray[faceindex][level], internalFormat, 0, 0, x, y, width, height, renderTarget);
    }
    else
    {
        if (redefined)
        {
            convertToRenderTarget();
            pushTexture(mTexture, true);
        }
        else
        {
            needRenderTarget();
        }

        ASSERT(width == height);

        if (width > 0 && level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;

            IDirect3DSurface9 *dest = getCubeMapSurface(target, level);

            getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, internalFormat, 0, 0, dest);
            dest->Release();
        }
    }

    mImageArray[faceindex][level].width = width;
    mImageArray[faceindex][level].height = height;
    mImageArray[faceindex][level].format = internalFormat;
}

IDirect3DSurface9 *TextureCubeMap::getCubeMapSurface(unsigned int faceIdentifier, unsigned int level)
{
    unsigned int faceIndex;

    if (faceIdentifier < 6)
    {
        faceIndex = faceIdentifier;
    }
    else if (faceIdentifier >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && faceIdentifier <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
    {
        faceIndex = faceIdentifier - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }
    else
    {
        UNREACHABLE();
        faceIndex = 0;
    }

    if (mTexture == NULL)
    {
        UNREACHABLE();
        return NULL;
    }

    IDirect3DSurface9 *surface = NULL;

    HRESULT hr = mTexture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(faceIndex), level, &surface);

    return (SUCCEEDED(hr)) ? surface : NULL;
}

void TextureCubeMap::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, RenderbufferStorage *source)
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
    bool redefined = redefineTexture(0, mImageArray[0][0].format, mImageArray[0][0].width);
   
    if (!isRenderableFormat())
    {
        copyNonRenderable(&mImageArray[faceindex][level], getFormat(), 0, 0, x, y, width, height, renderTarget);
    }
    else
    {
        if (redefined)
        {
            convertToRenderTarget();
            pushTexture(mTexture, true);
        }
        else
        {
            needRenderTarget();
        }

        if (level < levelCount())
        {
            RECT sourceRect;
            sourceRect.left = x;
            sourceRect.right = x + width;
            sourceRect.top = y;
            sourceRect.bottom = y + height;

            IDirect3DSurface9 *dest = getCubeMapSurface(target, level);

            getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, mImageArray[0][0].format, xoffset, yoffset, dest);
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
    if (!isPow2(mImageArray[0][0].width) || !isCubeComplete())
    {
        return error(GL_INVALID_OPERATION);
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

            mImageArray[f][i].dirty = false;

            mImageArray[f][i].format = mImageArray[f][0].format;
            mImageArray[f][i].width = std::max(mImageArray[f][0].width >> i, 1);
            mImageArray[f][i].height = mImageArray[f][i].width;
        }
    }

    if (isRenderable())
    {
        if (mTexture == NULL)
        {
            return;
        }

        for (unsigned int f = 0; f < 6; f++)
        {
            for (unsigned int i = 1; i <= q; i++)
            {
                IDirect3DSurface9 *upper = getCubeMapSurface(f, i-1);
                IDirect3DSurface9 *lower = getCubeMapSurface(f, i);

                if (upper != NULL && lower != NULL)
                {
                    getBlitter()->boxFilter(upper, lower);
                }

                if (upper != NULL) upper->Release();
                if (lower != NULL) lower->Release();
            }
        }
    }
    else
    {
        for (unsigned int f = 0; f < 6; f++)
        {
            for (unsigned int i = 1; i <= q; i++)
            {
                createSurface(mImageArray[f][i].width, mImageArray[f][i].height, mImageArray[f][i].format, mType, &mImageArray[f][i]);
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

        mDirtyMetaData = true;
    }
}

Renderbuffer *TextureCubeMap::getColorbuffer(GLenum target)
{
    if (!IsCubemapTextureTarget(target))
    {
        return error(GL_INVALID_OPERATION, (Renderbuffer *)NULL);
    }

    unsigned int face = faceIndex(target);

    if (mFaceProxies[face].get() == NULL)
    {
        mFaceProxies[face].set(new Renderbuffer(id(), new TextureColorbufferProxy(this, target)));
    }

    return mFaceProxies[face].get();
}

IDirect3DSurface9 *TextureCubeMap::getRenderTarget(GLenum target)
{
    ASSERT(IsCubemapTextureTarget(target));

    needRenderTarget();
    
    if (mTexture == NULL)
    {
        return NULL;
    }
    
    IDirect3DSurface9 *renderTarget = NULL;
    mTexture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(faceIndex(target)), 0, &renderTarget);

    return renderTarget;
}

Texture::TextureColorbufferProxy::TextureColorbufferProxy(Texture *texture, GLenum target)
  : Colorbuffer(texture), mTexture(texture), mTarget(target)
{
    ASSERT(IsTextureTarget(target));
}

void Texture::TextureColorbufferProxy::addRef() const
{
    mTexture->addRef();
}

void Texture::TextureColorbufferProxy::release() const
{
    mTexture->release();
}

IDirect3DSurface9 *Texture::TextureColorbufferProxy::getRenderTarget()
{
    if (mRenderTarget) mRenderTarget->Release();

    mRenderTarget = mTexture->getRenderTarget(mTarget);

    return mRenderTarget;
}

int Texture::TextureColorbufferProxy::getWidth() const
{
    return mTexture->getWidth();
}

int Texture::TextureColorbufferProxy::getHeight() const
{
    return mTexture->getHeight();
}

GLenum Texture::TextureColorbufferProxy::getFormat() const
{
    return mTexture->getFormat();
}

bool Texture::TextureColorbufferProxy::isFloatingPoint() const
{
    return mTexture->isFloatingPoint();
}

}
