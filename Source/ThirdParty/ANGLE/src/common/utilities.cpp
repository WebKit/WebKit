//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// utilities.cpp: Conversion functions and other utility routines.

#include "common/utilities.h"
#include "common/mathutil.h"
#include "common/platform.h"

#include <set>

#if defined(ANGLE_ENABLE_WINDOWS_STORE)
#  include <wrl.h>
#  include <wrl/wrappers/corewrappers.h>
#  include <windows.applicationmodel.core.h>
#  include <windows.graphics.display.h>
#endif

namespace
{

template <class IndexType>
gl::IndexRange ComputeTypedIndexRange(const IndexType *indices,
                                      size_t count,
                                      bool primitiveRestartEnabled,
                                      GLuint primitiveRestartIndex)
{
    ASSERT(count > 0);

    IndexType minIndex                = 0;
    IndexType maxIndex                = 0;
    size_t nonPrimitiveRestartIndices = 0;

    if (primitiveRestartEnabled)
    {
        // Find the first non-primitive restart index to initialize the min and max values
        size_t i = 0;
        for (; i < count; i++)
        {
            if (indices[i] != primitiveRestartIndex)
            {
                minIndex = indices[i];
                maxIndex = indices[i];
                nonPrimitiveRestartIndices++;
                break;
            }
        }

        // Loop over the rest of the indices
        for (; i < count; i++)
        {
            if (indices[i] != primitiveRestartIndex)
            {
                if (minIndex > indices[i])
                {
                    minIndex = indices[i];
                }
                if (maxIndex < indices[i])
                {
                    maxIndex = indices[i];
                }
                nonPrimitiveRestartIndices++;
            }
        }
    }
    else
    {
        minIndex                   = indices[0];
        maxIndex                   = indices[0];
        nonPrimitiveRestartIndices = count;

        for (size_t i = 1; i < count; i++)
        {
            if (minIndex > indices[i])
            {
                minIndex = indices[i];
            }
            if (maxIndex < indices[i])
            {
                maxIndex = indices[i];
            }
        }
    }

    return gl::IndexRange(static_cast<size_t>(minIndex), static_cast<size_t>(maxIndex),
                          nonPrimitiveRestartIndices);
}

}  // anonymous namespace

