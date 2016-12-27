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

bool ValidateES2TexImageParameters(Context *context,
                                   GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   bool isCompressed,
                                   bool isSubImage,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei imageSize,
                                   const GLvoid *pixels);

bool ValidateES2CopyTexImageParameters(ValidationContext *context,
                                       GLenum target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border);

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
                                 GLuint destId,
                                 GLint internalFormat,
                                 GLenum destType,
                                 GLboolean unpackFlipY,
                                 GLboolean unpackPremultiplyAlpha,
                                 GLboolean unpackUnmultiplyAlpha);
bool ValidateCopySubTextureCHROMIUM(Context *context,
                                    GLuint sourceId,
                                    GLuint destId,
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

bool ValidateEnableExtensionANGLE(ValidationContext *context, const GLchar *name);

bool ValidateActiveTexture(ValidationContext *context, GLenum texture);
bool ValidateAttachShader(ValidationContext *context, GLuint program, GLuint shader);

}  // namespace gl

#endif // LIBANGLE_VALIDATION_ES2_H_
