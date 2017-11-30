//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES.h: Validation functions for generic OpenGL ES entry point parameters

#ifndef LIBANGLE_VALIDATION_ES_H_
#define LIBANGLE_VALIDATION_ES_H_

#include "common/mathutil.h"
#include "libANGLE/PackedGLEnums.h"

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
struct LinkedUniform;
class Program;
class Shader;
class ValidationContext;

bool ValidTextureTarget(const ValidationContext *context, GLenum target);
bool ValidTexture2DTarget(const ValidationContext *context, GLenum target);
bool ValidTexture3DTarget(const ValidationContext *context, GLenum target);
bool ValidTextureExternalTarget(const ValidationContext *context, GLenum target);
bool ValidTexture2DDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidTexture3DDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidTexLevelDestinationTarget(const ValidationContext *context, GLenum target);
bool ValidFramebufferTarget(const ValidationContext *context, GLenum target);
bool ValidBufferType(const ValidationContext *context, BufferBinding target);
bool ValidBufferParameter(const ValidationContext *context, GLenum pname, GLsizei *numParams);
bool ValidMipLevel(const ValidationContext *context, GLenum target, GLint level);
bool ValidImageSizeParameters(ValidationContext *context,
                              GLenum target,
                              GLint level,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              bool isSubImage);
bool ValidCompressedImageSize(const ValidationContext *context,
                              GLenum internalFormat,
                              GLint level,
                              GLsizei width,
                              GLsizei height);
bool ValidCompressedSubImageSize(const ValidationContext *context,
                                 GLenum internalFormat,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 size_t textureWidth,
                                 size_t textureHeight);
bool ValidImageDataSize(ValidationContext *context,
                        GLenum textureTarget,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLenum format,
                        GLenum type,
                        const void *pixels,
                        GLsizei imageSize);

bool ValidQueryType(const Context *context, GLenum queryType);

bool ValidateWebGLVertexAttribPointer(ValidationContext *context,
                                      GLenum type,
                                      GLboolean normalized,
                                      GLsizei stride,
                                      const void *ptr,
                                      bool pureInteger);

// Returns valid program if id is a valid program name
// Errors INVALID_OPERATION if valid shader is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Program *GetValidProgram(ValidationContext *context, GLuint id);

// Returns valid shader if id is a valid shader name
// Errors INVALID_OPERATION if valid program is given and returns NULL
// Errors INVALID_VALUE otherwise and returns NULL
Shader *GetValidShader(ValidationContext *context, GLuint id);

bool ValidateAttachmentTarget(Context *context, GLenum attachment);
bool ValidateRenderbufferStorageParametersBase(ValidationContext *context,
                                               GLenum target,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height);
bool ValidateFramebufferRenderbufferParameters(Context *context,
                                               GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               GLuint renderbuffer);

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

bool ValidateReadPixelsBase(Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLsizei *columns,
                            GLsizei *rows,
                            void *pixels);
bool ValidateReadPixelsRobustANGLE(Context *context,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLsizei *columns,
                                   GLsizei *rows,
                                   void *pixels);
bool ValidateReadnPixelsEXT(Context *context,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            GLsizei bufSize,
                            void *pixels);
bool ValidateReadnPixelsRobustANGLE(Context *context,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLsizei *columns,
                                    GLsizei *rows,
                                    void *data);

bool ValidateGenQueriesEXT(gl::Context *context, GLsizei n, GLuint *ids);
bool ValidateDeleteQueriesEXT(gl::Context *context, GLsizei n, const GLuint *ids);
bool ValidateIsQueryEXT(gl::Context *context, GLuint id);
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

bool ValidateUniformCommonBase(ValidationContext *context,
                               gl::Program *program,
                               GLint location,
                               GLsizei count,
                               const LinkedUniform **uniformOut);
bool ValidateUniform1ivValue(ValidationContext *context,
                             GLenum uniformType,
                             GLsizei count,
                             const GLint *value);
