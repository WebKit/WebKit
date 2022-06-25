//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL4.cpp: Validation functions for OpenGL 4.0 entry point parameters

#include "libANGLE/validationGL4_autogen.h"

namespace gl
{

bool ValidateBeginQueryIndexed(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum target,
                               GLuint index,
                               QueryID id)
{
    return true;
}

bool ValidateDrawTransformFeedback(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum mode,
                                   TransformFeedbackID id)
{
    return true;
}

bool ValidateDrawTransformFeedbackStream(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         GLenum mode,
                                         TransformFeedbackID id,
                                         GLuint stream)
{
    return true;
}

bool ValidateEndQueryIndexed(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLuint index)
{
    return true;
}

bool ValidateGetActiveSubroutineName(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     ShaderProgramID program,
                                     GLenum shadertype,
                                     GLuint index,
                                     GLsizei bufsize,
                                     const GLsizei *length,
                                     const GLchar *name)
{
    return true;
}

bool ValidateGetActiveSubroutineUniformName(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            ShaderProgramID program,
                                            GLenum shadertype,
                                            GLuint index,
                                            GLsizei bufsize,
                                            const GLsizei *length,
                                            const GLchar *name)
{
    return true;
}

bool ValidateGetActiveSubroutineUniformiv(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID program,
                                          GLenum shadertype,
                                          GLuint index,
                                          GLenum pname,
                                          const GLint *values)
{
    return true;
}

bool ValidateGetProgramStageiv(const Context *context,
                               angle::EntryPoint entryPoint,
                               ShaderProgramID program,
                               GLenum shadertype,
                               GLenum pname,
                               const GLint *values)
{
    return true;
}

bool ValidateGetQueryIndexediv(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum target,
                               GLuint index,
                               GLenum pname,
                               const GLint *params)
{
    return true;
}

bool ValidateGetSubroutineIndex(const Context *context,
                                angle::EntryPoint entryPoint,
                                ShaderProgramID program,
                                GLenum shadertype,
                                const GLchar *name)
{
    return true;
}

bool ValidateGetSubroutineUniformLocation(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          ShaderProgramID program,
                                          GLenum shadertype,
                                          const GLchar *name)
{
    return true;
}

bool ValidateGetUniformSubroutineuiv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLenum shadertype,
                                     GLint location,
                                     const GLuint *params)
{
    return true;
}

bool ValidateGetUniformdv(const Context *context,
                          angle::EntryPoint entryPoint,
                          ShaderProgramID program,
                          UniformLocation location,
                          const GLdouble *params)
{
    return true;
}

bool ValidatePatchParameterfv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum pname,
                              const GLfloat *values)
{
    return true;
}

bool ValidateUniform1d(const Context *context,
                       angle::EntryPoint entryPoint,
                       UniformLocation location,
                       GLdouble x)
{
    return true;
}

