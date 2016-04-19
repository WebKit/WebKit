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

namespace egl
{
class Display;
class Image;
}

namespace gl
{
class Context;
class Program;
class Shader;
class ValidationContext;

bool ValidCap(const Context *context, GLenum cap);
bool ValidTextureTarget(const ValidationContext *context, GLenum target);
bool ValidTexture2DTarget(const ValidationContext *context, GLenum target);
bool ValidTexture3DTarget(const ValidationContext *context, GLenum target);
bool ValidTexture2DDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidTexture3DDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidFramebufferTarget(GLenum target);
bool ValidBufferTarget(const Context *context, GLenum target);
bool ValidBufferParameter(const Context *context, GLenum pname);
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
bool ValidQueryType(const Context *context, GLenum queryType);

// Returns valid program if id is a valid program name
// Errors INVALID_OPERATION if valid shader is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Program *GetValidProgram(Context *context, GLuint id);

// Returns valid shader if id is a valid shader name
// Errors INVALID_OPERATION if valid program is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Shader *GetValidShader(Context *context, GLuint id);

bool ValidateAttachmentTarget(Context *context, GLenum attachment);
bool ValidateRenderbufferStorageParametersBase(Context *context, GLenum target, GLsizei samples,
                                               GLenum internalformat, GLsizei width, GLsizei height);
bool ValidateRenderbufferStorageParametersANGLE(Context *context, GLenum target, GLsizei samples,
                                                GLenum internalformat, GLsizei width, GLsizei height);

bool ValidateFramebufferRenderbufferParameters(Context *context, GLenum target, GLenum attachment,
                                               GLenum renderbuffertarget, GLuint renderbuffer);

bool ValidateBlitFramebufferParameters(Context *context,
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

bool ValidateGetVertexAttribParameters(Context *context, GLenum pname);

bool ValidateTexParamParameters(Context *context, GLenum pname, GLint param);

bool ValidateSamplerObjectParameter(Context *context, GLenum pname);

bool ValidateReadPixels(Context *context,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
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

bool ValidateGenQueriesEXT(gl::Context *context, GLsizei n);
bool ValidateDeleteQueriesEXT(gl::Context *context, GLsizei n);
bool ValidateBeginQueryBase(Context *context, GLenum target, GLuint id);
bool ValidateBeginQueryEXT(Context *context, GLenum target, GLuint id);
bool ValidateEndQueryBase(Context *context, GLenum target);
bool ValidateEndQueryEXT(Context *context, GLenum target);
bool ValidateQueryCounterEXT(Context *context, GLuint id, GLenum target);
bool ValidateGetQueryivBase(Context *context, GLenum target, GLenum pname);
bool ValidateGetQueryivEXT(Context *context, GLenum target, GLenum pname, GLint *params);
bool ValidateGetQueryObjectValueBase(Context *context, GLenum target, GLenum pname);
bool ValidateGetQueryObjectivEXT(Context *context, GLuint id, GLenum pname, GLint *params);
bool ValidateGetQueryObjectuivEXT(Context *context, GLuint id, GLenum pname, GLuint *params);
bool ValidateGetQueryObjecti64vEXT(Context *context, GLuint id, GLenum pname, GLint64 *params);
bool ValidateGetQueryObjectui64vEXT(Context *context, GLuint id, GLenum pname, GLuint64 *params);

bool ValidateUniform(Context *context, GLenum uniformType, GLint location, GLsizei count);
bool ValidateUniformMatrix(Context *context, GLenum matrixType, GLint location, GLsizei count,
                           GLboolean transpose);

bool ValidateStateQuery(Context *context, GLenum pname, GLenum *nativeType, unsigned int *numParams);

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
                                        GLenum *textureInternalFormatOut);

bool ValidateDrawArrays(Context *context, GLenum mode, GLint first, GLsizei count, GLsizei primcount);
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

bool ValidateGetBufferPointervBase(Context *context, GLenum target, GLenum pname, void **params);
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

bool ValidateGenBuffers(Context *context, GLint n, GLuint *buffers);
bool ValidateDeleteBuffers(Context *context, GLint n, const GLuint *buffers);
bool ValidateGenFramebuffers(Context *context, GLint n, GLuint *framebuffers);
bool ValidateDeleteFramebuffers(Context *context, GLint n, const GLuint *framebuffers);
bool ValidateGenRenderbuffers(Context *context, GLint n, GLuint *renderbuffers);
bool ValidateDeleteRenderbuffers(Context *context, GLint n, const GLuint *renderbuffers);
bool ValidateGenTextures(Context *context, GLint n, GLuint *textures);
bool ValidateDeleteTextures(Context *context, GLint n, const GLuint *textures);

bool ValidateGenOrDelete(Context *context, GLint n);

// Error messages shared here for use in testing.
extern const char *g_ExceedsMaxElementErrorMessage;
}  // namespace gl

#endif // LIBANGLE_VALIDATION_ES_H_
