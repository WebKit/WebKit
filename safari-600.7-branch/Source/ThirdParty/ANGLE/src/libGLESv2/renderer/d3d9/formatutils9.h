//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils9.h: Queries for GL image formats and their translations to D3D9
// formats.

#ifndef LIBGLESV2_RENDERER_FORMATUTILS9_H_
#define LIBGLESV2_RENDERER_FORMATUTILS9_H_

#include "libGLESv2/formatutils.h"

namespace rx
{

class Renderer9;

namespace d3d9
{

typedef std::set<D3DFORMAT> D3DFormatSet;

MipGenerationFunction GetMipGenerationFunction(D3DFORMAT format);
LoadImageFunction GetImageLoadFunction(GLenum internalFormat, const Renderer9 *renderer);

GLuint GetFormatPixelBytes(D3DFORMAT format);
GLuint GetBlockWidth(D3DFORMAT format);
GLuint GetBlockHeight(D3DFORMAT format);
GLuint GetBlockSize(D3DFORMAT format, GLuint width, GLuint height);

void MakeValidSize(bool isImage, D3DFORMAT format, GLsizei *requestWidth, GLsizei *requestHeight, int *levelOffset);

const D3DFormatSet &GetAllUsedD3DFormats();

ColorReadFunction GetColorReadFunction(D3DFORMAT format);
ColorCopyFunction GetFastCopyFunction(D3DFORMAT sourceFormat, GLenum destFormat, GLenum destType, GLuint clientVersion);

VertexCopyFunction GetVertexCopyFunction(const gl::VertexFormat &vertexFormat);
size_t GetVertexElementSize(const gl::VertexFormat &vertexFormat);
VertexConversionType GetVertexConversionType(const gl::VertexFormat &vertexFormat);
D3DDECLTYPE GetNativeVertexFormat(const gl::VertexFormat &vertexFormat);

GLenum GetDeclTypeComponentType(D3DDECLTYPE declType);
int GetDeclTypeComponentCount(D3DDECLTYPE declType);
bool IsDeclTypeNormalized(D3DDECLTYPE declType);

void InitializeVertexTranslations(const rx::Renderer9 *renderer);

}

namespace gl_d3d9
{

D3DFORMAT GetTextureFormat(GLenum internalFormat, const Renderer9 *renderer);
D3DFORMAT GetRenderFormat(GLenum internalFormat, const Renderer9 *renderer);

D3DMULTISAMPLE_TYPE GetMultisampleType(GLsizei samples);

bool RequiresTextureDataInitialization(GLint internalFormat);
InitializeTextureDataFunction GetTextureDataInitializationFunction(GLint internalFormat);

}

namespace d3d9_gl
{

GLenum GetInternalFormat(D3DFORMAT format);
GLsizei GetSamplesCount(D3DMULTISAMPLE_TYPE type);
bool IsFormatChannelEquivalent(D3DFORMAT d3dformat, GLenum format, GLuint clientVersion);

}

}

#endif // LIBGLESV2_RENDERER_FORMATUTILS9_H_
