//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES.h: Validation functions for generic OpenGL ES entry point parameters

#ifndef LIBANGLE_VALIDATION_ES_H_
#define LIBANGLE_VALIDATION_ES_H_

#include "common/mathutil.h"

#include <GLES2/gl2.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

namespace egl
{
class Display;
class Image;
}

namespace gl
{
class Context;
struct Format;
class Program;
class Shader;
class ValidationContext;

bool ValidTextureTarget(const ValidationContext *context, GLenum target);
bool ValidTexture2DTarget(const ValidationContext *context, GLenum target);
bool ValidTexture3DTarget(const ValidationContext *context, GLenum target);
bool ValidTextureExternalTarget(const ValidationContext *context, GLenum target);
bool ValidTexture2DDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidTexture3DDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidFramebufferTarget(GLenum target);
bool ValidBufferTarget(const ValidationContext *context, GLenum target);
bool ValidBufferParameter(const ValidationContext *context, GLenum pname, GLsizei *numParams);
bool ValidMipLevel(const ValidationContext *context, GLenum target, GLint level);
bool ValidImageSizeParameters(const Context *context,
                              GLenum target,
                              GLint level,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              bool isSubImage);
bool ValidCompressedImageSize(const ValidationContext *context,
                              GLenum internalFormat,
                              GLsizei width,
                              GLsizei height);
bool ValidImageDataSize(ValidationContext *context,
                        GLenum textureTarget,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLenum internalFormat,
                        GLenum type,
                        const GLvoid *pixels,
                        GLsizei imageSize);

bool ValidQueryType(const Context *context, GLenum queryType);

// Returns valid program if id is a valid program name
// Errors INVALID_OPERATION if valid shader is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Program *GetValidProgram(ValidationContext *context, GLuint id);

// Returns valid shader if id is a valid shader name
// Errors INVALID_OPERATION if valid program is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Shader *GetValidShader(ValidationContext *context, GLuint id);

bool ValidateAttachmentTarget(Context *context, GLenum attachment);
bool ValidateRenderbufferStorageParametersBase(Context *context, GLenum target, GLsizei samples,
                                               GLenum internalformat, GLsizei width, GLsizei height);
bool ValidateRenderbufferStorageParametersANGLE(Context *context, GLenum target, GLsizei samples,
                                                GLenum internalformat, GLsizei width, GLsizei height);

bool ValidateFramebufferRenderbufferParameters(Context *context, GLenum target, GLenum attachment,
                                               GLenum renderbuffertarget, GLuint renderbuffer);

bool ValidateBlitFramebufferParameters(ValidationContext *context,
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

bool ValidateReadPixels(ValidationContext *context,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        GLvoid *pixels);
bool ValidateReadPixelsRobustANGLE(ValidationContext *context,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLvoid *pixels);
bool ValidateReadnPixelsEXT(Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            GLvoid *pixels);
bool ValidateReadnPixelsRobustANGLE(ValidationContext *context,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLvoid *data);

bool ValidateGenQueriesEXT(gl::Context *context, GLsizei n);
bool ValidateDeleteQueriesEXT(gl::Context *context, GLsizei n);
bool ValidateBeginQueryBase(Context *context, GLenum target, GLuint id);
bool ValidateBeginQueryEXT(Context *context, GLenum target, GLuint id);
bool ValidateEndQueryBase(Context *context, GLenum target);
bool ValidateEndQueryEXT(Context *context, GLenum target);
bool ValidateQueryCounterEXT(Context *context, GLuint id, GLenum target);
bool ValidateGetQueryivBase(Context *context, GLenum target, GLenum pname, GLsizei *numParams);
bool ValidateGetQueryivEXT(Context *context, GLenum target, GLenum pname, GLint *params);
bool ValidateGetQueryivRobustANGLE(Context *context,
                                   GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params);
bool ValidateGetQueryObjectValueBase(Context *context,
                                     GLenum target,
                                     GLenum pname,
                                     GLsizei *numParams);
bool ValidateGetQueryObjectivEXT(Context *context, GLuint id, GLenum pname, GLint *params);
bool ValidateGetQueryObjectivRobustANGLE(Context *context,
                                         GLuint id,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params);
bool ValidateGetQueryObjectuivEXT(Context *context, GLuint id, GLenum pname, GLuint *params);
bool ValidateGetQueryObjectuivRobustANGLE(Context *context,
                                          GLuint id,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLuint *params);
bool ValidateGetQueryObjecti64vEXT(Context *context, GLuint id, GLenum pname, GLint64 *params);
bool ValidateGetQueryObjecti64vRobustANGLE(Context *context,
                                           GLuint id,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint64 *params);
bool ValidateGetQueryObjectui64vEXT(Context *context, GLuint id, GLenum pname, GLuint64 *params);
bool ValidateGetQueryObjectui64vRobustANGLE(Context *context,
                                            GLuint id,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint64 *params);

bool ValidateUniform(Context *context, GLenum uniformType, GLint location, GLsizei count);
bool ValidateUniformMatrix(Context *context, GLenum matrixType, GLint location, GLsizei count,
                           GLboolean transpose);

bool ValidateStateQuery(ValidationContext *context,
                        GLenum pname,
                        GLenum *nativeType,
                        unsigned int *numParams);

bool ValidateRobustStateQuery(ValidationContext *context,
                              GLenum pname,
                              GLsizei bufSize,
                              GLenum *nativeType,
                              unsigned int *numParams);

bool ValidateCopyTexImageParametersBase(ValidationContext *context,
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
                                        GLint border,
                                        Format *textureFormatOut);

bool ValidateDrawArrays(ValidationContext *context,
                        GLenum mode,
                        GLint first,
                        GLsizei count,
                        GLsizei primcount);
bool ValidateDrawArraysInstanced(Context *context, GLenum mode, GLint first, GLsizei count, GLsizei primcount);
bool ValidateDrawArraysInstancedANGLE(Context *context, GLenum mode, GLint first, GLsizei count, GLsizei primcount);

bool ValidateDrawElements(ValidationContext *context,
                          GLenum mode,
                          GLsizei count,
                          GLenum type,
                          const GLvoid *indices,
                          GLsizei primcount,
                          IndexRange *indexRangeOut);

bool ValidateDrawElementsInstanced(Context *context,
                                   GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const GLvoid *indices,
                                   GLsizei primcount,
                                   IndexRange *indexRangeOut);
bool ValidateDrawElementsInstancedANGLE(Context *context,
                                        GLenum mode,
                                        GLsizei count,
                                        GLenum type,
                                        const GLvoid *indices,
                                        GLsizei primcount,
                                        IndexRange *indexRangeOut);

bool ValidateFramebufferTextureBase(Context *context, GLenum target, GLenum attachment,
                                    GLuint texture, GLint level);
bool ValidateFramebufferTexture2D(Context *context, GLenum target, GLenum attachment,
                                   GLenum textarget, GLuint texture, GLint level);
bool ValidateFramebufferRenderbuffer(Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer);

bool ValidateGetUniformBase(Context *context, GLuint program, GLint location);
bool ValidateGetUniformfv(Context *context, GLuint program, GLint location, GLfloat* params);
bool ValidateGetUniformiv(Context *context, GLuint program, GLint location, GLint* params);
bool ValidateGetnUniformfvEXT(Context *context, GLuint program, GLint location, GLsizei bufSize, GLfloat* params);
bool ValidateGetnUniformivEXT(Context *context, GLuint program, GLint location, GLsizei bufSize, GLint* params);
bool ValidateGetUniformfvRobustANGLE(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLfloat *params);
bool ValidateGetUniformivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params);
bool ValidateGetUniformuivRobustANGLE(Context *context,
                                      GLuint program,
                                      GLint location,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLuint *params);

bool ValidateDiscardFramebufferBase(Context *context, GLenum target, GLsizei numAttachments,
                                    const GLenum *attachments, bool defaultFramebuffer);

bool ValidateInsertEventMarkerEXT(Context *context, GLsizei length, const char *marker);
bool ValidatePushGroupMarkerEXT(Context *context, GLsizei length, const char *marker);

bool ValidateEGLImageTargetTexture2DOES(Context *context,
                                        egl::Display *display,
                                        GLenum target,
                                        egl::Image *image);
bool ValidateEGLImageTargetRenderbufferStorageOES(Context *context,
                                                  egl::Display *display,
                                                  GLenum target,
                                                  egl::Image *image);

bool ValidateBindVertexArrayBase(Context *context, GLuint array);

bool ValidateLinkProgram(Context *context, GLuint program);
bool ValidateProgramBinaryBase(Context *context,
                               GLuint program,
                               GLenum binaryFormat,
                               const void *binary,
                               GLint length);
bool ValidateGetProgramBinaryBase(Context *context,
                                  GLuint program,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLenum *binaryFormat,
                                  void *binary);
bool ValidateUseProgram(Context *context, GLuint program);

bool ValidateCopyTexImage2D(ValidationContext *context,
                            GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLint border);
bool ValidateDrawBuffersBase(ValidationContext *context, GLsizei n, const GLenum *bufs);
bool ValidateCopyTexSubImage2D(Context *context,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height);

bool ValidateGetBufferPointervBase(Context *context,
                                   GLenum target,
                                   GLenum pname,
                                   GLsizei *length,
                                   void **params);
bool ValidateUnmapBufferBase(Context *context, GLenum target);
bool ValidateMapBufferRangeBase(Context *context,
                                GLenum target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access);
bool ValidateFlushMappedBufferRangeBase(Context *context,
                                        GLenum target,
                                        GLintptr offset,
                                        GLsizeiptr length);

bool ValidateGenerateMipmap(Context *context, GLenum target);

bool ValidateGenBuffers(Context *context, GLint n, GLuint *buffers);
bool ValidateDeleteBuffers(Context *context, GLint n, const GLuint *buffers);
bool ValidateGenFramebuffers(Context *context, GLint n, GLuint *framebuffers);
bool ValidateDeleteFramebuffers(Context *context, GLint n, const GLuint *framebuffers);
bool ValidateGenRenderbuffers(Context *context, GLint n, GLuint *renderbuffers);
bool ValidateDeleteRenderbuffers(Context *context, GLint n, const GLuint *renderbuffers);
bool ValidateGenTextures(Context *context, GLint n, GLuint *textures);
bool ValidateDeleteTextures(Context *context, GLint n, const GLuint *textures);

bool ValidateGenOrDelete(Context *context, GLint n);

bool ValidateEnable(Context *context, GLenum cap);
bool ValidateDisable(Context *context, GLenum cap);
bool ValidateIsEnabled(Context *context, GLenum cap);

bool ValidateRobustEntryPoint(ValidationContext *context, GLsizei bufSize);
bool ValidateRobustBufferSize(ValidationContext *context, GLsizei bufSize, GLsizei numParams);

bool ValidateGetFramebufferAttachmentParameteriv(ValidationContext *context,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 GLsizei *numParams);
bool ValidateGetFramebufferAttachmentParameterivRobustANGLE(ValidationContext *context,
                                                            GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *numParams);

bool ValidateGetBufferParameteriv(ValidationContext *context,
                                  GLenum target,
                                  GLenum pname,
                                  GLint *params);
bool ValidateGetBufferParameterivRobustANGLE(ValidationContext *context,
                                             GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLint *params);

bool ValidateGetBufferParameteri64v(ValidationContext *context,
                                    GLenum target,
                                    GLenum pname,
                                    GLint64 *params);
bool ValidateGetBufferParameteri64vRobustANGLE(ValidationContext *context,
                                               GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint64 *params);

bool ValidateGetProgramiv(Context *context, GLuint program, GLenum pname, GLsizei *numParams);
bool ValidateGetProgramivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams);

