//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.cpp: Conversion functions and other utility routines.

#include "libGLESv2/utilities.h"

#include <limits>
#include <stdio.h>

#include "common/debug.h"
#include "common/system.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/Context.h"

namespace gl
{

// This is how much data the application expects for a uniform
int UniformExternalComponentCount(GLenum type)
{
    switch (type)
    {
      case GL_BOOL:
      case GL_FLOAT:
      case GL_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_CUBE:
          return 1;
      case GL_BOOL_VEC2:
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
          return 2;
      case GL_INT_VEC3:
      case GL_FLOAT_VEC3:
      case GL_BOOL_VEC3:
          return 3;
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_FLOAT_MAT2:
          return 4;
      case GL_FLOAT_MAT3:
          return 9;
      case GL_FLOAT_MAT4:
          return 16;
      default:
          UNREACHABLE();
    }

    return 0;
}

// This is how much data we actually store for a uniform
int UniformInternalComponentCount(GLenum type)
{
    switch (type)
    {
      case GL_BOOL:
      case GL_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_CUBE:
          return 1;
      case GL_BOOL_VEC2:
      case GL_INT_VEC2:
          return 2;
      case GL_INT_VEC3:
      case GL_BOOL_VEC3:
          return 3;
      case GL_FLOAT:
      case GL_FLOAT_VEC2:
      case GL_FLOAT_VEC3:
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
          return 4;
      case GL_FLOAT_MAT2:
          return 8;
      case GL_FLOAT_MAT3:
          return 12;
      case GL_FLOAT_MAT4:
          return 16;
      default:
          UNREACHABLE();
    }

    return 0;
}

GLenum UniformComponentType(GLenum type)
{
    switch(type)
    {
      case GL_BOOL:
      case GL_BOOL_VEC2:
      case GL_BOOL_VEC3:
      case GL_BOOL_VEC4:
          return GL_BOOL;
      case GL_FLOAT:
      case GL_FLOAT_VEC2:
      case GL_FLOAT_VEC3:
      case GL_FLOAT_VEC4:
      case GL_FLOAT_MAT2:
      case GL_FLOAT_MAT3:
      case GL_FLOAT_MAT4:
          return GL_FLOAT;
      case GL_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_CUBE:
      case GL_INT_VEC2:
      case GL_INT_VEC3:
      case GL_INT_VEC4:
          return GL_INT;
      default:
          UNREACHABLE();
    }

    return GL_NONE;
}

size_t UniformComponentSize(GLenum type)
{
    switch(type)
    {
      case GL_BOOL:  return sizeof(GLint);
      case GL_FLOAT: return sizeof(GLfloat);
      case GL_INT:   return sizeof(GLint);
      default:       UNREACHABLE();
    }

    return 0;
}

size_t UniformInternalSize(GLenum type)
{
    return UniformComponentSize(UniformComponentType(type)) * UniformInternalComponentCount(type);
}

size_t UniformExternalSize(GLenum type)
{
    return UniformComponentSize(UniformComponentType(type)) * UniformExternalComponentCount(type);
}

int VariableRowCount(GLenum type)
{
    switch (type)
    {
      case GL_NONE:
        return 0;
      case GL_BOOL:
      case GL_FLOAT:
      case GL_INT:
      case GL_BOOL_VEC2:
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_INT_VEC3:
      case GL_FLOAT_VEC3:
      case GL_BOOL_VEC3:
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
        return 1;
      case GL_FLOAT_MAT2:
        return 2;
      case GL_FLOAT_MAT3:
        return 3;
      case GL_FLOAT_MAT4:
        return 4;
      default:
        UNREACHABLE();
    }

    return 0;
}

int VariableColumnCount(GLenum type)
{
    switch (type)
    {
      case GL_NONE:
        return 0;
      case GL_BOOL:
      case GL_FLOAT:
      case GL_INT:
        return 1;
      case GL_BOOL_VEC2:
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_FLOAT_MAT2:
        return 2;
      case GL_INT_VEC3:
      case GL_FLOAT_VEC3:
      case GL_BOOL_VEC3:
      case GL_FLOAT_MAT3:
        return 3;
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_FLOAT_MAT4:
        return 4;
      default:
        UNREACHABLE();
    }

    return 0;
}

int AllocateFirstFreeBits(unsigned int *bits, unsigned int allocationSize, unsigned int bitsSize)
{
    ASSERT(allocationSize <= bitsSize);

    unsigned int mask = std::numeric_limits<unsigned int>::max() >> (std::numeric_limits<unsigned int>::digits - allocationSize);

    for (unsigned int i = 0; i < bitsSize - allocationSize + 1; i++)
    {
        if ((*bits & mask) == 0)
        {
            *bits |= mask;
            return i;
        }

        mask <<= 1;
    }

    return -1;
}

GLsizei ComputePitch(GLsizei width, GLint internalformat, GLint alignment)
{
    ASSERT(alignment > 0 && isPow2(alignment));

    GLsizei rawPitch = ComputePixelSize(internalformat) * width;
    return (rawPitch + alignment - 1) & ~(alignment - 1);
}

GLsizei ComputeCompressedPitch(GLsizei width, GLenum internalformat)
{
    return ComputeCompressedSize(width, 1, internalformat);
}

GLsizei ComputeCompressedSize(GLsizei width, GLsizei height, GLenum internalformat)
{
    switch (internalformat)
    {
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return 8 * ((width + 3) / 4) * ((height + 3) / 4);
      case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
      case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
        return 16 * ((width + 3) / 4) * ((height + 3) / 4);
      default:
        return 0;
    }
}

bool IsCompressed(GLenum format)
{
    if(format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
       format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
       format == GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE ||
       format == GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool IsDepthTexture(GLenum format)
{
    if (format == GL_DEPTH_COMPONENT ||
        format == GL_DEPTH_STENCIL_OES ||
        format == GL_DEPTH_COMPONENT16 ||
        format == GL_DEPTH_COMPONENT32_OES ||
        format == GL_DEPTH24_STENCIL8_OES)
    {
        return true;
    }

    return false;
}

bool IsStencilTexture(GLenum format)
{
    if (format == GL_DEPTH_STENCIL_OES ||
        format == GL_DEPTH24_STENCIL8_OES)
    {
        return true;
    }

    return false;
}

// Returns the size, in bytes, of a single texel in an Image
int ComputePixelSize(GLint internalformat)
{
    switch (internalformat)
    {
      case GL_ALPHA8_EXT:                       return sizeof(unsigned char);
      case GL_LUMINANCE8_EXT:                   return sizeof(unsigned char);
      case GL_ALPHA32F_EXT:                     return sizeof(float);
      case GL_LUMINANCE32F_EXT:                 return sizeof(float);
      case GL_ALPHA16F_EXT:                     return sizeof(unsigned short);
      case GL_LUMINANCE16F_EXT:                 return sizeof(unsigned short);
      case GL_LUMINANCE8_ALPHA8_EXT:            return sizeof(unsigned char) * 2;
      case GL_LUMINANCE_ALPHA32F_EXT:           return sizeof(float) * 2;
      case GL_LUMINANCE_ALPHA16F_EXT:           return sizeof(unsigned short) * 2;
      case GL_RGB8_OES:                         return sizeof(unsigned char) * 3;
      case GL_RGB565:                           return sizeof(unsigned short);
      case GL_RGB32F_EXT:                       return sizeof(float) * 3;
      case GL_RGB16F_EXT:                       return sizeof(unsigned short) * 3;
      case GL_RGBA8_OES:                        return sizeof(unsigned char) * 4;
      case GL_RGBA4:                            return sizeof(unsigned short);
      case GL_RGB5_A1:                          return sizeof(unsigned short);
      case GL_RGBA32F_EXT:                      return sizeof(float) * 4;
      case GL_RGBA16F_EXT:                      return sizeof(unsigned short) * 4;
      case GL_BGRA8_EXT:                        return sizeof(unsigned char) * 4;
      case GL_BGRA4_ANGLEX:                     return sizeof(unsigned short);
      case GL_BGR5_A1_ANGLEX:                   return sizeof(unsigned short);
      default: UNREACHABLE();
    }

    return 0;
}

bool IsCubemapTextureTarget(GLenum target)
{
    return (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
}

bool IsInternalTextureTarget(GLenum target)
{
    return target == GL_TEXTURE_2D || IsCubemapTextureTarget(target);
}

GLint ConvertSizedInternalFormat(GLenum format, GLenum type)
{
    switch (format)
    {
      case GL_ALPHA:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:    return GL_ALPHA8_EXT;
          case GL_FLOAT:            return GL_ALPHA32F_EXT;
          case GL_HALF_FLOAT_OES:   return GL_ALPHA16F_EXT;
          default:                  UNIMPLEMENTED();
        }
        break;
      case GL_LUMINANCE:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:    return GL_LUMINANCE8_EXT;
          case GL_FLOAT:            return GL_LUMINANCE32F_EXT;
          case GL_HALF_FLOAT_OES:   return GL_LUMINANCE16F_EXT;
          default:                  UNIMPLEMENTED();
        }
        break;
      case GL_LUMINANCE_ALPHA:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:    return GL_LUMINANCE8_ALPHA8_EXT;
          case GL_FLOAT:            return GL_LUMINANCE_ALPHA32F_EXT;
          case GL_HALF_FLOAT_OES:   return GL_LUMINANCE_ALPHA16F_EXT;
          default:                  UNIMPLEMENTED();
        }
        break;
      case GL_RGB:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:            return GL_RGB8_OES;
          case GL_UNSIGNED_SHORT_5_6_5:     return GL_RGB565;
          case GL_FLOAT:                    return GL_RGB32F_EXT;
          case GL_HALF_FLOAT_OES:           return GL_RGB16F_EXT;
          default:                          UNIMPLEMENTED();
        }
        break;
      case GL_RGBA:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:            return GL_RGBA8_OES;
          case GL_UNSIGNED_SHORT_4_4_4_4:   return GL_RGBA4;
          case GL_UNSIGNED_SHORT_5_5_5_1:   return GL_RGB5_A1;
          case GL_FLOAT:                    return GL_RGBA32F_EXT;
          case GL_HALF_FLOAT_OES:           return GL_RGBA16F_EXT;
            break;
          default:                          UNIMPLEMENTED();
        }
        break;
      case GL_BGRA_EXT:
        switch (type)
        {
          case GL_UNSIGNED_BYTE:                    return GL_BGRA8_EXT;
          case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:   return GL_BGRA4_ANGLEX;
          case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:   return GL_BGR5_A1_ANGLEX;
          default:                                  UNIMPLEMENTED();
        }
        break;
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
      case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
        return format;
      case GL_DEPTH_COMPONENT:
        switch (type)
        {
          case GL_UNSIGNED_SHORT:           return GL_DEPTH_COMPONENT16;
          case GL_UNSIGNED_INT:             return GL_DEPTH_COMPONENT32_OES;
          default:                          UNIMPLEMENTED();
        }
        break;
      case GL_DEPTH_STENCIL_OES:
        switch (type)
        {
          case GL_UNSIGNED_INT_24_8_OES:    return GL_DEPTH24_STENCIL8_OES;
          default:                          UNIMPLEMENTED();
        }
        break;
      default:
        UNIMPLEMENTED();
    }

    return GL_NONE;
}

