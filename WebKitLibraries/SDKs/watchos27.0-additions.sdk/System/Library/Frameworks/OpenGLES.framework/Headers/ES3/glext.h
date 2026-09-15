#ifndef __gl_es30ext_h_
#define __gl_es30ext_h_

#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/OpenGLESAvailability.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.0 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
**
** http://oss.sgi.com/projects/FreeB
**
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
**
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
**
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
*/

#ifndef GL_APIENTRYP
#   define GL_APIENTRYP GL_APIENTRY*
#endif

#define GL_APPLE_clip_distance                                  1
#define GL_APPLE_color_buffer_packed_float                      1
#define GL_APPLE_copy_texture_levels                            1
#define GL_APPLE_rgb_422                                        1
#define GL_APPLE_texture_format_BGRA8888                        1
#define GL_EXT_color_buffer_half_float                          1
#define GL_EXT_debug_label                                      1
#define GL_EXT_debug_marker                                     1
#define GL_EXT_pvrtc_sRGB                                       1
#define GL_EXT_read_format_bgra                                 1
#define GL_EXT_separate_shader_objects                          1
#define GL_EXT_shader_framebuffer_fetch                         1
#define GL_EXT_shader_texture_lod                               1
#define GL_EXT_shadow_samplers                                  1
#define GL_EXT_shader_framebuffer_fetch                         1
#define GL_EXT_texture_filter_anisotropic                       1
#define GL_IMG_read_format                                      1
#define GL_IMG_texture_compression_pvrtc                        1
#define GL_KHR_texture_compression_astc_ldr                     1
#define GL_OES_standard_derivatives                             1

/*------------------------------------------------------------------------*
 * APPLE extension tokens
 *------------------------------------------------------------------------*/
#if GL_APPLE_color_buffer_packed_float
#define GL_R11F_G11F_B10F_APPLE                                 0x8C3A
#define GL_RGB9_E5_APPLE                                        0x8C3D
#endif

#if GL_APPLE_clip_distance
#define GL_CLIP_DISTANCE0_APPLE           0x3000
#define GL_CLIP_DISTANCE1_APPLE           0x3001
#define GL_CLIP_DISTANCE2_APPLE           0x3002
#define GL_CLIP_DISTANCE3_APPLE           0x3003
#define GL_CLIP_DISTANCE4_APPLE           0x3004
#define GL_CLIP_DISTANCE5_APPLE           0x3005
#define GL_CLIP_DISTANCE6_APPLE           0x3006
#define GL_CLIP_DISTANCE7_APPLE           0x3007
#define GL_MAX_CLIP_DISTANCES_APPLE       0x0D32
#endif

#if GL_APPLE_rgb_422
#define GL_RGB_422_APPLE                                        0x8A1F
#define GL_UNSIGNED_SHORT_8_8_APPLE                             0x85BA
#define GL_UNSIGNED_SHORT_8_8_REV_APPLE                         0x85BB
#define GL_RGB_RAW_422_APPLE                                    0x8A51
#endif

#if GL_APPLE_texture_format_BGRA8888
#define GL_BGRA_EXT                                             0x80E1
#define GL_BGRA8_EXT                                            0x93A1
#endif

#if GL_APPLE_texture_format_BGRA8888 || GL_IMG_read_format
#define GL_BGRA                                                 0x80E1
#endif

/*------------------------------------------------------------------------*
 * EXT extension tokens
 *------------------------------------------------------------------------*/
#if GL_EXT_color_buffer_half_float
#define GL_RGBA16F_EXT                                          0x881A
#define GL_RGB16F_EXT                                           0x881B
#define GL_RG16F_EXT                                            0x822F
#define GL_R16F_EXT                                             0x822D
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT            0x8211
#define GL_UNSIGNED_NORMALIZED_EXT                              0x8C17
#endif

#if GL_EXT_debug_label
#define GL_BUFFER_OBJECT_EXT                                    0x9151
#define GL_SHADER_OBJECT_EXT                                    0x8B48
#define GL_PROGRAM_OBJECT_EXT                                   0x8B40
#define GL_QUERY_OBJECT_EXT                                     0x9153
#define GL_VERTEX_ARRAY_OBJECT_EXT                              0x9154
#define GL_PROGRAM_PIPELINE_OBJECT_EXT                          0x8A4F
#define GL_SYNC_OBJECT_APPLE                                    0x8A53
#define GL_SAMPLER                                              0x82E6
#endif

#if GL_EXT_pvrtc_sRGB
#define GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT                   0x8A54
#define GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT                   0x8A55
#define GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT             0x8A56
#define GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT             0x8A57
#endif

#if GL_EXT_read_format_bgra
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT                       0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT                       0x8366
#define GL_UNSIGNED_SHORT_1_5_5_5_REV                           0x8366
#endif

#if GL_EXT_read_format_bgra || GL_IMG_read_format
#define GL_UNSIGNED_SHORT_4_4_4_4_REV                           0x8365
#endif

#if GL_EXT_separate_shader_objects
#define GL_VERTEX_SHADER_BIT_EXT                                0x00000001
#define GL_FRAGMENT_SHADER_BIT_EXT                              0x00000002
#define GL_ALL_SHADER_BITS_EXT                                  0xFFFFFFFF
#define GL_PROGRAM_SEPARABLE_EXT                                0x8258
#define GL_ACTIVE_PROGRAM_EXT                                   0x8259
#define GL_PROGRAM_PIPELINE_BINDING_EXT                         0x825A
#endif

