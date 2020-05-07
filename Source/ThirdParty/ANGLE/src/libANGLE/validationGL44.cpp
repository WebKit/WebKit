//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL44.cpp: Validation functions for OpenGL 4.4 entry point parameters

#include "libANGLE/validationGL44_autogen.h"

namespace gl
{

bool ValidateBindBuffersBase(Context *context,
                             GLenum target,
                             GLuint first,
                             GLsizei count,
                             const BufferID *buffers)
{
    return true;
}

bool ValidateBindBuffersRange(Context *context,
                              GLenum target,
                              GLuint first,
                              GLsizei count,
                              const BufferID *buffers,
                              const GLintptr *offsets,
                              const GLsizeiptr *sizes)
{
    return true;
}

bool ValidateBindImageTextures(Context *context,
                               GLuint first,
                               GLsizei count,
                               const GLuint *textures)
{
    return true;
}

bool ValidateBindSamplers(Context *context, GLuint first, GLsizei count, const GLuint *samplers)
{
    return true;
}

bool ValidateBindTextures(Context *context, GLuint first, GLsizei count, const GLuint *textures)
{
    return true;
}

bool ValidateBindVertexBuffers(Context *context,
                               GLuint first,
                               GLsizei count,
                               const BufferID *buffers,
                               const GLintptr *offsets,
                               const GLsizei *strides)
{
    return true;
}

bool ValidateBufferStorage(Context *context,
                           GLenum target,
                           GLsizeiptr size,
                           const void *data,
                           GLbitfield flags)
{
    return true;
}

bool ValidateClearTexImage(Context *context,
                           TextureID texture,
                           GLint level,
                           GLenum format,
                           GLenum type,
                           const void *data)
{
    return true;
}

bool ValidateClearTexSubImage(Context *context,
                              TextureID texture,
                              GLint level,
                              GLint xoffset,
                              GLint yoffset,
                              GLint zoffset,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLenum format,
                              GLenum type,
                              const void *data)
{
    return true;
}

}  // namespace gl