GLenum ExtractFormat(GLenum internalformat)
{
    switch (internalformat)
    {
      case GL_RGB565:                          return GL_RGB;
      case GL_RGBA4:                           return GL_RGBA;
      case GL_RGB5_A1:                         return GL_RGBA;
      case GL_RGB8_OES:                        return GL_RGB;
      case GL_RGBA8_OES:                       return GL_RGBA;
      case GL_LUMINANCE8_ALPHA8_EXT:           return GL_LUMINANCE_ALPHA;
      case GL_LUMINANCE8_EXT:                  return GL_LUMINANCE;
      case GL_ALPHA8_EXT:                      return GL_ALPHA;
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:    return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:   return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE: return GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE;
      case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE: return GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE;
      case GL_RGBA32F_EXT:                     return GL_RGBA;
      case GL_RGB32F_EXT:                      return GL_RGB;
      case GL_ALPHA32F_EXT:                    return GL_ALPHA;
      case GL_LUMINANCE32F_EXT:                return GL_LUMINANCE;
      case GL_LUMINANCE_ALPHA32F_EXT:          return GL_LUMINANCE_ALPHA;
      case GL_RGBA16F_EXT:                     return GL_RGBA;
      case GL_RGB16F_EXT:                      return GL_RGB;
      case GL_ALPHA16F_EXT:                    return GL_ALPHA;
      case GL_LUMINANCE16F_EXT:                return GL_LUMINANCE;
      case GL_LUMINANCE_ALPHA16F_EXT:          return GL_LUMINANCE_ALPHA;
      case GL_BGRA8_EXT:                       return GL_BGRA_EXT;
      case GL_DEPTH_COMPONENT16:               return GL_DEPTH_COMPONENT;
      case GL_DEPTH_COMPONENT32_OES:           return GL_DEPTH_COMPONENT;
      case GL_DEPTH24_STENCIL8_OES:            return GL_DEPTH_STENCIL_OES;
      default:                                 return GL_NONE;   // Unsupported
    }
}

