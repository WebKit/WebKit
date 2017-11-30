//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_gles_2_0_ext.h : Defines the GLES 2.0 extension entry points.

#ifndef LIBGLESV2_ENTRYPOINTGLES20EXT_H_
#define LIBGLESV2_ENTRYPOINTGLES20EXT_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <export.h>

namespace gl
{

// GL_ANGLE_framebuffer_blit
ANGLE_EXPORT void GL_APIENTRY BlitFramebufferANGLE(GLint srcX0,
                                                   GLint srcY0,
                                                   GLint srcX1,
                                                   GLint srcY1,
                                                   GLint dstX0,
                                                   GLint dstY0,
                                                   GLint dstX1,
                                                   GLint dstY1,
                                                   GLbitfield mask,
                                                   GLenum filter);

// GL_ANGLE_framebuffer_multisample
ANGLE_EXPORT void GL_APIENTRY RenderbufferStorageMultisampleANGLE(GLenum target,
                                                                  GLsizei samples,
                                                                  GLenum internalformat,
                                                                  GLsizei width,
                                                                  GLsizei height);

// GL_EXT_discard_framebuffer
ANGLE_EXPORT void GL_APIENTRY DiscardFramebufferEXT(GLenum target,
                                                    GLsizei numAttachments,
                                                    const GLenum *attachments);

// GL_NV_fence
ANGLE_EXPORT void GL_APIENTRY DeleteFencesNV(GLsizei n, const GLuint *fences);
ANGLE_EXPORT void GL_APIENTRY GenFencesNV(GLsizei n, GLuint *fences);
ANGLE_EXPORT GLboolean GL_APIENTRY IsFenceNV(GLuint fence);
ANGLE_EXPORT GLboolean GL_APIENTRY TestFenceNV(GLuint fence);
ANGLE_EXPORT void GL_APIENTRY GetFenceivNV(GLuint fence, GLenum pname, GLint *params);
ANGLE_EXPORT void GL_APIENTRY FinishFenceNV(GLuint fence);
ANGLE_EXPORT void GL_APIENTRY SetFenceNV(GLuint fence, GLenum condition);

// GL_ANGLE_translated_shader_source
ANGLE_EXPORT void GL_APIENTRY GetTranslatedShaderSourceANGLE(GLuint shader,
                                                             GLsizei bufsize,
                                                             GLsizei *length,
                                                             GLchar *source);

// GL_EXT_texture_storage
ANGLE_EXPORT void GL_APIENTRY TexStorage2DEXT(GLenum target,
                                              GLsizei levels,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height);

// GL_EXT_robustness
ANGLE_EXPORT GLenum GL_APIENTRY GetGraphicsResetStatusEXT(void);
ANGLE_EXPORT void GL_APIENTRY ReadnPixelsEXT(GLint x,
                                             GLint y,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             GLsizei bufSize,
                                             void *data);
ANGLE_EXPORT void GL_APIENTRY GetnUniformfvEXT(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               float *params);
ANGLE_EXPORT void GL_APIENTRY GetnUniformivEXT(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               GLint *params);

// GL_EXT_occlusion_query_boolean
ANGLE_EXPORT void GL_APIENTRY GenQueriesEXT(GLsizei n, GLuint *ids);
ANGLE_EXPORT void GL_APIENTRY DeleteQueriesEXT(GLsizei n, const GLuint *ids);
ANGLE_EXPORT GLboolean GL_APIENTRY IsQueryEXT(GLuint id);
ANGLE_EXPORT void GL_APIENTRY BeginQueryEXT(GLenum target, GLuint id);
ANGLE_EXPORT void GL_APIENTRY EndQueryEXT(GLenum target);
ANGLE_EXPORT void GL_APIENTRY GetQueryivEXT(GLenum target, GLenum pname, GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint *params);

// GL_EXT_disjoint_timer_query
ANGLE_EXPORT void GL_APIENTRY QueryCounterEXT(GLuint id, GLenum target);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectivEXT(GLuint id, GLenum pname, GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 *params);

// GL_EXT_draw_buffers
ANGLE_EXPORT void GL_APIENTRY DrawBuffersEXT(GLsizei n, const GLenum *bufs);

// GL_ANGLE_instanced_arrays
ANGLE_EXPORT void GL_APIENTRY DrawArraysInstancedANGLE(GLenum mode,
                                                       GLint first,
                                                       GLsizei count,
                                                       GLsizei primcount);
ANGLE_EXPORT void GL_APIENTRY DrawElementsInstancedANGLE(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void *indices,
                                                         GLsizei primcount);
ANGLE_EXPORT void GL_APIENTRY VertexAttribDivisorANGLE(GLuint index, GLuint divisor);

// GL_OES_get_program_binary
ANGLE_EXPORT void GL_APIENTRY GetProgramBinaryOES(GLuint program,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLenum *binaryFormat,
                                                  void *binary);
ANGLE_EXPORT void GL_APIENTRY ProgramBinaryOES(GLuint program,
                                               GLenum binaryFormat,
                                               const void *binary,
                                               GLint length);

// GL_OES_mapbuffer
ANGLE_EXPORT void *GL_APIENTRY MapBufferOES(GLenum target, GLenum access);
ANGLE_EXPORT GLboolean GL_APIENTRY UnmapBufferOES(GLenum target);
ANGLE_EXPORT void GL_APIENTRY GetBufferPointervOES(GLenum target, GLenum pname, void **params);

// GL_EXT_map_buffer_range
ANGLE_EXPORT void *GL_APIENTRY MapBufferRangeEXT(GLenum target,
                                                 GLintptr offset,
                                                 GLsizeiptr length,
                                                 GLbitfield access);
ANGLE_EXPORT void GL_APIENTRY FlushMappedBufferRangeEXT(GLenum target,
                                                        GLintptr offset,
                                                        GLsizeiptr length);

// GL_EXT_debug_marker
ANGLE_EXPORT void GL_APIENTRY InsertEventMarkerEXT(GLsizei length, const char *marker);
ANGLE_EXPORT void GL_APIENTRY PushGroupMarkerEXT(GLsizei length, const char *marker);
ANGLE_EXPORT void GL_APIENTRY PopGroupMarkerEXT();

// GL_OES_EGL_image
ANGLE_EXPORT void GL_APIENTRY EGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image);
ANGLE_EXPORT void GL_APIENTRY EGLImageTargetRenderbufferStorageOES(GLenum target,
                                                                   GLeglImageOES image);

