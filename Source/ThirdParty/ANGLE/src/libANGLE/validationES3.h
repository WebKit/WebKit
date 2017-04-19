//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.h: Validation functions for OpenGL ES 3.0 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES3_H_
#define LIBANGLE_VALIDATION_ES3_H_

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
                                       const GLvoid *pixels);

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
                                     const GLvoid *pixels);

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
                                     const GLvoid *pixels);

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

bool ValidateFramebufferTextureLayer(Context *context, GLenum target, GLenum attachment,
                                     GLuint texture, GLint level, GLint layer);

bool ValidateInvalidateFramebuffer(Context *context, GLenum target, GLsizei numAttachments,
                                   const GLenum *attachments);

bool ValidateClearBuffer(ValidationContext *context);

bool ValidateDrawRangeElements(Context *context,
                               GLenum mode,
                               GLuint start,
                               GLuint end,
                               GLsizei count,
                               GLenum type,
                               const GLvoid *indices,
                               IndexRange *indexRange);

bool ValidateGetUniformuiv(Context *context, GLuint program, GLint location, GLuint* params);

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
                                  const GLvoid *data);

bool ValidateBindVertexArray(Context *context, GLuint array);
bool ValidateIsVertexArray(Context *context);

bool ValidateBindBufferBase(Context *context, GLenum target, GLuint index, GLuint buffer);
bool ValidateBindBufferRange(Context *context,
                             GLenum target,
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
                        const GLvoid *pixels);
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
                                   const GLvoid *pixels);
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
                           const GLvoid *pixels);
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
                                      const GLvoid *pixels);
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
                                     const GLvoid *data);

bool ValidateGenQueries(Context *context, GLint n, GLuint *ids);
bool ValidateDeleteQueries(Context *context, GLint n, const GLuint *ids);
bool ValidateGenSamplers(Context *context, GLint count, GLuint *samplers);
bool ValidateDeleteSamplers(Context *context, GLint count, const GLuint *samplers);
bool ValidateGenTransformFeedbacks(Context *context, GLint n, GLuint *ids);
bool ValidateDeleteTransformFeedbacks(Context *context, GLint n, const GLuint *ids);
bool ValidateGenVertexArrays(Context *context, GLint n, GLuint *arrays);
bool ValidateDeleteVertexArrays(Context *context, GLint n, const GLuint *arrays);

bool ValidateGenOrDeleteES3(Context *context, GLint n);
bool ValidateGenOrDeleteCountES3(Context *context, GLint count);

bool ValidateBeginTransformFeedback(Context *context, GLenum primitiveMode);

bool ValidateGetBufferPointerv(Context *context, GLenum target, GLenum pname, GLvoid **params);
bool ValidateGetBufferPointervRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLvoid **params);
bool ValidateUnmapBuffer(Context *context, GLenum target);
bool ValidateMapBufferRange(Context *context,
                            GLenum target,
                            GLintptr offset,
                            GLsizeiptr length,
                            GLbitfield access);
bool ValidateFlushMappedBufferRange(Context *context,
                                    GLenum target,
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
                               GLenum readTarget,
                               GLenum writeTarget,
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
                                  const GLvoid *pointer);

bool ValidateGetSynciv(Context *context,
                       GLsync sync,
                       GLenum pname,
                       GLsizei bufSize,
                       GLsizei *length,
                       GLint *values);

}  // namespace gl

#endif // LIBANGLE_VALIDATION_ES3_H_
