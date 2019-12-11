//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL43.cpp: Validation functions for OpenGL 4.3 entry point parameters

#include "libANGLE/validationGL43_autogen.h"

namespace gl
{

bool ValidateClearBufferData(Context *context,
                             GLenum target,
                             GLenum internalformat,
                             GLenum format,
                             GLenum type,
                             const void *data)
{
    return true;
}

bool ValidateClearBufferSubData(Context *context,
                                GLenum target,
                                GLenum internalformat,
                                GLintptr offset,
                                GLsizeiptr size,
                                GLenum format,
                                GLenum type,
                                const void *data)
{
    return true;
}

bool ValidateCopyImageSubData(Context *context,
                              GLuint srcName,
                              GLenum srcTarget,
                              GLint srcLevel,
                              GLint srcX,
                              GLint srcY,
                              GLint srcZ,
                              GLuint dstName,
                              GLenum dstTarget,
                              GLint dstLevel,
                              GLint dstX,
                              GLint dstY,
                              GLint dstZ,
                              GLsizei srcWidth,
                              GLsizei srcHeight,
                              GLsizei srcDepth)
{
    return true;
}

bool ValidateDebugMessageCallback(Context *context, GLDEBUGPROC callback, const void *userParam)
{
    return true;
}

bool ValidateDebugMessageControl(Context *context,
                                 GLenum source,
                                 GLenum type,
                                 GLenum severity,
                                 GLsizei count,
                                 const GLuint *ids,
                                 GLboolean enabled)
{
    return true;
}

bool ValidateDebugMessageInsert(Context *context,
                                GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar *buf)
{
    return true;
}

bool ValidateGetDebugMessageLog(Context *context,
                                GLuint count,
                                GLsizei bufSize,
                                GLenum *sources,
                                GLenum *types,
                                GLuint *ids,
                                GLenum *severities,
                                GLsizei *lengths,
                                GLchar *messageLog)
{
    return true;
}

bool ValidateGetInternalformati64v(Context *context,
                                   GLenum target,
                                   GLenum internalformat,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLint64 *params)
{
    return true;
}

bool ValidateGetObjectLabel(Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLchar *label)
{
    return true;
}

bool ValidateGetObjectPtrLabel(Context *context,
                               const void *ptr,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLchar *label)
{
    return true;
}

bool ValidateGetProgramResourceLocationIndex(Context *context,
                                             ShaderProgramID program,
                                             GLenum programInterface,
                                             const GLchar *name)
{
    return true;
}

bool ValidateInvalidateBufferData(Context *context, BufferID buffer)
{
    return true;
}

bool ValidateInvalidateBufferSubData(Context *context,
                                     BufferID buffer,
                                     GLintptr offset,
                                     GLsizeiptr length)
{
    return true;
}

bool ValidateInvalidateTexImage(Context *context, TextureID texture, GLint level)
{
    return true;
}

bool ValidateInvalidateTexSubImage(Context *context,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth)
{
    return true;
}

bool ValidateMultiDrawArraysIndirect(Context *context,
                                     GLenum mode,
                                     const void *indirect,
                                     GLsizei drawcount,
                                     GLsizei stride)
{
    return true;
}

bool ValidateMultiDrawElementsIndirect(Context *context,
                                       GLenum mode,
                                       GLenum type,
                                       const void *indirect,
                                       GLsizei drawcount,
                                       GLsizei stride)
{
    return true;
}

bool ValidateObjectLabel(Context *context,
                         GLenum identifier,
                         GLuint name,
                         GLsizei length,
                         const GLchar *label)
{
    return true;
}

bool ValidateObjectPtrLabel(Context *context, const void *ptr, GLsizei length, const GLchar *label)
{
    return true;
}

bool ValidatePopDebugGroup(Context *context)
{
    return true;
}

bool ValidatePushDebugGroup(Context *context,
                            GLenum source,
                            GLuint id,
                            GLsizei length,
                            const GLchar *message)
{
    return true;
}

bool ValidateShaderStorageBlockBinding(Context *context,
                                       ShaderProgramID program,
                                       GLuint storageBlockIndex,
                                       GLuint storageBlockBinding)
{
    return true;
}

bool ValidateTexBufferRange(Context *context,
                            GLenum target,
                            GLenum internalformat,
                            BufferID buffer,
                            GLintptr offset,
                            GLsizeiptr size)
{
    return true;
}

bool ValidateTexStorage3DMultisample(Context *context,
                                     TextureType targetPacked,
                                     GLsizei samples,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTextureView(Context *context,
                         TextureID texture,
                         GLenum target,
                         GLuint origtexture,
                         GLenum internalformat,
                         GLuint minlevel,
                         GLuint numlevels,
                         GLuint minlayer,
                         GLuint numlayers)
{
    return true;
}

bool ValidateVertexAttribLFormat(Context *context,
                                 GLuint attribindex,
                                 GLint size,
                                 GLenum type,
                                 GLuint relativeoffset)
{
    return true;
}

}  // namespace gl