#if GL_EXT_shader_framebuffer_fetch
#define GL_FRAGMENT_SHADER_DISCARDS_SAMPLES_EXT                 0x8A52
#endif

#if GL_EXT_shadow_samplers
#define GL_TEXTURE_COMPARE_MODE_EXT                             0x884C
#define GL_TEXTURE_COMPARE_FUNC_EXT                             0x884D
#define GL_COMPARE_REF_TO_TEXTURE_EXT                           0x884E
#define GL_SAMPLER_2D_SHADOW_EXT                                0x8B62
#endif

#if GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT                           0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT                       0x84FF
#endif


/*------------------------------------------------------------------------*
 * IMG extension tokens
 *------------------------------------------------------------------------*/
#if GL_IMG_read_format
#define GL_BGRA_IMG                                             0x80E1
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG                       0x8365
#endif

#if GL_IMG_texture_compression_pvrtc
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG                      0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG                      0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG                     0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG                     0x8C03
#endif

/*------------------------------------------------------------------------*
 * KHR extension tokens
 *------------------------------------------------------------------------*/
#if GL_KHR_texture_compression_astc_ldr
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR                         0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR                         0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR                         0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR                         0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR                         0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR                         0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR                         0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR                         0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR                        0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR                        0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR                        0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR                       0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR                       0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR                       0x93BD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR                 0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR                 0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR                 0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR                 0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR                 0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR                 0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR                 0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR                 0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR                0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR                0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR                0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR               0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR               0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR               0x93DD
#endif

/*------------------------------------------------------------------------*
 * OES extension tokens
 *------------------------------------------------------------------------*/
#if GL_OES_standard_derivatives
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES                  0x8B8B
#endif

/**************************************************************************/

/*------------------------------------------------------------------------*
 * APPLE extension functions
 *------------------------------------------------------------------------*/
#if GL_APPLE_copy_texture_levels
GL_API GLvoid glCopyTextureLevelsAPPLE(GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount)  OPENGLES_DEPRECATED(ios(6.0, 12.0), tvos(9.0, 12.0));
#endif

/*------------------------------------------------------------------------*
 * EXT extension functions
 *------------------------------------------------------------------------*/
#if GL_EXT_debug_label
GL_API GLvoid glLabelObjectEXT(GLenum type, GLuint object, GLsizei length, const GLchar *label)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glGetObjectLabelEXT(GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
#endif

#if GL_EXT_debug_marker
GL_API GLvoid glInsertEventMarkerEXT(GLsizei length, const GLchar *marker)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glPushGroupMarkerEXT(GLsizei length, const GLchar *marker)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glPopGroupMarkerEXT(void)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
#endif

#if GL_EXT_separate_shader_objects
GL_API GLvoid glUseProgramStagesEXT(GLuint pipeline, GLbitfield stages, GLuint program)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glActiveShaderProgramEXT(GLuint pipeline, GLuint program)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLuint glCreateShaderProgramvEXT(GLenum type, GLsizei count, const GLchar* const *strings)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glBindProgramPipelineEXT(GLuint pipeline)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glDeleteProgramPipelinesEXT(GLsizei n, const GLuint *pipelines)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glGenProgramPipelinesEXT(GLsizei n, GLuint *pipelines)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLboolean glIsProgramPipelineEXT(GLuint pipeline)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramParameteriEXT(GLuint program, GLenum pname, GLint value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glGetProgramPipelineivEXT(GLuint pipeline, GLenum pname, GLint *params)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glValidateProgramPipelineEXT(GLuint pipeline)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glGetProgramPipelineInfoLogEXT(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniform1iEXT(GLuint program, GLint location, GLint x)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform2iEXT(GLuint program, GLint location, GLint x, GLint y)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform3iEXT(GLuint program, GLint location, GLint x, GLint y, GLint z)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform4iEXT(GLuint program, GLint location, GLint x, GLint y, GLint z, GLint w)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniform1fEXT(GLuint program, GLint location, GLfloat x)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform2fEXT(GLuint program, GLint location, GLfloat x, GLfloat y)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform3fEXT(GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform4fEXT(GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniform1uiEXT(GLuint program, GLint location, GLuint x)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform2uiEXT(GLuint program, GLint location, GLuint x, GLuint y)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform3uiEXT(GLuint program, GLint location, GLuint x, GLuint y, GLuint z)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform4uiEXT(GLuint program, GLint location, GLuint x, GLuint y, GLuint z, GLuint w)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count, const GLint *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniform1uivEXT(GLuint program, GLint location, GLsizei count, const GLuint *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform2uivEXT(GLuint program, GLint location, GLsizei count, const GLuint *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform3uivEXT(GLuint program, GLint location, GLsizei count, const GLuint *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniform4uivEXT(GLuint program, GLint location, GLsizei count, const GLuint *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));

GL_API GLvoid glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(5.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix2x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix3x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix2x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix4x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix3x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
GL_API GLvoid glProgramUniformMatrix4x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)   OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));
#endif

#ifdef __cplusplus
}
#endif

#endif /* __gl_es30ext_h_ */
