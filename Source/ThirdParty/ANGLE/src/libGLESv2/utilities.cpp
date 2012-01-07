//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.cpp: Conversion functions and other utility routines.

#include "libGLESv2/utilities.h"

#include <limits>
#include <stdio.h>
#include <windows.h>

#include "common/debug.h"

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

GLsizei ComputePitch(GLsizei width, GLenum format, GLenum type, GLint alignment)
{
    ASSERT(alignment > 0 && isPow2(alignment));

    GLsizei rawPitch = ComputePixelSize(format, type) * width;
    return (rawPitch + alignment - 1) & ~(alignment - 1);
}

GLsizei ComputeCompressedPitch(GLsizei width, GLenum format)
{
    return ComputeCompressedSize(width, 1, format);
}

GLsizei ComputeCompressedSize(GLsizei width, GLsizei height, GLenum format)
{
    switch (format)
    {
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return 8 * (GLsizei)ceil((float)width / 4.0f) * (GLsizei)ceil((float)height / 4.0f);
        break;
      case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE:
      case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE:
        return 16 * (GLsizei)ceil((float)width / 4.0f) * (GLsizei)ceil((float)height / 4.0f);
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

// Returns the size, in bytes, of a single texel in an Image
int ComputePixelSize(GLenum format, GLenum type)
{
    switch (type)
    {
      case GL_UNSIGNED_BYTE:
        switch (format)
        {
          case GL_ALPHA:           return sizeof(unsigned char);
          case GL_LUMINANCE:       return sizeof(unsigned char);
          case GL_LUMINANCE_ALPHA: return sizeof(unsigned char) * 2;
          case GL_RGB:             return sizeof(unsigned char) * 3;
          case GL_RGBA:            return sizeof(unsigned char) * 4;
          case GL_BGRA_EXT:        return sizeof(unsigned char) * 4;
          default: UNREACHABLE();
        }
        break;
      case GL_UNSIGNED_SHORT_4_4_4_4:
      case GL_UNSIGNED_SHORT_5_5_5_1:
      case GL_UNSIGNED_SHORT_5_6_5:
        return sizeof(unsigned short);
      case GL_FLOAT:
        switch (format)
        {
          case GL_ALPHA:           return sizeof(float);
          case GL_LUMINANCE:       return sizeof(float);
          case GL_LUMINANCE_ALPHA: return sizeof(float) * 2;
          case GL_RGB:             return sizeof(float) * 3;
          case GL_RGBA:            return sizeof(float) * 4;
          default: UNREACHABLE();
        }
        break;
      case GL_HALF_FLOAT_OES:
        switch (format)
        {
          case GL_ALPHA:           return sizeof(unsigned short);
          case GL_LUMINANCE:       return sizeof(unsigned short);
          case GL_LUMINANCE_ALPHA: return sizeof(unsigned short) * 2;
          case GL_RGB:             return sizeof(unsigned short) * 3;
          case GL_RGBA:            return sizeof(unsigned short) * 4;
          default: UNREACHABLE();
        }
        break;
      default: UNREACHABLE();
    }

    return 0;
}

bool IsCubemapTextureTarget(GLenum target)
{
    return (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
}

bool IsTextureTarget(GLenum target)
{
    return target == GL_TEXTURE_2D || IsCubemapTextureTarget(target);
}

// Verify that format/type are one of the combinations from table 3.4.
bool CheckTextureFormatType(GLenum format, GLenum type)
{
    switch (type)
    {
      case GL_UNSIGNED_BYTE:
        switch (format)
        {
          case GL_RGBA:
          case GL_BGRA_EXT:
          case GL_RGB:
          case GL_ALPHA:
          case GL_LUMINANCE:
          case GL_LUMINANCE_ALPHA:
            return true;

          default:
            return false;
        }

      case GL_FLOAT:
      case GL_HALF_FLOAT_OES:
        switch (format)
        {
          case GL_RGBA:
          case GL_RGB:
          case GL_ALPHA:
          case GL_LUMINANCE:
          case GL_LUMINANCE_ALPHA:
            return true;

          default:
            return false;
        }

      case GL_UNSIGNED_SHORT_4_4_4_4:
      case GL_UNSIGNED_SHORT_5_5_5_1:
        return (format == GL_RGBA);

      case GL_UNSIGNED_SHORT_5_6_5:
        return (format == GL_RGB);

      default:
        return false;
    }
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

    // Map a cube map texture target to the corresponding  D3D surface index. Note that the
    // Y faces are swapped because the Y coordinate to the texture lookup intrinsic functions
    // are negated in the pixel shader.
    switch (cubeFace)
    {
      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        face = D3DCUBEMAP_FACE_POSITIVE_X;
        break;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        face = D3DCUBEMAP_FACE_NEGATIVE_X;
        break;
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        face = D3DCUBEMAP_FACE_NEGATIVE_Y;
        break;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        face = D3DCUBEMAP_FACE_POSITIVE_Y;
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

D3DTEXTUREFILTERTYPE ConvertMagFilter(GLenum magFilter)
{
    D3DTEXTUREFILTERTYPE d3dMagFilter = D3DTEXF_POINT;
    switch (magFilter)
    {
      case GL_NEAREST: d3dMagFilter = D3DTEXF_POINT;  break;
      case GL_LINEAR:  d3dMagFilter = D3DTEXF_LINEAR; break;
      default: UNREACHABLE();
    }

    return d3dMagFilter;
}

void ConvertMinFilter(GLenum minFilter, D3DTEXTUREFILTERTYPE *d3dMinFilter, D3DTEXTUREFILTERTYPE *d3dMipFilter)
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

bool IsFloat32Format(D3DFORMAT surfaceFormat)
{
    switch(surfaceFormat)
    {
      case D3DFMT_R16F:
      case D3DFMT_G16R16F:
      case D3DFMT_A16B16G16R16F:
        return false;
      case D3DFMT_R32F:
      case D3DFMT_G32R32F:
      case D3DFMT_A32B32G32R32F:
        return true;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A1R5G5B5:
      case D3DFMT_R5G6B5:
        return false;
      default: UNREACHABLE();
    }
    return false;
}

bool IsFloat16Format(D3DFORMAT surfaceFormat)
{
    switch(surfaceFormat)
    {
      case D3DFMT_R16F:
      case D3DFMT_G16R16F:
      case D3DFMT_A16B16G16R16F:
        return true;
      case D3DFMT_R32F:
      case D3DFMT_G32R32F:
      case D3DFMT_A32B32G32R32F:
        return false;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A1R5G5B5:
      case D3DFMT_R5G6B5:
        return false;
      default: UNREACHABLE();
    }
    return false;
}

GLsizei GetSamplesFromMultisampleType(D3DMULTISAMPLE_TYPE type)
{
    if (type == D3DMULTISAMPLE_NONMASKABLE)
        return 0;
    else
        return type;
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
