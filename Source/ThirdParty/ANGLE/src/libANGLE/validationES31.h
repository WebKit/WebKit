//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES31.h: Validation functions for OpenGL ES 3.1 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES31_H_
#define LIBANGLE_VALIDATION_ES31_H_

#include <GLES3/gl31.h>

namespace gl
{
class Context;
class ValidationContext;

bool ValidateGetBooleani_v(Context *context, GLenum target, GLuint index, GLboolean *data);
bool ValidateGetBooleani_vRobustANGLE(Context *context,
                                      GLenum target,
                                      GLuint index,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLboolean *data);

bool ValidateGetTexLevelParameterfv(Context *context,
                                    GLenum target,
                                    GLint level,
                                    GLenum pname,
                                    GLfloat *params);
bool ValidateGetTexLevelParameteriv(Context *context,
                                    GLenum target,
                                    GLint level,
                                    GLenum pname,
                                    GLint *param);

bool ValidateTexStorage2DMultisample(Context *context,
                                     GLenum target,
                                     GLsizei samples,
                                     GLint internalFormat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLboolean fixedSampleLocations);
bool ValidateGetMultisamplefv(Context *context, GLenum pname, GLuint index, GLfloat *val);

bool ValidateDrawIndirectBase(Context *context, GLenum mode, const void *indirect);
bool ValidateDrawArraysIndirect(Context *context, GLenum mode, const void *indirect);
bool ValidateDrawElementsIndirect(Context *context, GLenum mode, GLenum type, const void *indirect);

bool ValidateProgramUniform1i(Context *context, GLuint program, GLint location, GLint v0);
bool ValidateProgramUniform2i(Context *context, GLuint program, GLint location, GLint v0, GLint v1);
bool ValidateProgramUniform3i(Context *context,
                              GLuint program,
                              GLint location,
                              GLint v0,
                              GLint v1,
                              GLint v2);
bool ValidateProgramUniform4i(Context *context,
                              GLuint program,
                              GLint location,
                              GLint v0,
                              GLint v1,
                              GLint v2,
                              GLint v3);
bool ValidateProgramUniform1ui(Context *context, GLuint program, GLint location, GLuint v0);
bool ValidateProgramUniform2ui(Context *context,
                               GLuint program,
                               GLint location,
                               GLuint v0,
                               GLuint v1);
bool ValidateProgramUniform3ui(Context *context,
                               GLuint program,
                               GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2);
bool ValidateProgramUniform4ui(Context *context,
                               GLuint program,
                               GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2,
                               GLuint v3);
bool ValidateProgramUniform1f(Context *context, GLuint program, GLint location, GLfloat v0);
bool ValidateProgramUniform2f(Context *context,
                              GLuint program,
                              GLint location,
                              GLfloat v0,
                              GLfloat v1);
bool ValidateProgramUniform3f(Context *context,
                              GLuint program,
                              GLint location,
                              GLfloat v0,
                              GLfloat v1,
                              GLfloat v2);
bool ValidateProgramUniform4f(Context *context,
                              GLuint program,
                              GLint location,
                              GLfloat v0,
                              GLfloat v1,
                              GLfloat v2,
                              GLfloat v3);

bool ValidateProgramUniform1iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value);
bool ValidateProgramUniform2iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value);
bool ValidateProgramUniform3iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value);
bool ValidateProgramUniform4iv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLint *value);
bool ValidateProgramUniform1uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value);
bool ValidateProgramUniform2uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value);
bool ValidateProgramUniform3uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value);
bool ValidateProgramUniform4uiv(Context *context,
                                GLuint program,
                                GLint location,
                                GLsizei count,
                                const GLuint *value);
bool ValidateProgramUniform1fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value);
bool ValidateProgramUniform2fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value);
bool ValidateProgramUniform3fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value);
bool ValidateProgramUniform4fv(Context *context,
                               GLuint program,
                               GLint location,
                               GLsizei count,
                               const GLfloat *value);
bool ValidateProgramUniformMatrix2fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value);
bool ValidateProgramUniformMatrix2fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value);
bool ValidateProgramUniformMatrix3fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value);
bool ValidateProgramUniformMatrix4fv(Context *context,
                                     GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value);