bool ValidateGetRenderbufferParameteriv(Context *context,
                                        GLenum target,
                                        GLenum pname,
                                        GLint *params);
bool ValidateGetRenderbufferParameterivRobustANGLE(Context *context,
                                                   GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params);

bool ValidateGetShaderiv(Context *context, GLuint shader, GLenum pname, GLint *params);
bool ValidateGetShaderivRobustANGLE(Context *context,
                                    GLuint shader,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint *params);

bool ValidateGetTexParameterfv(Context *context, GLenum target, GLenum pname, GLfloat *params);
bool ValidateGetTexParameterfvRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params);
bool ValidateGetTexParameteriv(Context *context, GLenum target, GLenum pname, GLint *params);
bool ValidateGetTexParameterivRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);

bool ValidateTexParameterf(Context *context, GLenum target, GLenum pname, GLfloat param);
bool ValidateTexParameterfv(Context *context, GLenum target, GLenum pname, const GLfloat *params);
bool ValidateTexParameterfvRobustANGLE(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *params);
bool ValidateTexParameteri(Context *context, GLenum target, GLenum pname, GLint param);
bool ValidateTexParameteriv(Context *context, GLenum target, GLenum pname, const GLint *params);
bool ValidateTexParameterivRobustANGLE(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *params);

bool ValidateGetSamplerParameterfv(Context *context, GLuint sampler, GLenum pname, GLfloat *params);
bool ValidateGetSamplerParameterfvRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLfloat *params);
bool ValidateGetSamplerParameteriv(Context *context, GLuint sampler, GLenum pname, GLint *params);
bool ValidateGetSamplerParameterivRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLint *params);