GLenum ExtractType(GLenum internalformat)
{
    switch (internalformat)
    {
      case GL_RGB565:                          return GL_UNSIGNED_SHORT_5_6_5;
      case GL_RGBA4:                           return GL_UNSIGNED_SHORT_4_4_4_4;
      case GL_RGB5_A1:                         return GL_UNSIGNED_SHORT_5_5_5_1;
      case GL_RGB8_OES:                        return GL_UNSIGNED_BYTE;
      case GL_RGBA8_OES:                       return GL_UNSIGNED_BYTE;
      case GL_LUMINANCE8_ALPHA8_EXT:           return GL_UNSIGNED_BYTE;
      case GL_LUMINANCE8_EXT:                  return GL_UNSIGNED_BYTE;
      case GL_ALPHA8_EXT:                      return GL_UNSIGNED_BYTE;
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:    return GL_UNSIGNED_BYTE;
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:   return GL_UNSIGNED_BYTE;
      case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE: return GL_UNSIGNED_BYTE;
      case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE: return GL_UNSIGNED_BYTE;
      case GL_RGBA32F_EXT:                     return GL_FLOAT;
      case GL_RGB32F_EXT:                      return GL_FLOAT;
      case GL_ALPHA32F_EXT:                    return GL_FLOAT;
      case GL_LUMINANCE32F_EXT:                return GL_FLOAT;
      case GL_LUMINANCE_ALPHA32F_EXT:          return GL_FLOAT;
      case GL_RGBA16F_EXT:                     return GL_HALF_FLOAT_OES;
      case GL_RGB16F_EXT:                      return GL_HALF_FLOAT_OES;
      case GL_ALPHA16F_EXT:                    return GL_HALF_FLOAT_OES;
      case GL_LUMINANCE16F_EXT:                return GL_HALF_FLOAT_OES;
      case GL_LUMINANCE_ALPHA16F_EXT:          return GL_HALF_FLOAT_OES;
      case GL_BGRA8_EXT:                       return GL_UNSIGNED_BYTE;
      case GL_DEPTH_COMPONENT16:               return GL_UNSIGNED_SHORT;
      case GL_DEPTH_COMPONENT32_OES:           return GL_UNSIGNED_INT;
      case GL_DEPTH24_STENCIL8_OES:            return GL_UNSIGNED_INT_24_8_OES;
      default:                                 return GL_NONE;   // Unsupported
    }
}