bool ValidateUniform1dv(const Context *context,
                        angle::EntryPoint entryPoint,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniform2d(const Context *context,
                       angle::EntryPoint entryPoint,
                       UniformLocation location,
                       GLdouble x,
                       GLdouble y)
{
    return true;
}

bool ValidateUniform2dv(const Context *context,
                        angle::EntryPoint entryPoint,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniform3d(const Context *context,
                       angle::EntryPoint entryPoint,
                       UniformLocation location,
                       GLdouble x,
                       GLdouble y,
                       GLdouble z)
{
    return true;
}

bool ValidateUniform3dv(const Context *context,
                        angle::EntryPoint entryPoint,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniform4d(const Context *context,
                       angle::EntryPoint entryPoint,
                       UniformLocation location,
                       GLdouble x,
                       GLdouble y,
                       GLdouble z,
                       GLdouble w)
{
    return true;
}

bool ValidateUniform4dv(const Context *context,
                        angle::EntryPoint entryPoint,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2x3dv(const Context *context,
                                angle::EntryPoint entryPoint,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2x4dv(const Context *context,
                                angle::EntryPoint entryPoint,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3x2dv(const Context *context,
                                angle::EntryPoint entryPoint,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3x4dv(const Context *context,
                                angle::EntryPoint entryPoint,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4x2dv(const Context *context,
                                angle::EntryPoint entryPoint,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4x3dv(const Context *context,
                                angle::EntryPoint entryPoint,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformSubroutinesuiv(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum shadertype,
                                   GLsizei count,
                                   const GLuint *indices)
{
    return true;
}

bool ValidateDepthRangeArrayv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint first,
                              GLsizei count,
                              const GLdouble *v)
{
    return true;
}

bool ValidateDepthRangeIndexed(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               GLdouble n,
                               GLdouble f)
{
    return true;
}

bool ValidateGetDoublei_v(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum target,
                          GLuint index,
                          const GLdouble *data)
{
    return true;
}

bool ValidateGetFloati_v(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum target,
                         GLuint index,
                         const GLfloat *data)
{
    return true;
}

bool ValidateGetVertexAttribLdv(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLuint index,
                                GLenum pname,
                                const GLdouble *params)
{
    return true;
}

bool ValidateProgramUniform1d(const Context *context,
                              angle::EntryPoint entryPoint,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0)
{
    return true;
}

bool ValidateProgramUniform1dv(const Context *context,
                               angle::EntryPoint entryPoint,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform2d(const Context *context,
                              angle::EntryPoint entryPoint,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0,
                              GLdouble v1)
{
    return true;
}

bool ValidateProgramUniform2dv(const Context *context,
                               angle::EntryPoint entryPoint,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform3d(const Context *context,
                              angle::EntryPoint entryPoint,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0,
                              GLdouble v1,
                              GLdouble v2)
{
    return true;
}

bool ValidateProgramUniform3dv(const Context *context,
                               angle::EntryPoint entryPoint,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform4d(const Context *context,
                              angle::EntryPoint entryPoint,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0,
                              GLdouble v1,
                              GLdouble v2,
                              GLdouble v3)
{
    return true;
}

bool ValidateProgramUniform4dv(const Context *context,
                               angle::EntryPoint entryPoint,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2dv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2x3dv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2x4dv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3dv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3x2dv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3x4dv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4dv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4x2dv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4x3dv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateScissorArrayv(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLuint first,
                           GLsizei count,
                           const GLint *v)
{
    return true;
}

bool ValidateScissorIndexed(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLuint index,
                            GLint left,
                            GLint bottom,
                            GLsizei width,
                            GLsizei height)
{
    return true;
}

bool ValidateScissorIndexedv(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             const GLint *v)
{
    return true;
}

bool ValidateVertexAttribL1d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLdouble x)
{
    return true;
}

bool ValidateVertexAttribL1dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL2d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLdouble x,
                             GLdouble y)
{
    return true;
}

bool ValidateVertexAttribL2dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL3d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLdouble x,
                             GLdouble y,
                             GLdouble z)
{
    return true;
}

bool ValidateVertexAttribL3dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL4d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLuint index,
                             GLdouble x,
                             GLdouble y,
                             GLdouble z,
                             GLdouble w)
{
    return true;
}

bool ValidateVertexAttribL4dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribLPointer(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLsizei stride,
                                  const void *pointer)
{
    return true;
}

bool ValidateViewportArrayv(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLuint first,
                            GLsizei count,
                            const GLfloat *v)
{
    return true;
}

bool ValidateViewportIndexedf(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint index,
                              GLfloat x,
                              GLfloat y,
                              GLfloat w,
                              GLfloat h)
{
    return true;
}

bool ValidateViewportIndexedfv(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint index,
                               const GLfloat *v)
{
    return true;
}

bool ValidateDrawArraysInstancedBaseInstance(const Context *context,
                                             angle::EntryPoint entryPoint,
                                             PrimitiveMode mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instancecount,
                                             GLuint baseinstance)
{
    return true;
}

bool ValidateDrawElementsInstancedBaseInstance(const Context *context,
                                               angle::EntryPoint entryPoint,
                                               GLenum mode,
                                               GLsizei count,
                                               GLenum type,
                                               const void *indices,
                                               GLsizei instancecount,
                                               GLuint baseinstance)
{
    return true;
}

bool ValidateDrawElementsInstancedBaseVertexBaseInstance(const Context *context,
                                                         angle::EntryPoint entryPoint,
                                                         PrimitiveMode mode,
                                                         GLsizei count,
                                                         DrawElementsType type,
                                                         const void *indices,
                                                         GLsizei instancecount,
                                                         GLint basevertex,
                                                         GLuint baseinstance)
{
    return true;
}

bool ValidateDrawTransformFeedbackInstanced(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            GLenum mode,
                                            TransformFeedbackID id,
                                            GLsizei instancecount)
{
    return true;
}

bool ValidateDrawTransformFeedbackStreamInstanced(const Context *context,
                                                  angle::EntryPoint entryPoint,
                                                  GLenum mode,
                                                  TransformFeedbackID id,
                                                  GLuint stream,
                                                  GLsizei instancecount)
{
    return true;
}

bool ValidateGetActiveAtomicCounterBufferiv(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            ShaderProgramID program,
                                            GLuint bufferIndex,
                                            GLenum pname,
                                            const GLint *params)
{
    return true;
}

bool ValidateTexStorage1D(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum target,
                          GLsizei levels,
                          GLenum internalformat,
                          GLsizei width)
{
    return true;
}

bool ValidateClearBufferData(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLenum internalformat,
                             GLenum format,
                             GLenum type,
                             const void *data)
{
    return true;
}

bool ValidateClearBufferSubData(const Context *context,
                                angle::EntryPoint entryPoint,
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

bool ValidateGetInternalformati64v(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum target,
                                   GLenum internalformat,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLint64 *params)
{
    return true;
}

bool ValidateGetProgramResourceLocationIndex(const Context *context,
                                             angle::EntryPoint entryPoint,
                                             ShaderProgramID program,
                                             GLenum programInterface,
                                             const GLchar *name)
{
    return true;
}

bool ValidateInvalidateBufferData(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  BufferID buffer)
{
    return true;
}

bool ValidateInvalidateBufferSubData(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     BufferID buffer,
                                     GLintptr offset,
                                     GLsizeiptr length)
{
    return true;
}

bool ValidateInvalidateTexImage(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureID texture,
                                GLint level)
{
    return true;
}

bool ValidateInvalidateTexSubImage(const Context *context,
                                   angle::EntryPoint entryPoint,
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

bool ValidateMultiDrawArraysIndirect(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     PrimitiveMode modePacked,
                                     const void *indirect,
                                     GLsizei drawcount,
                                     GLsizei stride)
{
    return true;
}

bool ValidateMultiDrawElementsIndirect(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       PrimitiveMode modePacked,
                                       DrawElementsType typePacked,
                                       const void *indirect,
                                       GLsizei drawcount,
                                       GLsizei stride)
{
    return true;
}

bool ValidateShaderStorageBlockBinding(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       ShaderProgramID program,
                                       GLuint storageBlockIndex,
                                       GLuint storageBlockBinding)
{
    return true;
}

bool ValidateTextureView(const Context *context,
                         angle::EntryPoint entryPoint,
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

bool ValidateVertexAttribLFormat(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLuint attribindex,
                                 GLint size,
                                 GLenum type,
                                 GLuint relativeoffset)
{
    return true;
}

bool ValidateBindBuffersBase(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLuint first,
                             GLsizei count,
                             const BufferID *buffers)
{
    return true;
}

bool ValidateBindBuffersRange(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              GLuint first,
                              GLsizei count,
                              const BufferID *buffers,
                              const GLintptr *offsets,
                              const GLsizeiptr *sizes)
{
    return true;
}

bool ValidateBindImageTextures(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint first,
                               GLsizei count,
                               const GLuint *textures)
{
    return true;
}

bool ValidateBindSamplers(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLuint first,
                          GLsizei count,
                          const GLuint *samplers)
{
    return true;
}

bool ValidateBindTextures(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLuint first,
                          GLsizei count,
                          const GLuint *textures)
{
    return true;
}

bool ValidateBindVertexBuffers(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint first,
                               GLsizei count,
                               const BufferID *buffers,
                               const GLintptr *offsets,
                               const GLsizei *strides)
{
    return true;
}

bool ValidateBufferStorage(const Context *context,
                           angle::EntryPoint entryPoint,
                           BufferBinding targetPacked,
                           GLsizeiptr size,
                           const void *data,
                           GLbitfield flags)
{
    return true;
}

bool ValidateClearTexImage(const Context *context,
                           angle::EntryPoint entryPoint,
                           TextureID texture,
                           GLint level,
                           GLenum format,
                           GLenum type,
                           const void *data)
{
    return true;
}

bool ValidateClearTexSubImage(const Context *context,
                              angle::EntryPoint entryPoint,
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

bool ValidateBindTextureUnit(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLuint unit,
                             TextureID texture)
{
    return true;
}

bool ValidateBlitNamedFramebuffer(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLuint readFramebuffer,
                                  GLuint drawFramebuffer,
                                  GLint srcX0,
                                  GLint srcY0,
                                  GLint srcX1,
                                  GLint srcY1,
                                  GLint dstX0,
                                  GLint dstY0,
                                  GLint dstX1,
                                  GLint dstY1,
                                  GLbitfield mask,
                                  GLenum filter)
{
    return true;
}

bool ValidateCheckNamedFramebufferStatus(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         FramebufferID framebuffer,
                                         GLenum target)
{
    return true;
}

bool ValidateClearNamedBufferData(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  BufferID buffer,
                                  GLenum internalformat,
                                  GLenum format,
                                  GLenum type,
                                  const void *data)
{
    return true;
}

bool ValidateClearNamedBufferSubData(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     BufferID buffer,
                                     GLenum internalformat,
                                     GLintptr offset,
                                     GLsizeiptr size,
                                     GLenum format,
                                     GLenum type,
                                     const void *data)
{
    return true;
}

bool ValidateClearNamedFramebufferfi(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     GLfloat depth,
                                     GLint stencil)
{
    return true;
}

bool ValidateClearNamedFramebufferfv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLfloat *value)
{
    return true;
}

bool ValidateClearNamedFramebufferiv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     FramebufferID framebuffer,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLint *value)
{
    return true;
}

bool ValidateClearNamedFramebufferuiv(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      FramebufferID framebuffer,
                                      GLenum buffer,
                                      GLint drawbuffer,
                                      const GLuint *value)
{
    return true;
}

bool ValidateClipControl(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLenum origin,
                         GLenum depth)
{
    return true;
}

bool ValidateCompressedTextureSubImage1D(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         TextureID texture,
                                         GLint level,
                                         GLint xoffset,
                                         GLsizei width,
                                         GLenum format,
                                         GLsizei imageSize,
                                         const void *data)
{
    return true;
}

bool ValidateCompressedTextureSubImage2D(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         TextureID texture,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLsizei imageSize,
                                         const void *data)
{
    return true;
}

bool ValidateCompressedTextureSubImage3D(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         TextureID texture,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLenum format,
                                         GLsizei imageSize,
                                         const void *data)
{
    return true;
}

bool ValidateCopyNamedBufferSubData(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLuint readBuffer,
                                    GLuint writeBuffer,
                                    GLintptr readOffset,
                                    GLintptr writeOffset,
                                    GLsizeiptr size)
{
    return true;
}

bool ValidateCopyTextureSubImage1D(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width)
{
    return true;
}

bool ValidateCopyTextureSubImage2D(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height)
{
    return true;
}

bool ValidateCopyTextureSubImage3D(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureID texture,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height)
{
    return true;
}

bool ValidateCreateBuffers(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLsizei n,
                           const BufferID *buffers)
{
    return true;
}

bool ValidateCreateFramebuffers(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLsizei n,
                                const GLuint *framebuffers)
{
    return true;
}

bool ValidateCreateProgramPipelines(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLsizei n,
                                    const GLuint *pipelines)
{
    return true;
}

bool ValidateCreateQueries(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum target,
                           GLsizei n,
                           const GLuint *ids)
{
    return true;
}

bool ValidateCreateRenderbuffers(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLsizei n,
                                 const RenderbufferID *renderbuffers)
{
    return true;
}

bool ValidateCreateSamplers(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLsizei n,
                            const GLuint *samplers)
{
    return true;
}

bool ValidateCreateTextures(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLenum target,
                            GLsizei n,
                            const GLuint *textures)
{
    return true;
}

bool ValidateCreateTransformFeedbacks(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      GLsizei n,
                                      const GLuint *ids)
{
    return true;
}

bool ValidateCreateVertexArrays(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLsizei n,
                                const VertexArrayID *arrays)
{
    return true;
}

bool ValidateDisableVertexArrayAttrib(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      VertexArrayID vaobj,
                                      GLuint index)
{
    return true;
}

bool ValidateEnableVertexArrayAttrib(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     VertexArrayID vaobj,
                                     GLuint index)
{
    return true;
}

bool ValidateFlushMappedNamedBufferRange(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         BufferID buffer,
                                         GLintptr offset,
                                         GLsizeiptr length)
{
    return true;
}

bool ValidateGenerateTextureMipmap(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureID texture)
{
    return true;
}

bool ValidateGetCompressedTextureImage(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       TextureID texture,
                                       GLint level,
                                       GLsizei bufSize,
                                       const void *pixels)
{
    return true;
}

bool ValidateGetCompressedTextureSubImage(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          TextureID texture,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLint zoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLsizei bufSize,
                                          const void *pixels)
{
    return true;
}

bool ValidateGetNamedBufferParameteri64v(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         BufferID buffer,
                                         GLenum pname,
                                         const GLint64 *params)
{
    return true;
}

bool ValidateGetNamedBufferParameteriv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       BufferID buffer,
                                       GLenum pname,
                                       const GLint *params)
{
    return true;
}

bool ValidateGetNamedBufferPointerv(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    BufferID buffer,
                                    GLenum pname,
                                    void *const *params)
{
    return true;
}

bool ValidateGetNamedBufferSubData(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   BufferID buffer,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void *data)
{
    return true;
}

bool ValidateGetNamedFramebufferAttachmentParameteriv(const Context *context,
                                                      angle::EntryPoint entryPoint,
                                                      FramebufferID framebuffer,
                                                      GLenum attachment,
                                                      GLenum pname,
                                                      const GLint *params)
{
    return true;
}

bool ValidateGetNamedFramebufferParameteriv(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            FramebufferID framebuffer,
                                            GLenum pname,
                                            const GLint *param)
{
    return true;
}

bool ValidateGetNamedRenderbufferParameteriv(const Context *context,
                                             angle::EntryPoint entryPoint,
                                             RenderbufferID renderbuffer,
                                             GLenum pname,
                                             const GLint *params)
{
    return true;
}

bool ValidateGetQueryBufferObjecti64v(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      GLuint id,
                                      BufferID buffer,
                                      GLenum pname,
                                      GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectiv(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLuint id,
                                    BufferID buffer,
                                    GLenum pname,
                                    GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectui64v(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLuint id,
                                       BufferID buffer,
                                       GLenum pname,
                                       GLintptr offset)
{
    return true;
}

bool ValidateGetQueryBufferObjectuiv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLuint id,
                                     BufferID buffer,
                                     GLenum pname,
                                     GLintptr offset)
{
    return true;
}

bool ValidateGetTextureImage(const Context *context,
                             angle::EntryPoint entryPoint,
                             TextureID texture,
                             GLint level,
                             GLenum format,
                             GLenum type,
                             GLsizei bufSize,
                             const void *pixels)
{
    return true;
}

bool ValidateGetTextureLevelParameterfv(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        TextureID texture,
                                        GLint level,
                                        GLenum pname,
                                        const GLfloat *params)
{
    return true;
}

bool ValidateGetTextureLevelParameteriv(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        TextureID texture,
                                        GLint level,
                                        GLenum pname,
                                        const GLint *params)
{
    return true;
}

bool ValidateGetTextureParameterIiv(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    TextureID texture,
                                    GLenum pname,
                                    const GLint *params)
{
    return true;
}

bool ValidateGetTextureParameterIuiv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     TextureID texture,
                                     GLenum pname,
                                     const GLuint *params)
{
    return true;
}

bool ValidateGetTextureParameterfv(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureID texture,
                                   GLenum pname,
                                   const GLfloat *params)
{
    return true;
}

bool ValidateGetTextureParameteriv(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureID texture,
                                   GLenum pname,
                                   const GLint *params)
{
    return true;
}

bool ValidateGetTextureSubImage(const Context *context,
                                angle::EntryPoint entryPoint,
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
                                GLsizei bufSize,
                                const void *pixels)
{
    return true;
}

bool ValidateGetTransformFeedbacki64_v(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       GLuint xfb,
                                       GLenum pname,
                                       GLuint index,
                                       const GLint64 *param)
{
    return true;
}

bool ValidateGetTransformFeedbacki_v(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLuint xfb,
                                     GLenum pname,
                                     GLuint index,
                                     const GLint *param)
{
    return true;
}

bool ValidateGetTransformFeedbackiv(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLuint xfb,
                                    GLenum pname,
                                    const GLint *param)
{
    return true;
}

bool ValidateGetVertexArrayIndexed64iv(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       VertexArrayID vaobj,
                                       GLuint index,
                                       GLenum pname,
                                       const GLint64 *param)
{
    return true;
}

bool ValidateGetVertexArrayIndexediv(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     VertexArrayID vaobj,
                                     GLuint index,
                                     GLenum pname,
                                     const GLint *param)
{
    return true;
}

bool ValidateGetVertexArrayiv(const Context *context,
                              angle::EntryPoint entryPoint,
                              VertexArrayID vaobj,
                              GLenum pname,
                              const GLint *param)
{
    return true;
}

bool ValidateGetnColorTable(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLenum target,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            const void *table)
{
    return true;
}

bool ValidateGetnCompressedTexImage(const Context *context,
                                    angle::EntryPoint entryPoint,
                                    GLenum target,
                                    GLint lod,
                                    GLsizei bufSize,
                                    const void *pixels)
{
    return true;
}

bool ValidateGetnConvolutionFilter(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLenum target,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const void *image)
{
    return true;
}

bool ValidateGetnHistogram(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum target,
                           GLboolean reset,
                           GLenum format,
                           GLenum type,
                           GLsizei bufSize,
                           const void *values)
{
    return true;
}

bool ValidateGetnMapdv(const Context *context,
                       angle::EntryPoint entryPoint,
                       GLenum target,
                       GLenum query,
                       GLsizei bufSize,
                       const GLdouble *v)
{
    return true;
}

bool ValidateGetnMapfv(const Context *context,
                       angle::EntryPoint entryPoint,
                       GLenum target,
                       GLenum query,
                       GLsizei bufSize,
                       const GLfloat *v)
{
    return true;
}

bool ValidateGetnMapiv(const Context *context,
                       angle::EntryPoint entryPoint,
                       GLenum target,
                       GLenum query,
                       GLsizei bufSize,
                       const GLint *v)
{
    return true;
}

bool ValidateGetnMinmax(const Context *context,
                        angle::EntryPoint entryPoint,
                        GLenum target,
                        GLboolean reset,
                        GLenum format,
                        GLenum type,
                        GLsizei bufSize,
                        const void *values)
{
    return true;
}

bool ValidateGetnPixelMapfv(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLenum map,
                            GLsizei bufSize,
                            const GLfloat *values)
{
    return true;
}

bool ValidateGetnPixelMapuiv(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum map,
                             GLsizei bufSize,
                             const GLuint *values)
{
    return true;
}

bool ValidateGetnPixelMapusv(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum map,
                             GLsizei bufSize,
                             const GLushort *values)
{
    return true;
}

bool ValidateGetnPolygonStipple(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLsizei bufSize,
                                const GLubyte *pattern)
{
    return true;
}

bool ValidateGetnSeparableFilter(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLenum target,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei rowBufSize,
                                 const void *row,
                                 GLsizei columnBufSize,
                                 const void *column,
                                 const void *span)
{
    return true;
}

bool ValidateGetnTexImage(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum target,
                          GLint level,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          const void *pixels)
{
    return true;
}

bool ValidateGetnUniformdv(const Context *context,
                           angle::EntryPoint entryPoint,
                           ShaderProgramID program,
                           UniformLocation location,
                           GLsizei bufSize,
                           const GLdouble *params)
{
    return true;
}

bool ValidateInvalidateNamedFramebufferData(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            FramebufferID framebuffer,
                                            GLsizei numAttachments,
                                            const GLenum *attachments)
{
    return true;
}

bool ValidateInvalidateNamedFramebufferSubData(const Context *context,
                                               angle::EntryPoint entryPoint,
                                               FramebufferID framebuffer,
                                               GLsizei numAttachments,
                                               const GLenum *attachments,
                                               GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height)
{
    return true;
}

bool ValidateMapNamedBuffer(const Context *context,
                            angle::EntryPoint entryPoint,
                            BufferID buffer,
                            GLenum access)
{
    return true;
}

bool ValidateMapNamedBufferRange(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 BufferID buffer,
                                 GLintptr offset,
                                 GLsizeiptr length,
                                 GLbitfield access)
{
    return true;
}

bool ValidateNamedBufferData(const Context *context,
                             angle::EntryPoint entryPoint,
                             BufferID buffer,
                             GLsizeiptr size,
                             const void *data,
                             GLenum usage)
{
    return true;
}

bool ValidateNamedBufferStorage(const Context *context,
                                angle::EntryPoint entryPoint,
                                BufferID buffer,
                                GLsizeiptr size,
                                const void *data,
                                GLbitfield flags)
{
    return true;
}

bool ValidateNamedBufferSubData(const Context *context,
                                angle::EntryPoint entryPoint,
                                BufferID buffer,
                                GLintptr offset,
                                GLsizeiptr size,
                                const void *data)
{
    return true;
}

bool ValidateNamedFramebufferDrawBuffer(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        FramebufferID framebuffer,
                                        GLenum buf)
{
    return true;
}

bool ValidateNamedFramebufferDrawBuffers(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         FramebufferID framebuffer,
                                         GLsizei n,
                                         const GLenum *bufs)
{
    return true;
}

bool ValidateNamedFramebufferParameteri(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        FramebufferID framebuffer,
                                        GLenum pname,
                                        GLint param)
{
    return true;
}

bool ValidateNamedFramebufferReadBuffer(const Context *context,
                                        angle::EntryPoint entryPoint,
                                        FramebufferID framebuffer,
                                        GLenum src)
{
    return true;
}

bool ValidateNamedFramebufferRenderbuffer(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          FramebufferID framebuffer,
                                          GLenum attachment,
                                          GLenum renderbuffertarget,
                                          RenderbufferID renderbuffer)
{
    return true;
}

bool ValidateNamedFramebufferTexture(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     FramebufferID framebuffer,
                                     GLenum attachment,
                                     TextureID texture,
                                     GLint level)
{
    return true;
}

bool ValidateNamedFramebufferTextureLayer(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          FramebufferID framebuffer,
                                          GLenum attachment,
                                          TextureID texture,
                                          GLint level,
                                          GLint layer)
{
    return true;
}

bool ValidateNamedRenderbufferStorage(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      RenderbufferID renderbuffer,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height)
{
    return true;
}

bool ValidateNamedRenderbufferStorageMultisample(const Context *context,
                                                 angle::EntryPoint entryPoint,
                                                 RenderbufferID renderbuffer,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height)
{
    return true;
}

bool ValidateTextureBarrier(const Context *context, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidateTextureBuffer(const Context *context,
                           angle::EntryPoint entryPoint,
                           TextureID texture,
                           GLenum internalformat,
                           BufferID buffer)
{
    return true;
}

bool ValidateTextureBufferRange(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureID texture,
                                GLenum internalformat,
                                BufferID buffer,
                                GLintptr offset,
                                GLsizeiptr size)
{
    return true;
}

bool ValidateTextureParameterIiv(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 TextureID texture,
                                 GLenum pname,
                                 const GLint *params)
{
    return true;
}

bool ValidateTextureParameterIuiv(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  TextureID texture,
                                  GLenum pname,
                                  const GLuint *params)
{
    return true;
}

bool ValidateTextureParameterf(const Context *context,
                               angle::EntryPoint entryPoint,
                               TextureID texture,
                               GLenum pname,
                               GLfloat param)
{
    return true;
}

bool ValidateTextureParameterfv(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureID texture,
                                GLenum pname,
                                const GLfloat *param)
{
    return true;
}

bool ValidateTextureParameteri(const Context *context,
                               angle::EntryPoint entryPoint,
                               TextureID texture,
                               GLenum pname,
                               GLint param)
{
    return true;
}

bool ValidateTextureParameteriv(const Context *context,
                                angle::EntryPoint entryPoint,
                                TextureID texture,
                                GLenum pname,
                                const GLint *param)
{
    return true;
}

bool ValidateTextureStorage1D(const Context *context,
                              angle::EntryPoint entryPoint,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width)
{
    return true;
}

bool ValidateTextureStorage2D(const Context *context,
                              angle::EntryPoint entryPoint,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height)
{
    return true;
}

bool ValidateTextureStorage2DMultisample(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         TextureID texture,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTextureStorage3D(const Context *context,
                              angle::EntryPoint entryPoint,
                              TextureID texture,
                              GLsizei levels,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth)
{
    return true;
}

bool ValidateTextureStorage3DMultisample(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         TextureID texture,
                                         GLsizei samples,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTextureSubImage1D(const Context *context,
                               angle::EntryPoint entryPoint,
                               TextureID texture,
                               GLint level,
                               GLint xoffset,
                               GLsizei width,
                               GLenum format,
                               GLenum type,
                               const void *pixels)
{
    return true;
}

bool ValidateTextureSubImage2D(const Context *context,
                               angle::EntryPoint entryPoint,
                               TextureID texture,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               const void *pixels)
{
    return true;
}

bool ValidateTextureSubImage3D(const Context *context,
                               angle::EntryPoint entryPoint,
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
                               const void *pixels)
{
    return true;
}

bool ValidateTransformFeedbackBufferBase(const Context *context,
                                         angle::EntryPoint entryPoint,
                                         GLuint xfb,
                                         GLuint index,
                                         BufferID buffer)
{
    return true;
}

bool ValidateTransformFeedbackBufferRange(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          GLuint xfb,
                                          GLuint index,
                                          BufferID buffer,
                                          GLintptr offset,
                                          GLsizeiptr size)
{
    return true;
}

bool ValidateUnmapNamedBuffer(const Context *context, angle::EntryPoint entryPoint, BufferID buffer)
{
    return true;
}

bool ValidateVertexArrayAttribBinding(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLuint bindingindex)
{
    return true;
}

bool ValidateVertexArrayAttribFormat(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     VertexArrayID vaobj,
                                     GLuint attribindex,
                                     GLint size,
                                     GLenum type,
                                     GLboolean normalized,
                                     GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayAttribIFormat(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayAttribLFormat(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      VertexArrayID vaobj,
                                      GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLuint relativeoffset)
{
    return true;
}

bool ValidateVertexArrayBindingDivisor(const Context *context,
                                       angle::EntryPoint entryPoint,
                                       VertexArrayID vaobj,
                                       GLuint bindingindex,
                                       GLuint divisor)
{
    return true;
}

bool ValidateVertexArrayElementBuffer(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      VertexArrayID vaobj,
                                      BufferID buffer)
{
    return true;
}

bool ValidateVertexArrayVertexBuffer(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     VertexArrayID vaobj,
                                     GLuint bindingindex,
                                     BufferID buffer,
                                     GLintptr offset,
                                     GLsizei stride)
{
    return true;
}

bool ValidateVertexArrayVertexBuffers(const Context *context,
                                      angle::EntryPoint entryPoint,
                                      VertexArrayID vaobj,
                                      GLuint first,
                                      GLsizei count,
                                      const BufferID *buffers,
                                      const GLintptr *offsets,
                                      const GLsizei *strides)
{
    return true;
}

bool ValidateMultiDrawArraysIndirectCount(const Context *context,
                                          angle::EntryPoint entryPoint,
                                          GLenum mode,
                                          const void *indirect,
                                          GLintptr drawcount,
                                          GLsizei maxdrawcount,
                                          GLsizei stride)
{
    return true;
}

bool ValidateMultiDrawElementsIndirectCount(const Context *context,
                                            angle::EntryPoint entryPoint,
                                            GLenum mode,
                                            GLenum type,
                                            const void *indirect,
                                            GLintptr drawcount,
                                            GLsizei maxdrawcount,
                                            GLsizei stride)
{
    return true;
}

bool ValidatePolygonOffsetClamp(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLfloat factor,
                                GLfloat units,
                                GLfloat clamp)
{
    return true;
}

bool ValidateSpecializeShader(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLuint shader,
                              const GLchar *pEntryPoint,
                              GLuint numSpecializationConstants,
                              const GLuint *pConstantIndex,
                              const GLuint *pConstantValue)
{
    return true;
}

}  // namespace gl