// GL_OES_vertex_array_object
ANGLE_EXPORT void GL_APIENTRY BindVertexArrayOES(GLuint array);
ANGLE_EXPORT void GL_APIENTRY DeleteVertexArraysOES(GLsizei n, const GLuint *arrays);
ANGLE_EXPORT void GL_APIENTRY GenVertexArraysOES(GLsizei n, GLuint *arrays);
ANGLE_EXPORT GLboolean GL_APIENTRY IsVertexArrayOES(GLuint array);

// GL_KHR_debug
ANGLE_EXPORT void GL_APIENTRY DebugMessageControlKHR(GLenum source,
                                                     GLenum type,
                                                     GLenum severity,
                                                     GLsizei count,
                                                     const GLuint *ids,
                                                     GLboolean enabled);
ANGLE_EXPORT void GL_APIENTRY DebugMessageInsertKHR(GLenum source,
                                                    GLenum type,
                                                    GLuint id,
                                                    GLenum severity,
                                                    GLsizei length,
                                                    const GLchar *buf);
ANGLE_EXPORT void GL_APIENTRY DebugMessageCallbackKHR(GLDEBUGPROCKHR callback,
                                                      const void *userParam);
ANGLE_EXPORT GLuint GL_APIENTRY GetDebugMessageLogKHR(GLuint count,
                                                      GLsizei bufSize,
                                                      GLenum *sources,
                                                      GLenum *types,
                                                      GLuint *ids,
                                                      GLenum *severities,
                                                      GLsizei *lengths,
                                                      GLchar *messageLog);
ANGLE_EXPORT void GL_APIENTRY PushDebugGroupKHR(GLenum source,
                                                GLuint id,
                                                GLsizei length,
                                                const GLchar *message);
ANGLE_EXPORT void GL_APIENTRY PopDebugGroupKHR(void);
ANGLE_EXPORT void GL_APIENTRY ObjectLabelKHR(GLenum identifier,
                                             GLuint name,
                                             GLsizei length,
                                             const GLchar *label);
ANGLE_EXPORT void GL_APIENTRY
GetObjectLabelKHR(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label);
ANGLE_EXPORT void GL_APIENTRY ObjectPtrLabelKHR(const void *ptr,
                                                GLsizei length,
                                                const GLchar *label);