namespace gl
{

int VariableComponentCount(GLenum type)
{
    return VariableRowCount(type) * VariableColumnCount(type);
}

GLenum VariableComponentType(GLenum type)
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

size_t VariableComponentSize(GLenum type)
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

size_t VariableInternalSize(GLenum type)
{
    // Expanded to 4-element vectors
    return VariableComponentSize(VariableComponentType(type)) * VariableRowCount(type) * 4;
}

size_t VariableExternalSize(GLenum type)
{
    return VariableComponentSize(VariableComponentType(type)) * VariableComponentCount(type);
}

GLenum VariableBoolVectorType(GLenum type)
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
      case GL_SAMPLER_EXTERNAL_OES:
      case GL_SAMPLER_2D_RECT_ARB:
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
      case GL_SAMPLER_EXTERNAL_OES:
      case GL_SAMPLER_2D_RECT_ARB:
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

bool IsSamplerType(GLenum type)
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

GLenum SamplerTypeToTextureType(GLenum samplerType)
{
    switch (samplerType)
    {
      case GL_SAMPLER_2D:
      case GL_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_SAMPLER_2D_SHADOW:
        return GL_TEXTURE_2D;

      case GL_SAMPLER_CUBE:
      case GL_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_SAMPLER_CUBE_SHADOW:
        return GL_TEXTURE_CUBE_MAP;

      case GL_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
        return GL_TEXTURE_2D_ARRAY;

      case GL_SAMPLER_3D:
      case GL_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
        return GL_TEXTURE_3D;

      default:
        UNREACHABLE();
        return 0;
    }
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

int VariableRegisterCount(GLenum type)
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

static_assert(GL_TEXTURE_CUBE_MAP_NEGATIVE_X - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 1, "Unexpected GL cube map enum value.");
static_assert(GL_TEXTURE_CUBE_MAP_POSITIVE_Y - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 2, "Unexpected GL cube map enum value.");
static_assert(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 3, "Unexpected GL cube map enum value.");
static_assert(GL_TEXTURE_CUBE_MAP_POSITIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 4, "Unexpected GL cube map enum value.");
static_assert(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 5, "Unexpected GL cube map enum value.");

bool IsCubeMapTextureTarget(GLenum target)
{
    return (target >= FirstCubeMapTextureTarget && target <= LastCubeMapTextureTarget);
}

size_t CubeMapTextureTargetToLayerIndex(GLenum target)
{
    ASSERT(IsCubeMapTextureTarget(target));
    return target - static_cast<size_t>(FirstCubeMapTextureTarget);
}

GLenum LayerIndexToCubeMapTextureTarget(size_t index)
{
    ASSERT(index <= (LastCubeMapTextureTarget - FirstCubeMapTextureTarget));
    return FirstCubeMapTextureTarget + static_cast<GLenum>(index);
}

IndexRange ComputeIndexRange(GLenum indexType,
                             const GLvoid *indices,
                             size_t count,
                             bool primitiveRestartEnabled)
{
    switch (indexType)
    {
        case GL_UNSIGNED_BYTE:
            return ComputeTypedIndexRange(static_cast<const GLubyte *>(indices), count,
                                          primitiveRestartEnabled,
                                          GetPrimitiveRestartIndex(indexType));
        case GL_UNSIGNED_SHORT:
            return ComputeTypedIndexRange(static_cast<const GLushort *>(indices), count,
                                          primitiveRestartEnabled,
                                          GetPrimitiveRestartIndex(indexType));
        case GL_UNSIGNED_INT:
            return ComputeTypedIndexRange(static_cast<const GLuint *>(indices), count,
                                          primitiveRestartEnabled,
                                          GetPrimitiveRestartIndex(indexType));
        default:
            UNREACHABLE();
            return IndexRange();
    }
}

GLuint GetPrimitiveRestartIndex(GLenum indexType)
{
    switch (indexType)
    {
        case GL_UNSIGNED_BYTE:
            return 0xFF;
        case GL_UNSIGNED_SHORT:
            return 0xFFFF;
        case GL_UNSIGNED_INT:
            return 0xFFFFFFFF;
        default:
            UNREACHABLE();
            return 0;
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

// [OpenGL ES SL 3.00.4] Section 11 p. 120
// Vertex Outs/Fragment Ins packing priorities
int VariableSortOrder(GLenum type)
{
    switch (type)
    {
      // 1. Arrays of mat4 and mat4
      // Non-square matrices of type matCxR consume the same space as a square
      // matrix of type matN where N is the greater of C and R
      case GL_FLOAT_MAT4:
      case GL_FLOAT_MAT2x4:
      case GL_FLOAT_MAT3x4:
      case GL_FLOAT_MAT4x2:
      case GL_FLOAT_MAT4x3:
        return 0;

      // 2. Arrays of mat2 and mat2 (since they occupy full rows)
      case GL_FLOAT_MAT2:
        return 1;

      // 3. Arrays of vec4 and vec4
      case GL_FLOAT_VEC4:
      case GL_INT_VEC4:
      case GL_BOOL_VEC4:
      case GL_UNSIGNED_INT_VEC4:
        return 2;

      // 4. Arrays of mat3 and mat3
      case GL_FLOAT_MAT3:
      case GL_FLOAT_MAT2x3:
      case GL_FLOAT_MAT3x2:
        return 3;

      // 5. Arrays of vec3 and vec3
      case GL_FLOAT_VEC3:
      case GL_INT_VEC3:
      case GL_BOOL_VEC3:
      case GL_UNSIGNED_INT_VEC3:
        return 4;

      // 6. Arrays of vec2 and vec2
      case GL_FLOAT_VEC2:
      case GL_INT_VEC2:
      case GL_BOOL_VEC2:
      case GL_UNSIGNED_INT_VEC2:
        return 5;

      // 7. Single component types
      case GL_FLOAT:
      case GL_INT:
      case GL_BOOL:
      case GL_UNSIGNED_INT:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_EXTERNAL_OES:
      case GL_SAMPLER_2D_RECT_ARB:
      case GL_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_3D:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
        return 6;

      default:
        UNREACHABLE();
        return 0;
    }
}

std::string ParseUniformName(const std::string &name, size_t *outSubscript)
{
    // Strip any trailing array operator and retrieve the subscript
    size_t open = name.find_last_of('[');
    size_t close = name.find_last_of(']');
    bool hasIndex = (open != std::string::npos) && (close == name.length() - 1);
    if (!hasIndex)
    {
        if (outSubscript)
        {
            *outSubscript = GL_INVALID_INDEX;
        }
        return name;
    }

    if (outSubscript)
    {
        int index = atoi(name.substr(open + 1).c_str());
        if (index >= 0)
        {
            *outSubscript = index;
        }
        else
        {
            *outSubscript = GL_INVALID_INDEX;
        }
    }

    return name.substr(0, open);
}

unsigned int ParseAndStripArrayIndex(std::string *name)
{
    unsigned int subscript = GL_INVALID_INDEX;

    // Strip any trailing array operator and retrieve the subscript
    size_t open  = name->find_last_of('[');
    size_t close = name->find_last_of(']');
    if (open != std::string::npos && close == name->length() - 1)
    {
        subscript = atoi(name->c_str() + open + 1);
        name->erase(open);
    }

    return subscript;
}

}  // namespace gl

namespace egl
{
static_assert(EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR == 1,
              "Unexpected EGL cube map enum value.");
static_assert(EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR == 2,
              "Unexpected EGL cube map enum value.");
static_assert(EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR == 3,
              "Unexpected EGL cube map enum value.");
static_assert(EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR == 4,
              "Unexpected EGL cube map enum value.");
static_assert(EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR == 5,
              "Unexpected EGL cube map enum value.");

bool IsCubeMapTextureTarget(EGLenum target)
{
    return (target >= FirstCubeMapTextureTarget && target <= LastCubeMapTextureTarget);
}

size_t CubeMapTextureTargetToLayerIndex(EGLenum target)
{
    ASSERT(IsCubeMapTextureTarget(target));
    return target - static_cast<size_t>(FirstCubeMapTextureTarget);
}

EGLenum LayerIndexToCubeMapTextureTarget(size_t index)
{
    ASSERT(index <= (LastCubeMapTextureTarget - FirstCubeMapTextureTarget));
    return FirstCubeMapTextureTarget + static_cast<GLenum>(index);
}

bool IsTextureTarget(EGLenum target)
{
    switch (target)
    {
        case EGL_GL_TEXTURE_2D_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
        case EGL_GL_TEXTURE_3D_KHR:
            return true;

        default:
            return false;
    }
}

bool IsRenderbufferTarget(EGLenum target)
{
    return target == EGL_GL_RENDERBUFFER_KHR;
}
}

namespace egl_gl
{
GLenum EGLCubeMapTargetToGLCubeMapTarget(EGLenum eglTarget)
{
    ASSERT(egl::IsCubeMapTextureTarget(eglTarget));
    return gl::LayerIndexToCubeMapTextureTarget(egl::CubeMapTextureTargetToLayerIndex(eglTarget));
}

GLenum EGLImageTargetToGLTextureTarget(EGLenum eglTarget)
{
    switch (eglTarget)
    {
        case EGL_GL_TEXTURE_2D_KHR:
            return GL_TEXTURE_2D;

        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
            return EGLCubeMapTargetToGLCubeMapTarget(eglTarget);

        case EGL_GL_TEXTURE_3D_KHR:
            return GL_TEXTURE_3D;

        default:
            UNREACHABLE();
            return GL_NONE;
    }
}

GLuint EGLClientBufferToGLObjectHandle(EGLClientBuffer buffer)
{
    return static_cast<GLuint>(reinterpret_cast<uintptr_t>(buffer));
}
}

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
std::string getTempPath()
{
#ifdef ANGLE_PLATFORM_WINDOWS
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
#endif // !ANGLE_ENABLE_WINDOWS_STORE

#if defined (ANGLE_PLATFORM_WINDOWS)

// Causes the thread to relinquish the remainder of its time slice to any
// other thread that is ready to run.If there are no other threads ready
// to run, the function returns immediately, and the thread continues execution.
void ScheduleYield()
{
    Sleep(0);
}

#endif
