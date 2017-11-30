//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.h: Validation functions for OpenGL ES 3.0 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES3_H_
#define LIBANGLE_VALIDATION_ES3_H_

#include "libANGLE/PackedGLEnums.h"

#include <GLES3/gl3.h>

namespace gl
{
class Context;
struct IndexRange;
class ValidationContext;

bool ValidateES3TexImageParametersBase(ValidationContext *context,
                                       GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isCompressed,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei imageSize,
                                       const void *pixels);

bool ValidateES3TexStorageParameters(Context *context,
                                     GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth);

bool ValidateES3TexImage2DParameters(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei imageSize,
                                     const void *pixels);

bool ValidateES3TexImage3DParameters(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei bufSize,
                                     const void *pixels);

bool ValidateES3CopyTexImageParametersBase(ValidationContext *context,
                                           GLenum target,
                                           GLint level,
                                           GLenum internalformat,
                                           bool isSubImage,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border);

bool ValidateES3CopyTexImage2DParameters(ValidationContext *context,
                                         GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border);

bool ValidateES3CopyTexImage3DParameters(ValidationContext *context,
                                         GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border);

bool ValidateES3TexStorageParametersBase(Context *context,
                                         GLenum target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth);

bool ValidateES3TexStorage2DParameters(Context *context,
                                       GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth);

bool ValidateES3TexStorage3DParameters(Context *context,
                                       GLenum target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth);

bool ValidateBeginQuery(Context *context, GLenum target, GLuint id);

bool ValidateEndQuery(Context *context, GLenum target);

bool ValidateGetQueryiv(Context *context, GLenum target, GLenum pname, GLint *params);

bool ValidateGetQueryObjectuiv(Context *context, GLuint id, GLenum pname, GLuint *params);

bool ValidateFramebufferTextureLayer(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLuint texture,
                                     GLint level,
                                     GLint layer);

bool ValidateInvalidateFramebuffer(Context *context,
                                   GLenum target,
                                   GLsizei numAttachments,
                                   const GLenum *attachments);

bool ValidateInvalidateSubFramebuffer(Context *context,
                                      GLenum target,
                                      GLsizei numAttachments,
                                      const GLenum *attachments,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height);

bool ValidateClearBuffer(ValidationContext *context);

bool ValidateDrawRangeElements(Context *context,
                               GLenum mode,
                               GLuint start,
                               GLuint end,
                               GLsizei count,
                               GLenum type,
                               const void *indices);

bool ValidateGetUniformuiv(Context *context, GLuint program, GLint location, GLuint *params);

bool ValidateReadBuffer(Context *context, GLenum mode);

bool ValidateCompressedTexImage3D(Context *context,
                                  GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void *data);
bool ValidateCompressedTexImage3DRobustANGLE(Context *context,
                                             GLenum target,
                                             GLint level,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLint border,
                                             GLsizei imageSize,
                                             GLsizei dataSize,
                                             const void *data);

bool ValidateBindVertexArray(Context *context, GLuint array);
bool ValidateIsVertexArray(Context *context, GLuint array);

bool ValidateBindBufferBase(Context *context, BufferBinding target, GLuint index, GLuint buffer);
bool ValidateBindBufferRange(Context *context,
                             BufferBinding target,
                             GLuint index,
                             GLuint buffer,
                             GLintptr offset,
                             GLsizeiptr size);

bool ValidateProgramBinary(Context *context,
                           GLuint program,
                           GLenum binaryFormat,
                           const void *binary,
                           GLint length);
bool ValidateGetProgramBinary(Context *context,
                              GLuint program,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLenum *binaryFormat,
                              void *binary);
bool ValidateProgramParameteri(Context *context, GLuint program, GLenum pname, GLint value);
bool ValidateBlitFramebuffer(Context *context,
                             GLint srcX0,
                             GLint srcY0,
                             GLint srcX1,
                             GLint srcY1,
                             GLint dstX0,
                             GLint dstY0,
                             GLint dstX1,
                             GLint dstY1,
                             GLbitfield mask,
                             GLenum filter);
bool ValidateClearBufferiv(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLint *value);
bool ValidateClearBufferuiv(ValidationContext *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLuint *value);
bool ValidateClearBufferfv(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLfloat *value);
bool ValidateClearBufferfi(ValidationContext *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           GLfloat depth,
                           GLint stencil);
bool ValidateDrawBuffers(ValidationContext *context, GLsizei n, const GLenum *bufs);
bool ValidateCopyTexSubImage3D(Context *context,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height);
bool ValidateTexImage3D(Context *context,
                        GLenum target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const void *pixels);
bool ValidateTexImage3DRobustANGLE(Context *context,
                                   GLenum target,
                                   GLint level,
                                   GLint internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const void *pixels);
bool ValidateTexSubImage3D(Context *context,
                           GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint zoffset,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLenum format,
                           GLenum type,
                           const void *pixels);
bool ValidateTexSubImage3DRobustANGLE(Context *context,
                                      GLenum target,
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
                                      const void *pixels);
bool ValidateCompressedTexSubImage3D(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void *data);
bool ValidateCompressedTexSubImage3DRobustANGLE(Context *context,
                                                GLenum target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLint zoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLenum format,
                                                GLsizei imageSize,
                                                GLsizei dataSize,
                                                const void *data);

bool ValidateGenQueries(Context *context, GLint n, GLuint *ids);
bool ValidateDeleteQueries(Context *context, GLint n, const GLuint *ids);
bool ValidateGenSamplers(Context *context, GLint count, GLuint *samplers);
bool ValidateDeleteSamplers(Context *context, GLint count, const GLuint *samplers);
bool ValidateGenTransformFeedbacks(Context *context, GLint n, GLuint *ids);
bool ValidateDeleteTransformFeedbacks(Context *context, GLint n, const GLuint *ids);
bool ValidateGenVertexArrays(Context *context, GLint n, GLuint *arrays);
bool ValidateDeleteVertexArrays(Context *context, GLint n, const GLuint *arrays);

bool ValidateBeginTransformFeedback(Context *context, GLenum primitiveMode);

bool ValidateGetBufferPointerv(Context *context, BufferBinding target, GLenum pname, void **params);
bool ValidateGetBufferPointervRobustANGLE(Context *context,
                                          BufferBinding target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          void **params);
bool ValidateUnmapBuffer(Context *context, BufferBinding target);
bool ValidateMapBufferRange(Context *context,
                            BufferBinding target,
                            GLintptr offset,
                            GLsizeiptr length,
                            GLbitfield access);
bool ValidateFlushMappedBufferRange(Context *context,
                                    BufferBinding target,
                                    GLintptr offset,
                                    GLsizeiptr length);

bool ValidateIndexedStateQuery(ValidationContext *context,
                               GLenum pname,
                               GLuint index,
                               GLsizei *length);
bool ValidateGetIntegeri_v(ValidationContext *context, GLenum target, GLuint index, GLint *data);
bool ValidateGetIntegeri_vRobustANGLE(ValidationContext *context,
                                      GLenum target,
                                      GLuint index,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *data);
bool ValidateGetInteger64i_v(ValidationContext *context,
                             GLenum target,
                             GLuint index,
                             GLint64 *data);
bool ValidateGetInteger64i_vRobustANGLE(ValidationContext *context,
                                        GLenum target,
                                        GLuint index,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint64 *data);

bool ValidateCopyBufferSubData(ValidationContext *context,
                               BufferBinding readTarget,
                               BufferBinding writeTarget,
                               GLintptr readOffset,
                               GLintptr writeOffset,
                               GLsizeiptr size);

bool ValidateGetStringi(Context *context, GLenum name, GLuint index);
bool ValidateRenderbufferStorageMultisample(ValidationContext *context,
                                            GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height);

bool ValidateVertexAttribIPointer(ValidationContext *context,
                                  GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLsizei stride,
                                  const void *pointer);

bool ValidateGetSynciv(Context *context,
                       GLsync sync,
                       GLenum pname,
                       GLsizei bufSize,
                       GLsizei *length,
                       GLint *values);

bool ValidateDrawElementsInstanced(ValidationContext *context,
                                   GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const void *indices,
                                   GLsizei instanceCount);

bool ValidateFramebufferTextureMultiviewLayeredANGLE(Context *context,
                                                     GLenum target,
                                                     GLenum attachment,
                                                     GLuint texture,
                                                     GLint level,
                                                     GLint baseViewIndex,
                                                     GLsizei numViews);

bool ValidateFramebufferTextureMultiviewSideBySideANGLE(Context *context,
                                                        GLenum target,
                                                        GLenum attachment,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei numViews,
                                                        const GLint *viewportOffsets);

bool ValidateIsQuery(Context *context, GLuint id);

bool ValidateUniform1ui(Context *context, GLint location, GLuint v0);
bool ValidateUniform2ui(Context *context, GLint location, GLuint v0, GLuint v1);
bool ValidateUniform3ui(Context *context, GLint location, GLuint v0, GLuint v1, GLuint v2);
bool ValidateUniform4ui(Context *context,
                        GLint location,
                        GLuint v0,
                        GLuint v1,
                        GLuint v2,
                        GLuint v3);

bool ValidateUniform1uiv(Context *context, GLint location, GLsizei count, const GLuint *value);
bool ValidateUniform2uiv(Context *context, GLint location, GLsizei count, const GLuint *value);
bool ValidateUniform3uiv(Context *context, GLint location, GLsizei count, const GLuint *value);
bool ValidateUniform4uiv(Context *context, GLint location, GLsizei count, const GLuint *value);

bool ValidateUniformMatrix2x3fv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value);
bool ValidateUniformMatrix3x2fv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value);
bool ValidateUniformMatrix2x4fv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value);
bool ValidateUniformMatrix4x2fv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value);
bool ValidateUniformMatrix3x4fv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value);
bool ValidateUniformMatrix4x3fv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value);