bool IsColorRenderable(GLenum internalformat)
{
    switch (internalformat)
    {
      case GL_RGBA4:
      case GL_RGB5_A1:
      case GL_RGB565:
      case GL_RGB8_OES:
      case GL_RGBA8_OES:
        return true;
      case GL_DEPTH_COMPONENT16:
      case GL_STENCIL_INDEX8:
      case GL_DEPTH24_STENCIL8_OES:
        return false;
      default:
        UNIMPLEMENTED();
    }

    return false;
}

bool IsDepthRenderable(GLenum internalformat)
{
    switch (internalformat)
    {
      case GL_DEPTH_COMPONENT16:
      case GL_DEPTH24_STENCIL8_OES:
        return true;
      case GL_STENCIL_INDEX8:
      case GL_RGBA4:
      case GL_RGB5_A1:
      case GL_RGB565:
      case GL_RGB8_OES:
      case GL_RGBA8_OES:
        return false;
      default:
        UNIMPLEMENTED();
    }

    return false;
}

bool IsStencilRenderable(GLenum internalformat)
{
    switch (internalformat)
    {
      case GL_STENCIL_INDEX8:
      case GL_DEPTH24_STENCIL8_OES:
        return true;
      case GL_RGBA4:
      case GL_RGB5_A1:
      case GL_RGB565:
      case GL_RGB8_OES:
      case GL_RGBA8_OES:
      case GL_DEPTH_COMPONENT16:
        return false;
      default:
        UNIMPLEMENTED();
    }

    return false;
}

bool IsFloat32Format(GLint internalformat)
{
    switch (internalformat)
    {
      case GL_RGBA32F_EXT:
      case GL_RGB32F_EXT:
      case GL_ALPHA32F_EXT:
      case GL_LUMINANCE32F_EXT:
      case GL_LUMINANCE_ALPHA32F_EXT:
        return true;
      default:
        return false;
    }
}

bool IsFloat16Format(GLint internalformat)
{
    switch (internalformat)
    {
      case GL_RGBA16F_EXT:
      case GL_RGB16F_EXT:
      case GL_ALPHA16F_EXT:
      case GL_LUMINANCE16F_EXT:
      case GL_LUMINANCE_ALPHA16F_EXT:
        return true;
      default:
        return false;
    }
}

}