bool ValidateUniformValue(ValidationContext *context, GLenum valueType, GLenum uniformType);
bool ValidateUniformMatrixValue(ValidationContext *context, GLenum valueType, GLenum uniformType);
bool ValidateUniform(ValidationContext *context, GLenum uniformType, GLint location, GLsizei count);
bool ValidateUniformMatrix(ValidationContext *context,
                           GLenum matrixType,
                           GLint location,
                           GLsizei count,
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

bool ValidateDrawBase(ValidationContext *context, GLenum mode, GLsizei count);
bool ValidateDrawArraysCommon(ValidationContext *context,
                              GLenum mode,
                              GLint first,
                              GLsizei count,
                              GLsizei primcount);
bool ValidateDrawArraysInstancedBase(Context *context,
                                     GLenum mode,
                                     GLint first,
                                     GLsizei count,
                                     GLsizei primcount);
bool ValidateDrawArraysInstancedANGLE(Context *context,
                                      GLenum mode,
                                      GLint first,
                                      GLsizei count,
                                      GLsizei primcount);

bool ValidateDrawElementsBase(ValidationContext *context, GLenum type);
bool ValidateDrawElementsCommon(ValidationContext *context,
                                GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void *indices,
                                GLsizei primcount);

bool ValidateDrawElementsInstancedCommon(ValidationContext *context,
                                         GLenum mode,
                                         GLsizei count,
                                         GLenum type,
                                         const void *indices,
                                         GLsizei primcount);
bool ValidateDrawElementsInstancedANGLE(Context *context,
                                        GLenum mode,
                                        GLsizei count,
                                        GLenum type,
                                        const void *indices,
                                        GLsizei primcount);

bool ValidateFramebufferTextureBase(Context *context,
                                    GLenum target,
                                    GLenum attachment,
                                    GLuint texture,
                                    GLint level);

bool ValidateGetUniformBase(Context *context, GLuint program, GLint location);
bool ValidateGetnUniformfvEXT(Context *context,
                              GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLfloat *params);
bool ValidateGetnUniformivEXT(Context *context,
                              GLuint program,
                              GLint location,
                              GLsizei bufSize,
                              GLint *params);
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

bool ValidateDiscardFramebufferBase(Context *context,
                                    GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments,
                                    bool defaultFramebuffer);

bool ValidateInsertEventMarkerEXT(Context *context, GLsizei length, const char *marker);
bool ValidatePushGroupMarkerEXT(Context *context, GLsizei length, const char *marker);

bool ValidateEGLImageTargetTexture2DOES(Context *context,
                                        GLenum target,
                                        egl::Image *image);
bool ValidateEGLImageTargetRenderbufferStorageOES(Context *context,
                                                  GLenum target,
                                                  egl::Image *image);

bool ValidateBindVertexArrayBase(Context *context, GLuint array);

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

bool ValidateDrawBuffersBase(ValidationContext *context, GLsizei n, const GLenum *bufs);

bool ValidateGetBufferPointervBase(Context *context,
                                   BufferBinding target,
                                   GLenum pname,
                                   GLsizei *length,
                                   void **params);
bool ValidateUnmapBufferBase(Context *context, BufferBinding target);
bool ValidateMapBufferRangeBase(Context *context,
                                BufferBinding target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access);
bool ValidateFlushMappedBufferRangeBase(Context *context,
                                        BufferBinding target,
                                        GLintptr offset,
                                        GLsizeiptr length);

bool ValidateGenOrDelete(Context *context, GLint n);

bool ValidateRobustEntryPoint(ValidationContext *context, GLsizei bufSize);
bool ValidateRobustBufferSize(ValidationContext *context, GLsizei bufSize, GLsizei numParams);

bool ValidateGetFramebufferAttachmentParameterivBase(ValidationContext *context,
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

bool ValidateGetBufferParameterBase(ValidationContext *context,
                                    BufferBinding target,
                                    GLenum pname,
                                    bool pointerVersion,
                                    GLsizei *numParams);
bool ValidateGetBufferParameterivRobustANGLE(ValidationContext *context,
                                             BufferBinding target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLint *params);

bool ValidateGetBufferParameteri64vRobustANGLE(ValidationContext *context,
                                               BufferBinding target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint64 *params);

bool ValidateGetProgramivBase(ValidationContext *context,
                              GLuint program,
                              GLenum pname,
                              GLsizei *numParams);
bool ValidateGetProgramivRobustANGLE(Context *context,
                                     GLuint program,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams);

bool ValidateGetRenderbufferParameterivBase(Context *context,
                                            GLenum target,
                                            GLenum pname,
                                            GLsizei *length);
bool ValidateGetRenderbufferParameterivRobustANGLE(Context *context,
                                                   GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params);

bool ValidateGetShaderivBase(Context *context, GLuint shader, GLenum pname, GLsizei *length);
bool ValidateGetShaderivRobustANGLE(Context *context,
                                    GLuint shader,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint *params);

bool ValidateGetTexParameterBase(Context *context, GLenum target, GLenum pname, GLsizei *length);
bool ValidateGetTexParameterfvRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params);
bool ValidateGetTexParameterivRobustANGLE(Context *context,
                                          GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);

template <typename ParamType>
bool ValidateTexParameterBase(Context *context,
                              GLenum target,
                              GLenum pname,
                              GLsizei bufSize,
                              const ParamType *params);
bool ValidateTexParameterfvRobustANGLE(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *params);
bool ValidateTexParameterivRobustANGLE(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *params);

bool ValidateGetSamplerParameterfvRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLfloat *params);
bool ValidateGetSamplerParameterivRobustANGLE(Context *context,
                                              GLuint sampler,
                                              GLenum pname,
                                              GLuint bufSize,
                                              GLsizei *length,
                                              GLint *params);

bool ValidateSamplerParameterfvRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLfloat *params);
bool ValidateSamplerParameterivRobustANGLE(Context *context,
                                           GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           const GLint *params);

bool ValidateGetVertexAttribBase(Context *context,
                                 GLuint index,
                                 GLenum pname,
                                 GLsizei *length,
                                 bool pointer,
                                 bool pureIntegerEntryPoint);
bool ValidateGetVertexAttribfvRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params);

bool ValidateGetVertexAttribivRobustANGLE(Context *context,
                                          GLuint index,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params);

bool ValidateGetVertexAttribPointervRobustANGLE(Context *context,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **pointer);

bool ValidateGetVertexAttribIivRobustANGLE(Context *context,
                                           GLuint index,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params);

bool ValidateGetVertexAttribIuivRobustANGLE(Context *context,
                                            GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params);

bool ValidateGetActiveUniformBlockivRobustANGLE(Context *context,
                                                GLuint program,
                                                GLuint uniformBlockIndex,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params);

bool ValidateGetInternalFormativRobustANGLE(Context *context,
                                            GLenum target,
                                            GLenum internalformat,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params);

bool ValidateVertexFormatBase(ValidationContext *context,
                              GLuint attribIndex,
                              GLint size,
                              GLenum type,
                              GLboolean pureInteger);

bool ValidateWebGLFramebufferAttachmentClearType(ValidationContext *context,
                                                 GLint drawbuffer,
                                                 const GLenum *validComponentTypes,
                                                 size_t validComponentTypeCount);

bool ValidateRobustCompressedTexImageBase(ValidationContext *context,
                                          GLsizei imageSize,
                                          GLsizei dataSize);

bool ValidateVertexAttribIndex(ValidationContext *context, GLuint index);

bool ValidateGetActiveUniformBlockivBase(Context *context,
                                         GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLenum pname,
                                         GLsizei *length);

bool ValidateGetSamplerParameterBase(Context *context,
                                     GLuint sampler,
                                     GLenum pname,
                                     GLsizei *length);

template <typename ParamType>
bool ValidateSamplerParameterBase(Context *context,
                                  GLuint sampler,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  ParamType *params);

bool ValidateGetInternalFormativBase(Context *context,
                                     GLenum target,
                                     GLenum internalformat,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *numParams);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES_H_