bool ValidateProgramUniformMatrix2x3fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value);
bool ValidateProgramUniformMatrix3x2fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value);
bool ValidateProgramUniformMatrix2x4fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value);
bool ValidateProgramUniformMatrix4x2fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value);
bool ValidateProgramUniformMatrix3x4fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value);
bool ValidateProgramUniformMatrix4x3fv(Context *context,
                                       GLuint program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat *value);

bool ValidateFramebufferParameteri(Context *context, GLenum target, GLenum pname, GLint param);
bool ValidateGetFramebufferParameteriv(Context *context,
                                       GLenum target,
                                       GLenum pname,
                                       GLint *params);

bool ValidateGetProgramResourceIndex(Context *context,
                                     GLuint program,
                                     GLenum programInterface,
                                     const GLchar *name);
bool ValidateGetProgramResourceName(Context *context,
                                    GLuint program,
                                    GLenum programInterface,
                                    GLuint index,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *name);
bool ValidateGetProgramResourceLocation(Context *context,
                                        GLuint program,
                                        GLenum programInterface,
                                        const GLchar *name);

bool ValidateGetProgramResourceiv(Context *context,
                                  GLuint program,
                                  GLenum programInterface,
                                  GLuint index,
                                  GLsizei propCount,
                                  const GLenum *props,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params);

bool ValidateGetProgramInterfaceiv(Context *context,
                                   GLuint program,
                                   GLenum programInterface,
                                   GLenum pname,
                                   GLint *params);

bool ValidateBindVertexBuffer(ValidationContext *context,
                              GLuint bindingIndex,
                              GLuint buffer,
                              GLintptr offset,
                              GLsizei stride);
bool ValidateVertexAttribFormat(ValidationContext *context,
                                GLuint attribindex,
                                GLint size,
                                GLenum type,
                                GLboolean normalized,
                                GLuint relativeoffset);
bool ValidateVertexAttribIFormat(ValidationContext *context,
                                 GLuint attribindex,
                                 GLint size,
                                 GLenum type,
                                 GLuint relativeoffset);
bool ValidateVertexAttribBinding(ValidationContext *context,
                                 GLuint attribIndex,
                                 GLuint bindingIndex);
bool ValidateVertexBindingDivisor(ValidationContext *context, GLuint bindingIndex, GLuint divisor);

bool ValidateDispatchCompute(Context *context,
                             GLuint numGroupsX,
                             GLuint numGroupsY,
                             GLuint numGroupsZ);
bool ValidateDispatchComputeIndirect(Context *context, GLintptr indirect);

bool ValidateBindImageTexture(Context *context,
                              GLuint unit,
                              GLuint texture,
                              GLint level,
                              GLboolean layered,
                              GLint layer,
                              GLenum access,
                              GLenum format);

bool ValidateGenProgramPipelines(Context *context, GLint n, GLuint *pipelines);
bool ValidateDeleteProgramPipelines(Context *context, GLint n, const GLuint *pipelines);
bool ValidateBindProgramPipeline(Context *context, GLuint pipeline);
bool ValidateIsProgramPipeline(Context *context, GLuint pipeline);
bool ValidateUseProgramStages(Context *context, GLuint pipeline, GLbitfield stages, GLuint program);
bool ValidateActiveShaderProgram(Context *context, GLuint pipeline, GLuint program);
bool ValidateCreateShaderProgramv(Context *context,
                                  GLenum type,
                                  GLsizei count,
                                  const GLchar *const *strings);
bool ValidateGetProgramPipelineiv(Context *context, GLuint pipeline, GLenum pname, GLint *params);
bool ValidateValidateProgramPipeline(Context *context, GLuint pipeline);
bool ValidateGetProgramPipelineInfoLog(Context *context,
                                       GLuint pipeline,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *infoLog);

bool ValidateMemoryBarrier(Context *context, GLbitfield barriers);
bool ValidateMemoryBarrierByRegion(Context *context, GLbitfield barriers);

bool ValidateSampleMaski(Context *context, GLuint maskNumber, GLbitfield mask);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES31_H_