ANGLE_EXPORT void GL_APIENTRY GetObjectPtrLabelKHR(const void *ptr,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLchar *label);
ANGLE_EXPORT void GL_APIENTRY GetPointervKHR(GLenum pname, void **params);

// GL_CHROMIUM_bind_uniform_location
ANGLE_EXPORT void GL_APIENTRY BindUniformLocationCHROMIUM(GLuint program,
                                                          GLint location,
                                                          const GLchar *name);

// GL_CHROMIUM_framebuffer_mixed_samples
ANGLE_EXPORT void GL_APIENTRY MatrixLoadfCHROMIUM(GLenum matrixMode, const GLfloat *matrix);
ANGLE_EXPORT void GL_APIENTRY MatrixLoadIdentityCHROMIUM(GLenum matrixMode);

ANGLE_EXPORT void GL_APIENTRY CoverageModulationCHROMIUM(GLenum components);

// GL_CHROMIUM_path_rendering
ANGLE_EXPORT GLuint GL_APIENTRY GenPathsCHROMIUM(GLsizei chromium);
ANGLE_EXPORT void GL_APIENTRY DeletePathsCHROMIUM(GLuint first, GLsizei range);
ANGLE_EXPORT GLboolean GL_APIENTRY IsPathCHROMIUM(GLuint path);
ANGLE_EXPORT void GL_APIENTRY PathCommandsCHROMIUM(GLuint path,
                                                   GLsizei numCommands,
                                                   const GLubyte *commands,
                                                   GLsizei numCoords,
                                                   GLenum coordType,
                                                   const void *coords);
