//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.h: Conversion functions and other utility routines.

#ifndef COMMON_UTILITIES_H_
#define COMMON_UTILITIES_H_

#include "angle_gl.h"
#include <string>
#include <math.h>

namespace gl
{

int VariableComponentCount(GLenum type);
GLenum VariableComponentType(GLenum type);
size_t VariableComponentSize(GLenum type);
size_t VariableInternalSize(GLenum type);
size_t VariableExternalSize(GLenum type);
GLenum VariableBoolVectorType(GLenum type);
int VariableRowCount(GLenum type);
int VariableColumnCount(GLenum type);
bool IsSamplerType(GLenum type);
bool IsMatrixType(GLenum type);
GLenum TransposeMatrixType(GLenum type);
int VariableRegisterCount(GLenum type);
int MatrixRegisterCount(GLenum type, bool isRowMajorMatrix);
int MatrixComponentCount(GLenum type, bool isRowMajorMatrix);
int VariableSortOrder(GLenum type);

int AllocateFirstFreeBits(unsigned int *bits, unsigned int allocationSize, unsigned int bitsSize);

static const GLenum FirstCubeMapTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
static const GLenum LastCubeMapTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
bool IsCubeMapTextureTarget(GLenum target);
size_t CubeMapTextureTargetToLayerIndex(GLenum target);
GLenum LayerIndexToCubeMapTextureTarget(size_t index);

// Parse the base uniform name and array index.  Returns the base name of the uniform. outSubscript is
// set to GL_INVALID_INDEX if the provided name is not an array or the array index is invalid.
std::string ParseUniformName(const std::string &name, size_t *outSubscript);

bool IsTriangleMode(GLenum drawMode);

// [OpenGL ES 3.0.2] Section 2.3.1 page 14
// Data Conversion For State-Setting Commands
// Floating-point values are rounded to the nearest integer, instead of truncated, as done by static_cast.
template <typename outT> outT iround(GLfloat value) { return static_cast<outT>(value > 0.0f ? floor(value + 0.5f) : ceil(value - 0.5f)); }
template <typename outT> outT uiround(GLfloat value) { return static_cast<outT>(value + 0.5f); }

}

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
std::string getTempPath();
void writeFile(const char* path, const void* data, size_t size);
#endif

#if defined (ANGLE_PLATFORM_WINDOWS)
void ScheduleYield();
#endif

#endif  // COMMON_UTILITIES_H_