bool ValidateEndTransformFeedback(Context *context);
bool ValidateTransformFeedbackVaryings(Context *context,
                                       GLuint program,
                                       GLsizei count,
                                       const GLchar *const *varyings,
                                       GLenum bufferMode);
bool ValidateGetTransformFeedbackVarying(Context *context,
                                         GLuint program,
                                         GLuint index,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLsizei *size,
                                         GLenum *type,
                                         GLchar *name);
bool ValidateBindTransformFeedback(Context *context, GLenum target, GLuint id);
bool ValidateIsTransformFeedback(Context *context, GLuint id);
bool ValidatePauseTransformFeedback(Context *context);
bool ValidateResumeTransformFeedback(Context *context);
bool ValidateVertexAttribI4i(Context *context, GLuint index, GLint x, GLint y, GLint z, GLint w);
bool ValidateVertexAttribI4ui(Context *context,
                              GLuint index,
                              GLuint x,
                              GLuint y,
                              GLuint z,
                              GLuint w);
bool ValidateVertexAttribI4iv(Context *context, GLuint index, const GLint *v);
bool ValidateVertexAttribI4uiv(Context *context, GLuint index, const GLuint *v);
bool ValidateGetFragDataLocation(Context *context, GLuint program, const GLchar *name);
bool ValidateGetUniformIndices(Context *context,
                               GLuint program,
                               GLsizei uniformCount,
                               const GLchar *const *uniformNames,
                               GLuint *uniformIndices);