ANGLE_EXPORT void GL_APIENTRY PathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value);
ANGLE_EXPORT void GL_APIENTRY PathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value);
ANGLE_EXPORT void GL_APIENTRY GetPathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat *value);
ANGLE_EXPORT void GL_APIENTRY GetPathParameteriCHROMIUM(GLuint path, GLenum pname, GLint *value);
ANGLE_EXPORT void GL_APIENTRY PathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask);
ANGLE_EXPORT void GL_APIENTRY StencilFillPathCHROMIUM(GLuint path, GLenum fillMode, GLuint mask);
ANGLE_EXPORT void GL_APIENTRY StencilStrokePathCHROMIUM(GLuint path, GLint reference, GLuint mask);
ANGLE_EXPORT void GL_APIENTRY CoverFillPathCHROMIUM(GLuint path, GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY CoverStrokePathCHROMIUM(GLuint path, GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY StencilThenCoverFillPathCHROMIUM(GLuint path,
                                                               GLenum fillMode,
                                                               GLuint mask,
                                                               GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY StencilThenCoverStrokePathCHROMIUM(GLuint path,
                                                                 GLint reference,
                                                                 GLuint mask,
                                                                 GLenum coverMode);
ANGLE_EXPORT void GL_APIENTRY CoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                             GLenum pathNameType,
                                                             const void *paths,
                                                             GLuint pathBase,
                                                             GLenum coverMode,
                                                             GLenum transformType,
                                                             const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY CoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBase,
                                                               GLenum coverMode,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY StencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBAse,
                                                               GLenum fillMode,
                                                               GLuint mask,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY StencilStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                                 GLenum pathNameType,
                                                                 const void *paths,
                                                                 GLuint pathBase,
                                                                 GLint reference,
                                                                 GLuint mask,
                                                                 GLenum transformType,
                                                                 const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY
StencilThenCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                          GLenum pathNameType,
                                          const void *paths,
                                          GLuint pathBase,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum coverMode,
                                          GLenum transformType,
                                          const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY
StencilThenCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                            GLenum pathNameType,
                                            const void *paths,
                                            GLuint pathBase,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat *transformValues);
ANGLE_EXPORT void GL_APIENTRY BindFragmentInputLocationCHROMIUM(GLuint program,
                                                                GLint location,
                                                                const GLchar *name);
ANGLE_EXPORT void GL_APIENTRY ProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                                                  GLint location,
                                                                  GLenum genMode,
                                                                  GLint components,
                                                                  const GLfloat *coeffs);

// GL_CHROMIUM_copy_texture
ANGLE_EXPORT void GL_APIENTRY CopyTextureCHROMIUM(GLuint sourceId,
                                                  GLint sourceLevel,
                                                  GLenum destTarget,
                                                  GLuint destId,
                                                  GLint destLevel,
                                                  GLint internalFormat,
                                                  GLenum destType,
                                                  GLboolean unpackFlipY,
                                                  GLboolean unpackPremultiplyAlpha,
                                                  GLboolean unpackUnmultiplyAlpha);

ANGLE_EXPORT void GL_APIENTRY CopySubTextureCHROMIUM(GLuint sourceId,
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

// GL_CHROMIUM_copy_compressed_texture
ANGLE_EXPORT void GL_APIENTRY CompressedCopyTextureCHROMIUM(GLuint sourceId, GLuint destId);

// GL_ANGLE_request_extension
ANGLE_EXPORT void GL_APIENTRY RequestExtensionANGLE(const GLchar *name);

// GL_ANGLE_robust_client_memory
ANGLE_EXPORT void GL_APIENTRY GetBooleanvRobustANGLE(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLboolean *data);
ANGLE_EXPORT void GL_APIENTRY GetBufferParameterivRobustANGLE(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei *length,
                                                              GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetFloatvRobustANGLE(GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLfloat *data);
ANGLE_EXPORT void GL_APIENTRY GetFramebufferAttachmentParameterivRobustANGLE(GLenum target,
                                                                             GLenum attachment,
                                                                             GLenum pname,
                                                                             GLsizei bufSize,
                                                                             GLsizei *length,
                                                                             GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetIntegervRobustANGLE(GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *data);
ANGLE_EXPORT void GL_APIENTRY GetProgramivRobustANGLE(GLuint program,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetRenderbufferParameterivRobustANGLE(GLenum target,
                                                                    GLenum pname,
                                                                    GLsizei bufSize,
                                                                    GLsizei *length,
                                                                    GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetShaderivRobustANGLE(GLuint shader,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterfvRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterivRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetUniformfvRobustANGLE(GLuint program,
                                                      GLint location,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetUniformivRobustANGLE(GLuint program,
                                                      GLint location,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribfvRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribivRobustANGLE(GLuint index,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribPointervRobustANGLE(GLuint index,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 void **pointer);
ANGLE_EXPORT void GL_APIENTRY ReadPixelsRobustANGLE(GLint x,
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
ANGLE_EXPORT void GL_APIENTRY TexImage2DRobustANGLE(GLenum target,
                                                    GLint level,
                                                    GLint internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLenum format,
                                                    GLenum type,
                                                    GLsizei bufSize,
                                                    const void *pixels);
ANGLE_EXPORT void GL_APIENTRY TexParameterfvRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY TexParameterivRobustANGLE(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLint *params);
ANGLE_EXPORT void GL_APIENTRY TexSubImage2DRobustANGLE(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufSize,
                                                       const void *pixels);

ANGLE_EXPORT void GL_APIENTRY TexImage3DRobustANGLE(GLenum target,
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
ANGLE_EXPORT void GL_APIENTRY TexSubImage3DRobustANGLE(GLenum target,
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

ANGLE_EXPORT void GL_APIENTRY CompressedTexImage2DRobustANGLE(GLenum target,
                                                              GLint level,
                                                              GLenum internalformat,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              GLint border,
                                                              GLsizei imageSize,
                                                              GLsizei dataSize,
                                                              const GLvoid *data);
ANGLE_EXPORT void GL_APIENTRY CompressedTexSubImage2DRobustANGLE(GLenum target,
                                                                 GLint level,
                                                                 GLint xoffset,
                                                                 GLint yoffset,
                                                                 GLsizei width,
                                                                 GLsizei height,
                                                                 GLenum format,
                                                                 GLsizei imageSize,
                                                                 GLsizei dataSize,
                                                                 const GLvoid *data);
ANGLE_EXPORT void GL_APIENTRY CompressedTexImage3DRobustANGLE(GLenum target,
                                                              GLint level,
                                                              GLenum internalformat,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              GLsizei depth,
                                                              GLint border,
                                                              GLsizei imageSize,
                                                              GLsizei dataSize,
                                                              const GLvoid *data);
ANGLE_EXPORT void GL_APIENTRY CompressedTexSubImage3DRobustANGLE(GLenum target,
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
                                                                 const GLvoid *data);

ANGLE_EXPORT void GL_APIENTRY
GetQueryivRobustANGLE(GLenum target, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectuivRobustANGLE(GLuint id,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetBufferPointervRobustANGLE(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei *length,
                                                           void **params);
ANGLE_EXPORT void GL_APIENTRY GetIntegeri_vRobustANGLE(GLenum target,
                                                       GLuint index,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint *data);
ANGLE_EXPORT void GL_APIENTRY GetInternalformativRobustANGLE(GLenum target,
                                                             GLenum internalformat,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribIivRobustANGLE(GLuint index,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetVertexAttribIuivRobustANGLE(GLuint index,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetUniformuivRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetActiveUniformBlockivRobustANGLE(GLuint program,
                                                                 GLuint uniformBlockIndex,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetInteger64vRobustANGLE(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint64 *data);
ANGLE_EXPORT void GL_APIENTRY GetInteger64i_vRobustANGLE(GLenum target,
                                                         GLuint index,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint64 *data);
ANGLE_EXPORT void GL_APIENTRY GetBufferParameteri64vRobustANGLE(GLenum target,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint64 *params);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterivRobustANGLE(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            const GLint *param);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterfvRobustANGLE(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            const GLfloat *param);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterivRobustANGLE(GLuint sampler,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterfvRobustANGLE(GLuint sampler,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLfloat *params);

ANGLE_EXPORT void GL_APIENTRY GetFramebufferParameterivRobustANGLE(GLenum target,
                                                                   GLenum pname,
                                                                   GLsizei bufSize,
                                                                   GLsizei *length,
                                                                   GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetProgramInterfaceivRobustANGLE(GLuint program,
                                                               GLenum programInterface,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei *length,
                                                               GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetBooleani_vRobustANGLE(GLenum target,
                                                       GLuint index,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLboolean *data);
ANGLE_EXPORT void GL_APIENTRY GetMultisamplefvRobustANGLE(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLfloat *val);
ANGLE_EXPORT void GL_APIENTRY GetTexLevelParameterivRobustANGLE(GLenum target,
                                                                GLint level,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexLevelParameterfvRobustANGLE(GLenum target,
                                                                GLint level,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLfloat *params);

ANGLE_EXPORT void GL_APIENTRY GetPointervRobustANGLERobustANGLE(GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                void **params);
ANGLE_EXPORT void GL_APIENTRY ReadnPixelsRobustANGLE(GLint x,
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
ANGLE_EXPORT void GL_APIENTRY GetnUniformfvRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLfloat *params);
ANGLE_EXPORT void GL_APIENTRY GetnUniformivRobustANGLE(GLuint program,
                                                       GLint location,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetnUniformuivRobustANGLE(GLuint program,
                                                        GLint location,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLuint *params);
ANGLE_EXPORT void GL_APIENTRY TexParameterIivRobustANGLE(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         const GLint *params);
ANGLE_EXPORT void GL_APIENTRY TexParameterIuivRobustANGLE(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          const GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterIivRobustANGLE(GLenum target,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetTexParameterIuivRobustANGLE(GLenum target,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint *params);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterIivRobustANGLE(GLuint sampler,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             const GLint *param);
ANGLE_EXPORT void GL_APIENTRY SamplerParameterIuivRobustANGLE(GLuint sampler,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              const GLuint *param);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterIivRobustANGLE(GLuint sampler,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei *length,
                                                                GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetSamplerParameterIuivRobustANGLE(GLuint sampler,
                                                                 GLenum pname,
                                                                 GLsizei bufSize,
                                                                 GLsizei *length,
                                                                 GLuint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectivRobustANGLE(GLuint id,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei *length,
                                                          GLint *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjecti64vRobustANGLE(GLuint id,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei *length,
                                                            GLint64 *params);
ANGLE_EXPORT void GL_APIENTRY GetQueryObjectui64vRobustANGLE(GLuint id,
                                                             GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei *length,
                                                             GLuint64 *params);

// GL_ANGLE_multiview
ANGLE_EXPORT void GL_APIENTRY FramebufferTextureMultiviewLayeredANGLE(GLenum target,
                                                                      GLenum attachment,
                                                                      GLuint texture,
                                                                      GLint level,
                                                                      GLint baseViewIndex,
                                                                      GLsizei numViews);
ANGLE_EXPORT void GL_APIENTRY
FramebufferTextureMultiviewSideBySideANGLE(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei numViews,
                                           const GLint *viewportOffsets);
}  // namespace gl

#endif  // LIBGLESV2_ENTRYPOINTGLES20EXT_H_