bool ValidateSamplerParameterf(Context *context, GLuint sampler, GLenum pname, GLfloat param);
bool ValidateSamplerParameterfv(Context *context,
                                GLuint sampler,
                                GLenum pname,
                                const GLfloat *params);
bool ValidateSamplerParameterfvRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLfloat *params);
bool ValidateSamplerParameteri(Context *context, GLuint sampler, GLenum pname, GLint param);
bool ValidateSamplerParameteriv(Context *context,
                                GLuint sampler,
                                GLenum pname,
                                const GLint *params);
bool ValidateSamplerParameterivRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLint *params);

bool ValidateGetVertexAttribfv(Context *context, GLuint index, GLenum pname, GLfloat *params);
bool ValidateGetVertexAttribfvRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params);

bool ValidateGetVertexAttribiv(Context *context, GLuint index, GLenum pname, GLint *params);
bool ValidateGetVertexAttribivRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);

bool ValidateGetVertexAttribPointerv(Context *context, GLuint index, GLenum pname, void **pointer);
bool ValidateGetVertexAttribPointervRobustANGLE(Context *context,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **pointer);

bool ValidateGetVertexAttribIiv(Context *context, GLuint index, GLenum pname, GLint *params);
bool ValidateGetVertexAttribIivRobustANGLE(Context *context,
                                           GLuint index,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params);

bool ValidateGetVertexAttribIuiv(Context *context, GLuint index, GLenum pname, GLuint *params);
bool ValidateGetVertexAttribIuivRobustANGLE(Context *context,
                                            GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params);

bool ValidateGetActiveUniformBlockiv(Context *context,
                                     GLuint program,
                                     GLuint uniformBlockIndex,
                                     GLenum pname,
                                     GLint *params);
bool ValidateGetActiveUniformBlockivRobustANGLE(Context *context,
                                                GLuint program,
                                                GLuint uniformBlockIndex,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params);

bool ValidateGetInternalFormativ(Context *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLint *params);
bool ValidateGetInternalFormativRobustANGLE(Context *context,
                                            GLenum target,
                                            GLenum internalformat,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params);

// Error messages shared here for use in testing.
extern const char *g_ExceedsMaxElementErrorMessage;
}  // namespace gl

#endif // LIBANGLE_VALIDATION_ES_H_