namespace es2dx
{

D3DCMPFUNC ConvertComparison(GLenum comparison)
{
    D3DCMPFUNC d3dComp = D3DCMP_ALWAYS;
    switch (comparison)
    {
      case GL_NEVER:    d3dComp = D3DCMP_NEVER;        break;
      case GL_ALWAYS:   d3dComp = D3DCMP_ALWAYS;       break;
      case GL_LESS:     d3dComp = D3DCMP_LESS;         break;
      case GL_LEQUAL:   d3dComp = D3DCMP_LESSEQUAL;    break;
      case GL_EQUAL:    d3dComp = D3DCMP_EQUAL;        break;
      case GL_GREATER:  d3dComp = D3DCMP_GREATER;      break;
      case GL_GEQUAL:   d3dComp = D3DCMP_GREATEREQUAL; break;
      case GL_NOTEQUAL: d3dComp = D3DCMP_NOTEQUAL;     break;
      default: UNREACHABLE();
    }

    return d3dComp;
}

D3DCOLOR ConvertColor(gl::Color color)
{
    return D3DCOLOR_RGBA(gl::unorm<8>(color.red),
                         gl::unorm<8>(color.green),
                         gl::unorm<8>(color.blue),
                         gl::unorm<8>(color.alpha));
}

D3DBLEND ConvertBlendFunc(GLenum blend)
{
    D3DBLEND d3dBlend = D3DBLEND_ZERO;

    switch (blend)
    {
      case GL_ZERO:                     d3dBlend = D3DBLEND_ZERO;           break;
      case GL_ONE:                      d3dBlend = D3DBLEND_ONE;            break;
      case GL_SRC_COLOR:                d3dBlend = D3DBLEND_SRCCOLOR;       break;
      case GL_ONE_MINUS_SRC_COLOR:      d3dBlend = D3DBLEND_INVSRCCOLOR;    break;
      case GL_DST_COLOR:                d3dBlend = D3DBLEND_DESTCOLOR;      break;
      case GL_ONE_MINUS_DST_COLOR:      d3dBlend = D3DBLEND_INVDESTCOLOR;   break;
      case GL_SRC_ALPHA:                d3dBlend = D3DBLEND_SRCALPHA;       break;
      case GL_ONE_MINUS_SRC_ALPHA:      d3dBlend = D3DBLEND_INVSRCALPHA;    break;
      case GL_DST_ALPHA:                d3dBlend = D3DBLEND_DESTALPHA;      break;
      case GL_ONE_MINUS_DST_ALPHA:      d3dBlend = D3DBLEND_INVDESTALPHA;   break;
      case GL_CONSTANT_COLOR:           d3dBlend = D3DBLEND_BLENDFACTOR;    break;
      case GL_ONE_MINUS_CONSTANT_COLOR: d3dBlend = D3DBLEND_INVBLENDFACTOR; break;
      case GL_CONSTANT_ALPHA:           d3dBlend = D3DBLEND_BLENDFACTOR;    break;
      case GL_ONE_MINUS_CONSTANT_ALPHA: d3dBlend = D3DBLEND_INVBLENDFACTOR; break;
      case GL_SRC_ALPHA_SATURATE:       d3dBlend = D3DBLEND_SRCALPHASAT;    break;
      default: UNREACHABLE();
    }

    return d3dBlend;
}

D3DBLENDOP ConvertBlendOp(GLenum blendOp)
{
    D3DBLENDOP d3dBlendOp = D3DBLENDOP_ADD;

    switch (blendOp)
    {
      case GL_FUNC_ADD:              d3dBlendOp = D3DBLENDOP_ADD;         break;
      case GL_FUNC_SUBTRACT:         d3dBlendOp = D3DBLENDOP_SUBTRACT;    break;
      case GL_FUNC_REVERSE_SUBTRACT: d3dBlendOp = D3DBLENDOP_REVSUBTRACT; break;
      default: UNREACHABLE();
    }

    return d3dBlendOp;
}

D3DSTENCILOP ConvertStencilOp(GLenum stencilOp)
{
    D3DSTENCILOP d3dStencilOp = D3DSTENCILOP_KEEP;

    switch (stencilOp)
    {
      case GL_ZERO:      d3dStencilOp = D3DSTENCILOP_ZERO;    break;
      case GL_KEEP:      d3dStencilOp = D3DSTENCILOP_KEEP;    break;
      case GL_REPLACE:   d3dStencilOp = D3DSTENCILOP_REPLACE; break;
      case GL_INCR:      d3dStencilOp = D3DSTENCILOP_INCRSAT; break;
      case GL_DECR:      d3dStencilOp = D3DSTENCILOP_DECRSAT; break;
      case GL_INVERT:    d3dStencilOp = D3DSTENCILOP_INVERT;  break;
      case GL_INCR_WRAP: d3dStencilOp = D3DSTENCILOP_INCR;    break;
      case GL_DECR_WRAP: d3dStencilOp = D3DSTENCILOP_DECR;    break;
      default: UNREACHABLE();
    }

    return d3dStencilOp;
}

D3DTEXTUREADDRESS ConvertTextureWrap(GLenum wrap)
{
    D3DTEXTUREADDRESS d3dWrap = D3DTADDRESS_WRAP;

    switch (wrap)
    {
      case GL_REPEAT:            d3dWrap = D3DTADDRESS_WRAP;   break;
      case GL_CLAMP_TO_EDGE:     d3dWrap = D3DTADDRESS_CLAMP;  break;
      case GL_MIRRORED_REPEAT:   d3dWrap = D3DTADDRESS_MIRROR; break;
      default: UNREACHABLE();
    }

    return d3dWrap;
}

D3DCULL ConvertCullMode(GLenum cullFace, GLenum frontFace)
{
    D3DCULL cull = D3DCULL_CCW;
    switch (cullFace)
    {
      case GL_FRONT:
        cull = (frontFace == GL_CCW ? D3DCULL_CW : D3DCULL_CCW);
        break;
      case GL_BACK:
        cull = (frontFace == GL_CCW ? D3DCULL_CCW : D3DCULL_CW);
        break;
      case GL_FRONT_AND_BACK:
        cull = D3DCULL_NONE; // culling will be handled during draw
        break;
      default: UNREACHABLE();
    }

    return cull;
}

D3DCUBEMAP_FACES ConvertCubeFace(GLenum cubeFace)
{
    D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X;

    switch (cubeFace)
    {
      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        face = D3DCUBEMAP_FACE_POSITIVE_X;
        break;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        face = D3DCUBEMAP_FACE_NEGATIVE_X;
        break;
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        face = D3DCUBEMAP_FACE_POSITIVE_Y;
        break;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        face = D3DCUBEMAP_FACE_NEGATIVE_Y;
        break;
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        face = D3DCUBEMAP_FACE_POSITIVE_Z;
        break;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        face = D3DCUBEMAP_FACE_NEGATIVE_Z;
        break;
      default: UNREACHABLE();
    }

    return face;
}

DWORD ConvertColorMask(bool red, bool green, bool blue, bool alpha)
{
    return (red   ? D3DCOLORWRITEENABLE_RED   : 0) |
           (green ? D3DCOLORWRITEENABLE_GREEN : 0) |
           (blue  ? D3DCOLORWRITEENABLE_BLUE  : 0) |
           (alpha ? D3DCOLORWRITEENABLE_ALPHA : 0);
}

D3DTEXTUREFILTERTYPE ConvertMagFilter(GLenum magFilter, float maxAnisotropy)
{
    if (maxAnisotropy > 1.0f)
    {
        return D3DTEXF_ANISOTROPIC;
    }

    D3DTEXTUREFILTERTYPE d3dMagFilter = D3DTEXF_POINT;
    switch (magFilter)
    {
      case GL_NEAREST: d3dMagFilter = D3DTEXF_POINT;  break;
      case GL_LINEAR:  d3dMagFilter = D3DTEXF_LINEAR; break;
      default: UNREACHABLE();
    }

    return d3dMagFilter;
}

void ConvertMinFilter(GLenum minFilter, D3DTEXTUREFILTERTYPE *d3dMinFilter, D3DTEXTUREFILTERTYPE *d3dMipFilter, float maxAnisotropy)
{
    switch (minFilter)
    {
      case GL_NEAREST:
        *d3dMinFilter = D3DTEXF_POINT;
        *d3dMipFilter = D3DTEXF_NONE;
        break;
      case GL_LINEAR:
        *d3dMinFilter = D3DTEXF_LINEAR;
        *d3dMipFilter = D3DTEXF_NONE;
        break;
      case GL_NEAREST_MIPMAP_NEAREST:
        *d3dMinFilter = D3DTEXF_POINT;
        *d3dMipFilter = D3DTEXF_POINT;
        break;
      case GL_LINEAR_MIPMAP_NEAREST:
        *d3dMinFilter = D3DTEXF_LINEAR;
        *d3dMipFilter = D3DTEXF_POINT;
        break;
      case GL_NEAREST_MIPMAP_LINEAR:
        *d3dMinFilter = D3DTEXF_POINT;
        *d3dMipFilter = D3DTEXF_LINEAR;
        break;
      case GL_LINEAR_MIPMAP_LINEAR:
        *d3dMinFilter = D3DTEXF_LINEAR;
        *d3dMipFilter = D3DTEXF_LINEAR;
        break;
      default:
        *d3dMinFilter = D3DTEXF_POINT;
        *d3dMipFilter = D3DTEXF_NONE;
        UNREACHABLE();
    }

    if (maxAnisotropy > 1.0f)
    {
        *d3dMinFilter = D3DTEXF_ANISOTROPIC;
    }
}

bool ConvertPrimitiveType(GLenum primitiveType, GLsizei elementCount,
                          D3DPRIMITIVETYPE *d3dPrimitiveType, int *d3dPrimitiveCount)
{
    switch (primitiveType)
    {
      case GL_POINTS:
        *d3dPrimitiveType = D3DPT_POINTLIST;
        *d3dPrimitiveCount = elementCount;
        break;
      case GL_LINES:
        *d3dPrimitiveType = D3DPT_LINELIST;
        *d3dPrimitiveCount = elementCount / 2;
        break;
      case GL_LINE_LOOP:
        *d3dPrimitiveType = D3DPT_LINESTRIP;
        *d3dPrimitiveCount = elementCount - 1;   // D3D doesn't support line loops, so we draw the last line separately
        break;
      case GL_LINE_STRIP:
        *d3dPrimitiveType = D3DPT_LINESTRIP;
        *d3dPrimitiveCount = elementCount - 1;
        break;
      case GL_TRIANGLES:
        *d3dPrimitiveType = D3DPT_TRIANGLELIST;
        *d3dPrimitiveCount = elementCount / 3;
        break;
      case GL_TRIANGLE_STRIP:
        *d3dPrimitiveType = D3DPT_TRIANGLESTRIP;
        *d3dPrimitiveCount = elementCount - 2;
        break;
      case GL_TRIANGLE_FAN:
        *d3dPrimitiveType = D3DPT_TRIANGLEFAN;
        *d3dPrimitiveCount = elementCount - 2;
        break;
      default:
        return false;
    }

    return true;
}

D3DFORMAT ConvertRenderbufferFormat(GLenum format)
{
    switch (format)
    {
      case GL_NONE:                 return D3DFMT_NULL;
      case GL_RGBA4:
      case GL_RGB5_A1:
      case GL_RGBA8_OES:            return D3DFMT_A8R8G8B8;
      case GL_RGB565:               return D3DFMT_R5G6B5;
      case GL_RGB8_OES:             return D3DFMT_X8R8G8B8;
      case GL_DEPTH_COMPONENT16:
      case GL_STENCIL_INDEX8:       
      case GL_DEPTH24_STENCIL8_OES: return D3DFMT_D24S8;
      default: UNREACHABLE();       return D3DFMT_A8R8G8B8;
    }
}

D3DMULTISAMPLE_TYPE GetMultisampleTypeFromSamples(GLsizei samples)
{
    if (samples <= 1)
        return D3DMULTISAMPLE_NONE;
    else
        return (D3DMULTISAMPLE_TYPE)samples;
}

}

