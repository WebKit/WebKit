//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.cpp: Conversion functions and other utility routines.

#include "common/utilities.h"
#include "common/mathutil.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <set>

namespace gl
{

int UniformComponentCount(GLenum type)
{
    switch (type)
    {
      case GL_BOOL:
      case GL_FLOAT:
      case GL_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_3D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_UNSIGNED_INT:
        return 1;
      case GL_BOOL_VEC2:
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_UNSIGNED_INT_VEC2:
        return 2;
      case GL_INT_VEC3:
      case GL_FLOAT_VEC3:
      case GL_BOOL_VEC3:
      case GL_UNSIGNED_INT_VEC3:
        return 3;
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_UNSIGNED_INT_VEC4:
      case GL_FLOAT_MAT2:
        return 4;
      case GL_FLOAT_MAT2x3:
      case GL_FLOAT_MAT3x2:
        return 6;
      case GL_FLOAT_MAT2x4:
      case GL_FLOAT_MAT4x2:
        return 8;
      case GL_FLOAT_MAT3:
        return 9;
      case GL_FLOAT_MAT3x4:
      case GL_FLOAT_MAT4x3:
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
      case GL_FLOAT_MAT2x3:
      case GL_FLOAT_MAT3x2:
      case GL_FLOAT_MAT2x4:
      case GL_FLOAT_MAT4x2:
      case GL_FLOAT_MAT3x4:
      case GL_FLOAT_MAT4x3:
        return GL_FLOAT;
      case GL_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_3D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_INT_VEC2:
      case GL_INT_VEC3:
      case GL_INT_VEC4:
        return GL_INT;
      case GL_UNSIGNED_INT:
      case GL_UNSIGNED_INT_VEC2:
      case GL_UNSIGNED_INT_VEC3:
      case GL_UNSIGNED_INT_VEC4:
        return GL_UNSIGNED_INT;
      default:
        UNREACHABLE();
    }

    return GL_NONE;
}

size_t UniformComponentSize(GLenum type)
{
    switch(type)
    {
      case GL_BOOL:         return sizeof(GLint);
      case GL_FLOAT:        return sizeof(GLfloat);
      case GL_INT:          return sizeof(GLint);
      case GL_UNSIGNED_INT: return sizeof(GLuint);
      default:       UNREACHABLE();
    }

    return 0;
}

size_t UniformInternalSize(GLenum type)
{
    // Expanded to 4-element vectors
    return UniformComponentSize(UniformComponentType(type)) * VariableRowCount(type) * 4;
}

size_t UniformExternalSize(GLenum type)
{
    return UniformComponentSize(UniformComponentType(type)) * UniformComponentCount(type);
}

GLenum UniformBoolVectorType(GLenum type)
{
    switch (type)
    {
      case GL_FLOAT:
      case GL_INT:
      case GL_UNSIGNED_INT:
        return GL_BOOL;
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_UNSIGNED_INT_VEC2:
        return GL_BOOL_VEC2;
      case GL_FLOAT_VEC3:
      case GL_INT_VEC3:
      case GL_UNSIGNED_INT_VEC3:
        return GL_BOOL_VEC3;
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_UNSIGNED_INT_VEC4:
        return GL_BOOL_VEC4;

      default:
        UNREACHABLE();
        return GL_NONE;
    }
}

int VariableRowCount(GLenum type)
{
    switch (type)
    {
      case GL_NONE:
      case GL_STRUCT_ANGLEX:
        return 0;
      case GL_BOOL:
      case GL_FLOAT:
      case GL_INT:
      case GL_UNSIGNED_INT:
      case GL_BOOL_VEC2:
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_UNSIGNED_INT_VEC2:
      case GL_BOOL_VEC3:
      case GL_FLOAT_VEC3:
      case GL_INT_VEC3:
      case GL_UNSIGNED_INT_VEC3:
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_UNSIGNED_INT_VEC4:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_3D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
        return 1;
      case GL_FLOAT_MAT2:
      case GL_FLOAT_MAT3x2:
      case GL_FLOAT_MAT4x2:
        return 2;
      case GL_FLOAT_MAT3:
      case GL_FLOAT_MAT2x3:
      case GL_FLOAT_MAT4x3:
        return 3;
      case GL_FLOAT_MAT4:
      case GL_FLOAT_MAT2x4:
      case GL_FLOAT_MAT3x4:
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
      case GL_STRUCT_ANGLEX:
        return 0;
      case GL_BOOL:
      case GL_FLOAT:
      case GL_INT:
      case GL_UNSIGNED_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_3D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
        return 1;
      case GL_BOOL_VEC2:
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_UNSIGNED_INT_VEC2:
      case GL_FLOAT_MAT2:
      case GL_FLOAT_MAT2x3:
      case GL_FLOAT_MAT2x4:
        return 2;
      case GL_BOOL_VEC3:
      case GL_FLOAT_VEC3:
      case GL_INT_VEC3:
      case GL_UNSIGNED_INT_VEC3:
      case GL_FLOAT_MAT3:
      case GL_FLOAT_MAT3x2:
      case GL_FLOAT_MAT3x4:
        return 3;
      case GL_BOOL_VEC4:
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_UNSIGNED_INT_VEC4:
      case GL_FLOAT_MAT4:
      case GL_FLOAT_MAT4x2:
      case GL_FLOAT_MAT4x3:
        return 4;
      default:
        UNREACHABLE();
    }

    return 0;
}

bool IsSampler(GLenum type)
{
    switch (type)
    {
      case GL_SAMPLER_2D:
      case GL_SAMPLER_3D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
        return true;
    }

    return false;
}

bool IsMatrixType(GLenum type)
{
    return VariableRowCount(type) > 1;
}

GLenum TransposeMatrixType(GLenum type)
{
    if (!IsMatrixType(type))
    {
        return type;
    }

    switch (type)
    {
      case GL_FLOAT_MAT2:   return GL_FLOAT_MAT2;
      case GL_FLOAT_MAT3:   return GL_FLOAT_MAT3;
      case GL_FLOAT_MAT4:   return GL_FLOAT_MAT4;
      case GL_FLOAT_MAT2x3: return GL_FLOAT_MAT3x2;
      case GL_FLOAT_MAT3x2: return GL_FLOAT_MAT2x3;
      case GL_FLOAT_MAT2x4: return GL_FLOAT_MAT4x2;
      case GL_FLOAT_MAT4x2: return GL_FLOAT_MAT2x4;
      case GL_FLOAT_MAT3x4: return GL_FLOAT_MAT4x3;
      case GL_FLOAT_MAT4x3: return GL_FLOAT_MAT3x4;
      default: UNREACHABLE(); return GL_NONE;
    }
}

int MatrixRegisterCount(GLenum type, bool isRowMajorMatrix)
{
    ASSERT(IsMatrixType(type));
    return isRowMajorMatrix ? VariableRowCount(type) : VariableColumnCount(type);
}

int MatrixComponentCount(GLenum type, bool isRowMajorMatrix)
{
    ASSERT(IsMatrixType(type));
    return isRowMajorMatrix ? VariableColumnCount(type) : VariableRowCount(type);
}

int AttributeRegisterCount(GLenum type)
{
    return IsMatrixType(type) ? VariableColumnCount(type) : 1;
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

bool IsCubemapTextureTarget(GLenum target)
{
    return (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
}

bool IsInternalTextureTarget(GLenum target, GLuint clientVersion)
{
    if (clientVersion == 2)
    {
        return target == GL_TEXTURE_2D || IsCubemapTextureTarget(target);
    }
    else if (clientVersion == 3)
    {
        return target == GL_TEXTURE_2D || IsCubemapTextureTarget(target) ||
               target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY;
    }
    else
    {
        UNREACHABLE();
        return false;
    }
}

bool IsTriangleMode(GLenum drawMode)
{
    switch (drawMode)
    {
      case GL_TRIANGLES:
      case GL_TRIANGLE_FAN:
      case GL_TRIANGLE_STRIP:
        return true;
      case GL_POINTS:
      case GL_LINES:
      case GL_LINE_LOOP:
      case GL_LINE_STRIP:
        return false;
      default: UNREACHABLE();
    }

    return false;
}

}

std::string getTempPath()
{
#if defined (_WIN32)
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
#else
    UNIMPLEMENTED();
    return "";
#endif
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