bool ValidateGetActiveUniformsiv(Context *context,
                                 GLuint program,
                                 GLsizei uniformCount,
                                 const GLuint *uniformIndices,
                                 GLenum pname,
                                 GLint *params);
bool ValidateGetUniformBlockIndex(Context *context, GLuint program, const GLchar *uniformBlockName);
bool ValidateGetActiveUniformBlockiv(Context *context,
                                     GLuint program,
                                     GLuint uniformBlockIndex,
                                     GLenum pname,
                                     GLint *params);
bool ValidateGetActiveUniformBlockName(Context *context,
                                       GLuint program,
                                       GLuint uniformBlockIndex,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *uniformBlockName);
bool ValidateUniformBlockBinding(Context *context,
                                 GLuint program,
                                 GLuint uniformBlockIndex,
                                 GLuint uniformBlockBinding);
bool ValidateDrawArraysInstanced(Context *context,
                                 GLenum mode,
                                 GLint first,
                                 GLsizei count,
                                 GLsizei primcount);

bool ValidateFenceSync(Context *context, GLenum condition, GLbitfield flags);
bool ValidateIsSync(Context *context, GLsync sync);
bool ValidateDeleteSync(Context *context, GLsync sync);
bool ValidateClientWaitSync(Context *context, GLsync sync, GLbitfield flags, GLuint64 timeout);
bool ValidateWaitSync(Context *context, GLsync sync, GLbitfield flags, GLuint64 timeout);
bool ValidateGetInteger64v(Context *context, GLenum pname, GLint64 *params);

bool ValidateIsSampler(Context *context, GLuint sampler);
bool ValidateBindSampler(Context *context, GLuint unit, GLuint sampler);
bool ValidateVertexAttribDivisor(Context *context, GLuint index, GLuint divisor);
bool ValidateTexStorage2D(Context *context,
                          GLenum target,
                          GLsizei levels,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height);
bool ValidateTexStorage3D(Context *context,
                          GLenum target,
                          GLsizei levels,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth);

bool ValidateGetVertexAttribIiv(Context *context, GLuint index, GLenum pname, GLint *params);
bool ValidateGetVertexAttribIuiv(Context *context, GLuint index, GLenum pname, GLuint *params);
bool ValidateGetBufferParameteri64v(ValidationContext *context,
                                    BufferBinding target,
                                    GLenum pname,
                                    GLint64 *params);
bool ValidateSamplerParameteri(Context *context, GLuint sampler, GLenum pname, GLint param);
bool ValidateSamplerParameteriv(Context *context,
                                GLuint sampler,
                                GLenum pname,
                                const GLint *params);
bool ValidateSamplerParameterf(Context *context, GLuint sampler, GLenum pname, GLfloat param);
bool ValidateSamplerParameterfv(Context *context,
                                GLuint sampler,
                                GLenum pname,
                                const GLfloat *params);
bool ValidateGetSamplerParameteriv(Context *context, GLuint sampler, GLenum pname, GLint *params);
bool ValidateGetSamplerParameterfv(Context *context, GLuint sampler, GLenum pname, GLfloat *params);
bool ValidateGetInternalformativ(Context *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLint *params);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES3_H_
