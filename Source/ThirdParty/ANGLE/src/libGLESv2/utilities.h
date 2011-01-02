//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
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

namespace gl
{

struct Color;

int UniformComponentCount(GLenum type);
GLenum UniformComponentType(GLenum type);
size_t UniformTypeSize(GLenum type);
int VariableRowCount(GLenum type);
int VariableColumnCount(GLenum type);

int AllocateFirstFreeBits(unsigned int *bits, unsigned int allocationSize, unsigned int bitsSize);

int ComputePixelSize(GLenum format, GLenum type);
GLsizei ComputePitch(GLsizei width, GLenum format, GLenum type, GLint alignment);
GLsizei ComputeCompressedPitch(GLsizei width, GLenum format);
GLsizei ComputeCompressedSize(GLsizei width, GLsizei height, GLenum format);
bool IsCompressed(GLenum format);
bool IsCubemapTextureTarget(GLenum target);
bool IsTextureTarget(GLenum target);
bool CheckTextureFormatType(GLenum format, GLenum type);

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
DWORD ConvertColorMask(bool red, bool green, bool blue, bool alpha);
D3DTEXTUREFILTERTYPE ConvertMagFilter(GLenum magFilter);
void ConvertMinFilter(GLenum minFilter, D3DTEXTUREFILTERTYPE *d3dMinFilter, D3DTEXTUREFILTERTYPE *d3dMipFilter);
unsigned int GetAlphaSize(D3DFORMAT colorFormat);
unsigned int GetRedSize(D3DFORMAT colorFormat);
unsigned int GetGreenSize(D3DFORMAT colorFormat);
unsigned int GetBlueSize(D3DFORMAT colorFormat);
unsigned int GetDepthSize(D3DFORMAT depthFormat);
unsigned int GetStencilSize(D3DFORMAT stencilFormat);
bool ConvertPrimitiveType(GLenum primitiveType, GLsizei primitiveCount,
                          D3DPRIMITIVETYPE *d3dPrimitiveType, int *d3dPrimitiveCount);
D3DFORMAT ConvertRenderbufferFormat(GLenum format);
D3DMULTISAMPLE_TYPE GetMultisampleTypeFromSamples(GLsizei samples);
GLsizei GetSamplesFromMultisampleType(D3DMULTISAMPLE_TYPE type);

}

#endif  // LIBGLESV2_UTILITIES_H
