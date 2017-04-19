//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES2.h: Validation functions for OpenGL ES 2.0 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES2_H_
#define LIBANGLE_VALIDATION_ES2_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace gl
{
class Context;
class ValidationContext;
class Texture;

bool ValidateES2TexStorageParameters(Context *context, GLenum target, GLsizei levels, GLenum internalformat,
                                                   GLsizei width, GLsizei height);

bool ValidateDiscardFramebufferEXT(Context *context, GLenum target, GLsizei numAttachments,
                                   const GLenum *attachments);

bool ValidateDrawBuffersEXT(ValidationContext *context, GLsizei n, const GLenum *bufs);

bool ValidateBindVertexArrayOES(Context *context, GLuint array);
bool ValidateDeleteVertexArraysOES(Context *context, GLsizei n);
bool ValidateGenVertexArraysOES(Context *context, GLsizei n);
bool ValidateIsVertexArrayOES(Context *context);

bool ValidateProgramBinaryOES(Context *context,
                              GLuint program,
                              GLenum binaryFormat,
                              const void *binary,
                              GLint length);
bool ValidateGetProgramBinaryOES(Context *context,
                                 GLuint program,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLenum *binaryFormat,
                                 void *binary);

// GL_KHR_debug
bool ValidateDebugMessageControlKHR(Context *context,
                                    GLenum source,
                                    GLenum type,
                                    GLenum severity,
                                    GLsizei count,
                                    const GLuint *ids,
                                    GLboolean enabled);
bool ValidateDebugMessageInsertKHR(Context *context,
                                   GLenum source,
                                   GLenum type,
                                   GLuint id,
                                   GLenum severity,
                                   GLsizei length,
                                   const GLchar *buf);
bool ValidateDebugMessageCallbackKHR(Context *context,
                                     GLDEBUGPROCKHR callback,
                                     const void *userParam);
bool ValidateGetDebugMessageLogKHR(Context *context,
                                   GLuint count,
                                   GLsizei bufSize,
                                   GLenum *sources,
                                   GLenum *types,
                                   GLuint *ids,
                                   GLenum *severities,
                                   GLsizei *lengths,
                                   GLchar *messageLog);
bool ValidatePushDebugGroupKHR(Context *context,
                               GLenum source,
                               GLuint id,
                               GLsizei length,
                               const GLchar *message);
bool ValidatePopDebugGroupKHR(Context *context);
bool ValidateObjectLabelKHR(Context *context,
                            GLenum identifier,
                            GLuint name,
                            GLsizei length,
                            const GLchar *label);
bool ValidateGetObjectLabelKHR(Context *context,
                               GLenum identifier,
                               GLuint name,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLchar *label);
bool ValidateObjectPtrLabelKHR(Context *context,
                               const void *ptr,
                               GLsizei length,
                               const GLchar *label);
bool ValidateGetObjectPtrLabelKHR(Context *context,
                                  const void *ptr,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLchar *label);
bool ValidateGetPointervKHR(Context *context, GLenum pname, void **params);
bool ValidateBlitFramebufferANGLE(Context *context,
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

bool ValidateClear(ValidationContext *context, GLbitfield mask);
bool ValidateTexImage2D(Context *context,
                        GLenum target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const GLvoid *pixels);
bool ValidateTexImage2DRobust(Context *context,
                              GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              GLsizei bufSize,
                              const GLvoid *pixels);
bool ValidateTexSubImage2D(Context *context,
                           GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLsizei width,
                           GLsizei height,
                           GLenum format,
                           GLenum type,
                           const GLvoid *pixels);
bool ValidateTexSubImage2DRobustANGLE(Context *context,
                                      GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei bufSize,
                                      const GLvoid *pixels);
bool ValidateCompressedTexImage2D(Context *context,
                                  GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border,
                                  GLsizei imageSize,
                                  const GLvoid *data);
bool ValidateCompressedTexSubImage2D(Context *context,
                                     GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const GLvoid *data);
bool ValidateBindTexture(Context *context, GLenum target, GLuint texture);

bool ValidateGetBufferPointervOES(Context *context, GLenum target, GLenum pname, void **params);
bool ValidateMapBufferOES(Context *context, GLenum target, GLenum access);
bool ValidateUnmapBufferOES(Context *context, GLenum target);
bool ValidateMapBufferRangeEXT(Context *context,
                               GLenum target,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access);
bool ValidateFlushMappedBufferRangeEXT(Context *context,
                                       GLenum target,
                                       GLintptr offset,
                                       GLsizeiptr length);

bool ValidateBindUniformLocationCHROMIUM(Context *context,
                                         GLuint program,
                                         GLint location,
                                         const GLchar *name);

bool ValidateCoverageModulationCHROMIUM(Context *context, GLenum components);

// CHROMIUM_path_rendering
bool ValidateMatrix(Context *context, GLenum matrixMode, const GLfloat *matrix);
bool ValidateMatrixMode(Context *context, GLenum matrixMode);
bool ValidateGenPaths(Context *context, GLsizei range);
bool ValidateDeletePaths(Context *context, GLuint first, GLsizei range);
bool ValidatePathCommands(Context *context,
                          GLuint path,
                          GLsizei numCommands,
                          const GLubyte *commands,
                          GLsizei numCoords,
                          GLenum coordType,
                          const void *coords);
bool ValidateSetPathParameter(Context *context, GLuint path, GLenum pname, GLfloat value);
bool ValidateGetPathParameter(Context *context, GLuint path, GLenum pname, GLfloat *value);
bool ValidatePathStencilFunc(Context *context, GLenum func, GLint ref, GLuint mask);
bool ValidateStencilFillPath(Context *context, GLuint path, GLenum fillMode, GLuint mask);
bool ValidateStencilStrokePath(Context *context, GLuint path, GLint reference, GLuint mask);
bool ValidateCoverPath(Context *context, GLuint path, GLenum coverMode);
bool ValidateStencilThenCoverFillPath(Context *context,
                                      GLuint path,
                                      GLenum fillMode,
                                      GLuint mask,
                                      GLenum coverMode);
bool ValidateStencilThenCoverStrokePath(Context *context,
                                        GLuint path,
                                        GLint reference,
                                        GLuint mask,
                                        GLenum coverMode);
bool ValidateIsPath(Context *context);
bool ValidateCoverFillPathInstanced(Context *context,
                                    GLsizei numPaths,
                                    GLenum pathNameType,
                                    const void *paths,
                                    GLuint pathBase,
                                    GLenum coverMode,
                                    GLenum transformType,
                                    const GLfloat *transformValues);
bool ValidateCoverStrokePathInstanced(Context *context,
                                      GLsizei numPaths,
                                      GLenum pathNameType,
                                      const void *paths,
                                      GLuint pathBase,
                                      GLenum coverMode,
                                      GLenum transformType,
                                      const GLfloat *transformValues);
bool ValidateStencilFillPathInstanced(Context *context,
                                      GLsizei numPaths,
                                      GLenum pathNameType,
                                      const void *paths,
                                      GLuint pathBAse,
                                      GLenum fillMode,
                                      GLuint mask,
                                      GLenum transformType,
                                      const GLfloat *transformValues);
bool ValidateStencilStrokePathInstanced(Context *context,
                                        GLsizei numPaths,
                                        GLenum pathNameType,
                                        const void *paths,
                                        GLuint pathBase,
                                        GLint reference,
                                        GLuint mask,
                                        GLenum transformType,
                                        const GLfloat *transformValues);
bool ValidateStencilThenCoverFillPathInstanced(Context *context,
                                               GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void *paths,
                                               GLuint pathBase,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum coverMode,
                                               GLenum transformType,
                                               const GLfloat *transformValues);
bool ValidateStencilThenCoverStrokePathInstanced(Context *context,
                                                 GLsizei numPaths,
                                                 GLenum pathNameType,
                                                 const void *paths,
                                                 GLuint pathBase,
                                                 GLint reference,
                                                 GLuint mask,
                                                 GLenum coverMode,
                                                 GLenum transformType,
                                                 const GLfloat *transformValues);
bool ValidateBindFragmentInputLocation(Context *context,
                                       GLuint program,
                                       GLint location,
                                       const GLchar *name);
bool ValidateProgramPathFragmentInputGen(Context *context,
                                         GLuint program,
                                         GLint location,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat *coeffs);

bool ValidateCopyTextureCHROMIUM(Context *context,
                                 GLuint sourceId,
                                 GLint sourceLevel,
                                 GLenum destTarget,
                                 GLuint destId,
                                 GLint destLevel,
                                 GLint internalFormat,
                                 GLenum destType,
                                 GLboolean unpackFlipY,
                                 GLboolean unpackPremultiplyAlpha,
                                 GLboolean unpackUnmultiplyAlpha);
bool ValidateCopySubTextureCHROMIUM(Context *context,
                                    GLuint sourceId,
                                    GLint sourceLevel,
                                    GLenum destTarget,
                                    GLuint destId,
                                    GLint destLevel,
                                    GLint xoffset,
                                    GLint yoffset,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLboolean unpackFlipY,
                                    GLboolean unpackPremultiplyAlpha,
                                    GLboolean unpackUnmultiplyAlpha);
bool ValidateCompressedCopyTextureCHROMIUM(Context *context, GLuint sourceId, GLuint destId);

bool ValidateCreateShader(Context *context, GLenum type);
bool ValidateBufferData(ValidationContext *context,
                        GLenum target,
                        GLsizeiptr size,
                        const GLvoid *data,
                        GLenum usage);
bool ValidateBufferSubData(ValidationContext *context,
                           GLenum target,
                           GLintptr offset,
                           GLsizeiptr size,
                           const GLvoid *data);

bool ValidateRequestExtensionANGLE(ValidationContext *context, const GLchar *name);

bool ValidateActiveTexture(ValidationContext *context, GLenum texture);
bool ValidateAttachShader(ValidationContext *context, GLuint program, GLuint shader);
bool ValidateBindAttribLocation(ValidationContext *context,
                                GLuint program,
                                GLuint index,
                                const GLchar *name);
bool ValidateBindBuffer(ValidationContext *context, GLenum target, GLuint buffer);
bool ValidateBindFramebuffer(ValidationContext *context, GLenum target, GLuint framebuffer);
bool ValidateBindRenderbuffer(ValidationContext *context, GLenum target, GLuint renderbuffer);
bool ValidateBlendColor(ValidationContext *context,
                        GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha);
bool ValidateBlendEquation(ValidationContext *context, GLenum mode);
bool ValidateBlendEquationSeparate(ValidationContext *context, GLenum modeRGB, GLenum modeAlpha);
bool ValidateBlendFunc(ValidationContext *context, GLenum sfactor, GLenum dfactor);
bool ValidateBlendFuncSeparate(ValidationContext *context,
                               GLenum srcRGB,
                               GLenum dstRGB,
                               GLenum srcAlpha,
                               GLenum dstAlpha);

bool ValidateGetString(Context *context, GLenum name);
bool ValidateLineWidth(ValidationContext *context, GLfloat width);
bool ValidateVertexAttribPointer(ValidationContext *context,
                                 GLuint index,
                                 GLint size,
                                 GLenum type,
                                 GLboolean normalized,
                                 GLsizei stride,
                                 const GLvoid *ptr);

bool ValidateDepthRangef(ValidationContext *context, GLclampf zNear, GLclampf zFar);
bool ValidateRenderbufferStorage(ValidationContext *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height);
bool ValidateRenderbufferStorageMultisampleANGLE(ValidationContext *context,
                                                 GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height);

bool ValidateCheckFramebufferStatus(ValidationContext *context, GLenum target);
bool ValidateClearColor(ValidationContext *context,
                        GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha);
bool ValidateClearDepthf(ValidationContext *context, GLclampf depth);
bool ValidateClearStencil(ValidationContext *context, GLint s);
bool ValidateColorMask(ValidationContext *context,
                       GLboolean red,
                       GLboolean green,
                       GLboolean blue,
                       GLboolean alpha);
bool ValidateCompileShader(ValidationContext *context, GLuint shader);
bool ValidateCreateProgram(ValidationContext *context);
bool ValidateCullFace(ValidationContext *context, GLenum mode);
bool ValidateDeleteProgram(ValidationContext *context, GLuint program);
bool ValidateDeleteShader(ValidationContext *context, GLuint shader);
bool ValidateDepthFunc(ValidationContext *context, GLenum func);
bool ValidateDepthMask(ValidationContext *context, GLboolean flag);
bool ValidateDetachShader(ValidationContext *context, GLuint program, GLuint shader);
bool ValidateDisableVertexAttribArray(ValidationContext *context, GLuint index);
bool ValidateEnableVertexAttribArray(ValidationContext *context, GLuint index);
bool ValidateFinish(ValidationContext *context);
bool ValidateFlush(ValidationContext *context);
bool ValidateFrontFace(ValidationContext *context, GLenum mode);
bool ValidateGetActiveAttrib(ValidationContext *context,
                             GLuint program,
                             GLuint index,
                             GLsizei bufsize,
                             GLsizei *length,
                             GLint *size,
                             GLenum *type,
                             GLchar *name);
bool ValidateGetActiveUniform(ValidationContext *context,
                              GLuint program,
                              GLuint index,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLint *size,
                              GLenum *type,
                              GLchar *name);
bool ValidateGetAttachedShaders(ValidationContext *context,
                                GLuint program,
                                GLsizei maxcount,
                                GLsizei *count,
                                GLuint *shaders);
bool ValidateGetAttribLocation(ValidationContext *context, GLuint program, const GLchar *name);
bool ValidateGetBooleanv(ValidationContext *context, GLenum pname, GLboolean *params);
bool ValidateGetError(ValidationContext *context);
bool ValidateGetFloatv(ValidationContext *context, GLenum pname, GLfloat *params);
bool ValidateGetIntegerv(ValidationContext *context, GLenum pname, GLint *params);
bool ValidateGetProgramInfoLog(ValidationContext *context,
                               GLuint program,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLchar *infolog);
bool ValidateGetShaderInfoLog(ValidationContext *context,
                              GLuint shader,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLchar *infolog);
bool ValidateGetShaderPrecisionFormat(ValidationContext *context,
                                      GLenum shadertype,
                                      GLenum precisiontype,
                                      GLint *range,
                                      GLint *precision);
bool ValidateGetShaderSource(ValidationContext *context,
                             GLuint shader,
                             GLsizei bufsize,
                             GLsizei *length,
                             GLchar *source);
bool ValidateGetUniformLocation(ValidationContext *context, GLuint program, const GLchar *name);
bool ValidateHint(ValidationContext *context, GLenum target, GLenum mode);
bool ValidateIsBuffer(ValidationContext *context, GLuint buffer);
bool ValidateIsFramebuffer(ValidationContext *context, GLuint framebuffer);
bool ValidateIsProgram(ValidationContext *context, GLuint program);
bool ValidateIsRenderbuffer(ValidationContext *context, GLuint renderbuffer);
bool ValidateIsShader(ValidationContext *context, GLuint shader);
bool ValidateIsTexture(ValidationContext *context, GLuint texture);
bool ValidatePixelStorei(ValidationContext *context, GLenum pname, GLint param);
bool ValidatePolygonOffset(ValidationContext *context, GLfloat factor, GLfloat units);
bool ValidateReleaseShaderCompiler(ValidationContext *context);
bool ValidateSampleCoverage(ValidationContext *context, GLclampf value, GLboolean invert);
bool ValidateScissor(ValidationContext *context, GLint x, GLint y, GLsizei width, GLsizei height);
bool ValidateShaderBinary(ValidationContext *context,
                          GLsizei n,
                          const GLuint *shaders,
                          GLenum binaryformat,
                          const GLvoid *binary,
                          GLsizei length);
bool ValidateShaderSource(ValidationContext *context,
                          GLuint shader,
                          GLsizei count,
                          const GLchar *const *string,
                          const GLint *length);
bool ValidateStencilFunc(ValidationContext *context, GLenum func, GLint ref, GLuint mask);
bool ValidateStencilFuncSeparate(ValidationContext *context,
                                 GLenum face,
                                 GLenum func,
                                 GLint ref,
                                 GLuint mask);
bool ValidateStencilMask(ValidationContext *context, GLuint mask);
bool ValidateStencilMaskSeparate(ValidationContext *context, GLenum face, GLuint mask);
bool ValidateStencilOp(ValidationContext *context, GLenum fail, GLenum zfail, GLenum zpass);
bool ValidateStencilOpSeparate(ValidationContext *context,
                               GLenum face,
                               GLenum fail,
                               GLenum zfail,
                               GLenum zpass);
bool ValidateUniform1f(ValidationContext *context, GLint location, GLfloat x);
bool ValidateUniform1fv(ValidationContext *context,
                        GLint location,
                        GLsizei count,
                        const GLfloat *v);
bool ValidateUniform2f(ValidationContext *context, GLint location, GLfloat x, GLfloat y);
bool ValidateUniform2fv(ValidationContext *context,
                        GLint location,
                        GLsizei count,
                        const GLfloat *v);
bool ValidateUniform2i(ValidationContext *context, GLint location, GLint x, GLint y);
bool ValidateUniform2iv(ValidationContext *context, GLint location, GLsizei count, const GLint *v);
bool ValidateUniform3f(ValidationContext *context, GLint location, GLfloat x, GLfloat y, GLfloat z);
bool ValidateUniform3fv(ValidationContext *context,
                        GLint location,
                        GLsizei count,
                        const GLfloat *v);
bool ValidateUniform3i(ValidationContext *context, GLint location, GLint x, GLint y, GLint z);
bool ValidateUniform3iv(ValidationContext *context, GLint location, GLsizei count, const GLint *v);
bool ValidateUniform4f(ValidationContext *context,
                       GLint location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z,
                       GLfloat w);
bool ValidateUniform4fv(ValidationContext *context,
                        GLint location,
                        GLsizei count,
                        const GLfloat *v);
bool ValidateUniform4i(ValidationContext *context,
                       GLint location,
                       GLint x,
                       GLint y,
                       GLint z,
                       GLint w);
bool ValidateUniform4iv(ValidationContext *context, GLint location, GLsizei count, const GLint *v);
bool ValidateUniformMatrix2fv(ValidationContext *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value);
bool ValidateUniformMatrix3fv(ValidationContext *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value);
bool ValidateUniformMatrix4fv(ValidationContext *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat *value);
bool ValidateValidateProgram(ValidationContext *context, GLuint program);
bool ValidateVertexAttrib1f(ValidationContext *context, GLuint index, GLfloat x);
bool ValidateVertexAttrib1fv(ValidationContext *context, GLuint index, const GLfloat *values);
bool ValidateVertexAttrib2f(ValidationContext *context, GLuint index, GLfloat x, GLfloat y);
bool ValidateVertexAttrib2fv(ValidationContext *context, GLuint index, const GLfloat *values);
bool ValidateVertexAttrib3f(ValidationContext *context,
                            GLuint index,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z);
bool ValidateVertexAttrib3fv(ValidationContext *context, GLuint index, const GLfloat *values);
bool ValidateVertexAttrib4f(ValidationContext *context,
                            GLuint index,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z,
                            GLfloat w);
bool ValidateVertexAttrib4fv(ValidationContext *context, GLuint index, const GLfloat *values);
bool ValidateViewport(ValidationContext *context, GLint x, GLint y, GLsizei width, GLsizei height);

bool ValidateDrawArrays(ValidationContext *context, GLenum mode, GLint first, GLsizei count);

}  // namespace gl

#endif // LIBANGLE_VALIDATION_ES2_H_
