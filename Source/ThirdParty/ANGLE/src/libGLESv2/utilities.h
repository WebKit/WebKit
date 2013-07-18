//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.h: Conversion functions and other utility routines.

#ifndef LIBGLESV2_UTILITIES_H
#define LIBGLESV2_UTILITIES_H

#define GL_APICALL
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <d3d9.h>

#include <string>

const D3DFORMAT D3DFMT_INTZ = ((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')));
const D3DFORMAT D3DFMT_NULL = ((D3DFORMAT)(MAKEFOURCC('N','U','L','L')));

namespace gl
{

struct Color;

int UniformExternalComponentCount(GLenum type);
int UniformInternalComponentCount(GLenum type);
GLenum UniformComponentType(GLenum type);
size_t UniformInternalSize(GLenum type);
size_t UniformExternalSize(GLenum type);
int VariableRowCount(GLenum type);
int VariableColumnCount(GLenum type);

int AllocateFirstFreeBits(unsigned int *bits, unsigned int allocationSize, unsigned int bitsSize);

int ComputePixelSize(GLint internalformat);
GLsizei ComputePitch(GLsizei width, GLint internalformat, GLint alignment);
GLsizei ComputeCompressedPitch(GLsizei width, GLenum format);
GLsizei ComputeCompressedSize(GLsizei width, GLsizei height, GLenum format);
bool IsCompressed(GLenum format);
bool IsDepthTexture(GLenum format);
bool IsStencilTexture(GLenum format);
bool IsCubemapTextureTarget(GLenum target);
bool IsInternalTextureTarget(GLenum target);
GLint ConvertSizedInternalFormat(GLenum format, GLenum type);
GLenum ExtractFormat(GLenum internalformat);
GLenum ExtractType(GLenum internalformat);

bool IsColorRenderable(GLenum internalformat);
bool IsDepthRenderable(GLenum internalformat);
bool IsStencilRenderable(GLenum internalformat);

bool IsFloat32Format(GLint internalformat);
bool IsFloat16Format(GLint internalformat);
}

namespace es2dx
{

D3DCMPFUNC ConvertComparison(GLenum comparison);
D3DCOLOR ConvertColor(gl::Color color);
D3DBLEND ConvertBlendFunc(GLenum blend);
D3DBLENDOP ConvertBlendOp(GLenum blendOp);
D3DSTENCILOP ConvertStencilOp(GLenum stencilOp);
D3DTEXTUREADDRESS ConvertTextureWrap(GLenum wrap);
D3DCULL ConvertCullMode(GLenum cullFace, GLenum frontFace);
D3DCUBEMAP_FACES ConvertCubeFace(GLenum cubeFace);
DWORD ConvertColorMask(bool red, bool green, bool blue, bool alpha);
D3DTEXTUREFILTERTYPE ConvertMagFilter(GLenum magFilter, float maxAnisotropy);
void ConvertMinFilter(GLenum minFilter, D3DTEXTUREFILTERTYPE *d3dMinFilter, D3DTEXTUREFILTERTYPE *d3dMipFilter, float maxAnisotropy);
bool ConvertPrimitiveType(GLenum primitiveType, GLsizei elementCount,
                          D3DPRIMITIVETYPE *d3dPrimitiveType, int *d3dPrimitiveCount);
D3DFORMAT ConvertRenderbufferFormat(GLenum format);
D3DMULTISAMPLE_TYPE GetMultisampleTypeFromSamples(GLsizei samples);

}

namespace dx2es
{

GLuint GetAlphaSize(D3DFORMAT colorFormat);
GLuint GetRedSize(D3DFORMAT colorFormat);
GLuint GetGreenSize(D3DFORMAT colorFormat);
GLuint GetBlueSize(D3DFORMAT colorFormat);
GLuint GetDepthSize(D3DFORMAT depthFormat);
GLuint GetStencilSize(D3DFORMAT stencilFormat);

GLsizei GetSamplesFromMultisampleType(D3DMULTISAMPLE_TYPE type);

bool IsFormatChannelEquivalent(D3DFORMAT d3dformat, GLenum format);
bool ConvertReadBufferFormat(D3DFORMAT d3dformat, GLenum *format, GLenum *type);
GLenum ConvertBackBufferFormat(D3DFORMAT format);
GLenum ConvertDepthStencilFormat(D3DFORMAT format);

}

namespace dx
{
bool IsCompressedFormat(D3DFORMAT format);
size_t ComputeRowSize(D3DFORMAT format, unsigned int width);
}

std::string getTempPath();
void writeFile(const char* path, const void* data, size_t size);

inline bool isDeviceLostError(HRESULT errorCode)
{
    switch (errorCode)
    {
      case D3DERR_DRIVERINTERNALERROR:
      case D3DERR_DEVICELOST:
      case D3DERR_DEVICEHUNG:
      case D3DERR_DEVICEREMOVED:
        return true;
      default:
        return false;
    }
}

#endif  // LIBGLESV2_UTILITIES_H
