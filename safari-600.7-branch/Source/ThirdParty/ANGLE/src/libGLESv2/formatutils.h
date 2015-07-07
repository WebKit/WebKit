//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils.h: Queries for GL image formats.

#ifndef LIBGLESV2_FORMATUTILS_H_
#define LIBGLESV2_FORMATUTILS_H_

#define GL_APICALL
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "libGLESv2/angletypes.h"

typedef void (*MipGenerationFunction)(unsigned int sourceWidth, unsigned int sourceHeight, unsigned int sourceDepth,
                                      const unsigned char *sourceData, int sourceRowPitch, int sourceDepthPitch,
                                      unsigned char *destData, int destRowPitch, int destDepthPitch);

typedef void (*LoadImageFunction)(int width, int height, int depth,
                                  const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                  void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch);

typedef void (*InitializeTextureDataFunction)(int width, int height, int depth,
                                              void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch);

typedef void (*ColorReadFunction)(const void *source, void *dest);
typedef void (*ColorWriteFunction)(const void *source, void *dest);
typedef void (*ColorCopyFunction)(const void *source, void *dest);

typedef void (*VertexCopyFunction)(const void *input, size_t stride, size_t count, void *output);

namespace rx
{

class Renderer;

}

namespace gl
{

class Context;

bool IsValidInternalFormat(GLenum internalFormat, const Context *context);
bool IsValidFormat(GLenum format, GLuint clientVersion);
bool IsValidType(GLenum type, GLuint clientVersion);

bool IsValidFormatCombination(GLenum internalFormat, GLenum format, GLenum type, GLuint clientVersion);
bool IsValidCopyTexImageCombination(GLenum textureInternalFormat, GLenum frameBufferInternalFormat, GLuint readBufferHandle, GLuint clientVersion);

bool IsSizedInternalFormat(GLenum internalFormat, GLuint clientVersion);
GLenum GetSizedInternalFormat(GLenum format, GLenum type, GLuint clientVersion);

GLuint GetPixelBytes(GLenum internalFormat, GLuint clientVersion);
GLuint GetAlphaBits(GLenum internalFormat, GLuint clientVersion);
GLuint GetRedBits(GLenum internalFormat, GLuint clientVersion);
GLuint GetGreenBits(GLenum internalFormat, GLuint clientVersion);
GLuint GetBlueBits(GLenum internalFormat, GLuint clientVersion);
GLuint GetLuminanceBits(GLenum internalFormat, GLuint clientVersion);
GLuint GetDepthBits(GLenum internalFormat, GLuint clientVersion);
GLuint GetStencilBits(GLenum internalFormat, GLuint clientVersion);

GLuint GetTypeBytes(GLenum type);
bool IsSpecialInterpretationType(GLenum type);
bool IsFloatOrFixedComponentType(GLenum type);

GLenum GetFormat(GLenum internalFormat, GLuint clientVersion);
GLenum GetType(GLenum internalFormat, GLuint clientVersion);

GLenum GetComponentType(GLenum internalFormat, GLuint clientVersion);
GLuint GetComponentCount(GLenum internalFormat, GLuint clientVersion);
GLenum GetColorEncoding(GLenum internalFormat, GLuint clientVersion);

bool IsColorRenderingSupported(GLenum internalFormat, const rx::Renderer *renderer);
bool IsColorRenderingSupported(GLenum internalFormat, const Context *context);
bool IsTextureFilteringSupported(GLenum internalFormat, const rx::Renderer *renderer);
bool IsTextureFilteringSupported(GLenum internalFormat, const Context *context);
bool IsDepthRenderingSupported(GLenum internalFormat, const rx::Renderer *renderer);
bool IsDepthRenderingSupported(GLenum internalFormat, const Context *context);
bool IsStencilRenderingSupported(GLenum internalFormat, const rx::Renderer *renderer);
bool IsStencilRenderingSupported(GLenum internalFormat, const Context *context);

GLuint GetRowPitch(GLenum internalFormat, GLenum type, GLuint clientVersion, GLsizei width, GLint alignment);
GLuint GetDepthPitch(GLenum internalFormat, GLenum type, GLuint clientVersion, GLsizei width, GLsizei height, GLint alignment);
GLuint GetBlockSize(GLenum internalFormat, GLenum type, GLuint clientVersion, GLsizei width, GLsizei height);

bool IsFormatCompressed(GLenum internalFormat, GLuint clientVersion);
GLuint GetCompressedBlockWidth(GLenum internalFormat, GLuint clientVersion);
GLuint GetCompressedBlockHeight(GLenum internalFormat, GLuint clientVersion);

ColorWriteFunction GetColorWriteFunction(GLenum format, GLenum type, GLuint clientVersion);

}

#endif LIBGLESV2_FORMATUTILS_H_