namespace dx2es
{

unsigned int GetStencilSize(D3DFORMAT stencilFormat)
{
    if (stencilFormat == D3DFMT_INTZ)
    {
        return 8;
    }
    switch(stencilFormat)
    {
      case D3DFMT_D24FS8:
      case D3DFMT_D24S8:
        return 8;
      case D3DFMT_D24X4S4:
        return 4;
      case D3DFMT_D15S1:
        return 1;
      case D3DFMT_D16_LOCKABLE:
      case D3DFMT_D32:
      case D3DFMT_D24X8:
      case D3DFMT_D32F_LOCKABLE:
      case D3DFMT_D16:
        return 0;
    //case D3DFMT_D32_LOCKABLE:  return 0;   // DirectX 9Ex only
    //case D3DFMT_S8_LOCKABLE:   return 8;   // DirectX 9Ex only
      default:
        return 0;
    }
}

unsigned int GetAlphaSize(D3DFORMAT colorFormat)
{
    switch (colorFormat)
    {
      case D3DFMT_A16B16G16R16F:
        return 16;
      case D3DFMT_A32B32G32R32F:
        return 32;
      case D3DFMT_A2R10G10B10:
        return 2;
      case D3DFMT_A8R8G8B8:
        return 8;
      case D3DFMT_A1R5G5B5:
        return 1;
      case D3DFMT_X8R8G8B8:
      case D3DFMT_R5G6B5:
        return 0;
      default:
        return 0;
    }
}

unsigned int GetRedSize(D3DFORMAT colorFormat)
{
    switch (colorFormat)
    {
      case D3DFMT_A16B16G16R16F:
        return 16;
      case D3DFMT_A32B32G32R32F:
        return 32;
      case D3DFMT_A2R10G10B10:
        return 10;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
        return 8;
      case D3DFMT_A1R5G5B5:
      case D3DFMT_R5G6B5:
        return 5;
      default:
        return 0;
    }
}

unsigned int GetGreenSize(D3DFORMAT colorFormat)
{
    switch (colorFormat)
    {
      case D3DFMT_A16B16G16R16F:
        return 16;
      case D3DFMT_A32B32G32R32F:
        return 32;
      case D3DFMT_A2R10G10B10:
        return 10;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
        return 8;
      case D3DFMT_A1R5G5B5:
        return 5;
      case D3DFMT_R5G6B5:
        return 6;
      default:
        return 0;
    }
}

unsigned int GetBlueSize(D3DFORMAT colorFormat)
{
    switch (colorFormat)
    {
      case D3DFMT_A16B16G16R16F:
        return 16;
      case D3DFMT_A32B32G32R32F:
        return 32;
      case D3DFMT_A2R10G10B10:
        return 10;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
        return 8;
      case D3DFMT_A1R5G5B5:
      case D3DFMT_R5G6B5:
        return 5;
      default:
        return 0;
    }
}

unsigned int GetDepthSize(D3DFORMAT depthFormat)
{
    if (depthFormat == D3DFMT_INTZ)
    {
        return 24;
    }
    switch (depthFormat)
    {
      case D3DFMT_D16_LOCKABLE:  return 16;
      case D3DFMT_D32:           return 32;
      case D3DFMT_D15S1:         return 15;
      case D3DFMT_D24S8:         return 24;
      case D3DFMT_D24X8:         return 24;
      case D3DFMT_D24X4S4:       return 24;
      case D3DFMT_D16:           return 16;
      case D3DFMT_D32F_LOCKABLE: return 32;
      case D3DFMT_D24FS8:        return 24;
    //case D3DFMT_D32_LOCKABLE:  return 32;   // D3D9Ex only
    //case D3DFMT_S8_LOCKABLE:   return 0;    // D3D9Ex only
      default:                   return 0;
    }
}

GLsizei GetSamplesFromMultisampleType(D3DMULTISAMPLE_TYPE type)
{
    if (type == D3DMULTISAMPLE_NONMASKABLE)
        return 0;
    else
        return type;
}

bool IsFormatChannelEquivalent(D3DFORMAT d3dformat, GLenum format)
{
    switch (d3dformat)
    {
      case D3DFMT_L8:
        return (format == GL_LUMINANCE);
      case D3DFMT_A8L8:
        return (format == GL_LUMINANCE_ALPHA);
      case D3DFMT_DXT1:
        return (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
      case D3DFMT_DXT3:
        return (format == GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE);
      case D3DFMT_DXT5:
        return (format == GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE);
      case D3DFMT_A8R8G8B8:
      case D3DFMT_A16B16G16R16F:
      case D3DFMT_A32B32G32R32F:
        return (format == GL_RGBA || format == GL_BGRA_EXT);
      case D3DFMT_X8R8G8B8:
        return (format == GL_RGB);
      default:
        if (d3dformat == D3DFMT_INTZ && gl::IsDepthTexture(format))
            return true;
        return false;
    }
}

bool ConvertReadBufferFormat(D3DFORMAT d3dformat, GLenum *format, GLenum *type)
{
    switch (d3dformat)
    {
      case D3DFMT_A8R8G8B8:
        *type = GL_UNSIGNED_BYTE;
        *format = GL_BGRA_EXT;
        break;
      case D3DFMT_X8R8G8B8:
        *type = GL_UNSIGNED_BYTE;
        *format = GL_RGB;
        break;
      case D3DFMT_R5G6B5:
        *type = GL_UNSIGNED_SHORT_5_6_5;
        *format = GL_RGB;
        break;
      case D3DFMT_A16B16G16R16F:
        *type = GL_HALF_FLOAT_OES;
        *format = GL_RGBA;
        break;
      case D3DFMT_A32B32G32R32F:
        *type = GL_FLOAT;
        *format = GL_RGBA;
        break;
      case D3DFMT_A4R4G4B4:
        *type = GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT;
        *format = GL_BGRA_EXT;
        break;
      case D3DFMT_A1R5G5B5:
        *type = GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT;
        *format = GL_BGRA_EXT;
        break;
      default:
        *type = GL_NONE;
        *format = GL_NONE;
        return false;
    }
    return true;
}

GLenum ConvertBackBufferFormat(D3DFORMAT format)
{
    switch (format)
    {
      case D3DFMT_A4R4G4B4: return GL_RGBA4;
      case D3DFMT_A8R8G8B8: return GL_RGBA8_OES;
      case D3DFMT_A1R5G5B5: return GL_RGB5_A1;
      case D3DFMT_R5G6B5:   return GL_RGB565;
      case D3DFMT_X8R8G8B8: return GL_RGB8_OES;
      default:
        UNREACHABLE();
    }

    return GL_RGBA4;
}

GLenum ConvertDepthStencilFormat(D3DFORMAT format)
{
    if (format == D3DFMT_INTZ)
    {
        return GL_DEPTH24_STENCIL8_OES;
    }
    switch (format)
    {
      case D3DFMT_D16:
      case D3DFMT_D24X8:
        return GL_DEPTH_COMPONENT16;
      case D3DFMT_D24S8:
        return GL_DEPTH24_STENCIL8_OES;
      default:
        UNREACHABLE();
    }

    return GL_DEPTH24_STENCIL8_OES;
}

}

namespace dx
{

bool IsCompressedFormat(D3DFORMAT surfaceFormat)
{
    switch(surfaceFormat)
    {
      case D3DFMT_DXT1:
      case D3DFMT_DXT2:
      case D3DFMT_DXT3:
      case D3DFMT_DXT4:
      case D3DFMT_DXT5:
        return true;
      default:
        return false;
    }
}

size_t ComputeRowSize(D3DFORMAT format, unsigned int width)
{
    if (format == D3DFMT_INTZ)
    {
        return 4 * width;
    }
    switch (format)
    {
      case D3DFMT_L8:
          return 1 * width;
      case D3DFMT_A8L8:
          return 2 * width;
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A8R8G8B8:
        return 4 * width;
      case D3DFMT_A16B16G16R16F:
        return 8 * width;
      case D3DFMT_A32B32G32R32F:
        return 16 * width;
      case D3DFMT_DXT1:
        return 8 * ((width + 3) / 4);
      case D3DFMT_DXT3:
      case D3DFMT_DXT5:
        return 16 * ((width + 3) / 4);
      default:
        UNREACHABLE();
        return 0;
    }
}

}

std::string getTempPath()
{
    char path[MAX_PATH];
    DWORD pathLen = GetTempPathA(sizeof(path) / sizeof(path[0]), path);
    if (pathLen == 0)
    {
        UNREACHABLE();
        return std::string();
    }

    UINT unique = GetTempFileNameA(path, "sh", 0, path);
    if (unique == 0)
    {
        UNREACHABLE();
        return std::string();
    }
    
    return path;
}

void writeFile(const char* path, const void* content, size_t size)
{
    FILE* file = fopen(path, "w");
    if (!file)
    {
        UNREACHABLE();
        return;
    }

    fwrite(content, sizeof(char), size, file);
    fclose(file);
}
