//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL41.cpp: Validation functions for OpenGL 4.1 entry point parameters

#include "libANGLE/validationGL41_autogen.h"

namespace gl
{

bool ValidateDepthRangeArrayv(const Context *context,
                              GLuint first,
                              GLsizei count,
                              const GLdouble *v)
{
    return true;
}

bool ValidateDepthRangeIndexed(const Context *context, GLuint index, GLdouble n, GLdouble f)
{
    return true;
}

bool ValidateGetDoublei_v(const Context *context, GLenum target, GLuint index, const GLdouble *data)
{
    return true;
}

bool ValidateGetFloati_v(const Context *context, GLenum target, GLuint index, const GLfloat *data)
{
    return true;
}

bool ValidateGetVertexAttribLdv(const Context *context,
                                GLuint index,
                                GLenum pname,
                                const GLdouble *params)
{
    return true;
}

bool ValidateProgramUniform1d(const Context *context,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0)
{
    return true;
}

bool ValidateProgramUniform1dv(const Context *context,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform2d(const Context *context,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0,
                              GLdouble v1)
{
    return true;
}

bool ValidateProgramUniform2dv(const Context *context,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform3d(const Context *context,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0,
                              GLdouble v1,
                              GLdouble v2)
{
    return true;
}

bool ValidateProgramUniform3dv(const Context *context,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform4d(const Context *context,
                              ShaderProgramID program,
                              UniformLocation location,
                              GLdouble v0,
                              GLdouble v1,
                              GLdouble v2,
                              GLdouble v3)
{
    return true;
}

bool ValidateProgramUniform4dv(const Context *context,
                               ShaderProgramID program,
                               UniformLocation location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2dv(const Context *context,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2x3dv(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2x4dv(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3dv(const Context *context,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3x2dv(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3x4dv(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4dv(const Context *context,
                                     ShaderProgramID program,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4x2dv(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4x3dv(const Context *context,
                                       ShaderProgramID program,
                                       UniformLocation location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateScissorArrayv(const Context *context, GLuint first, GLsizei count, const GLint *v)
{
    return true;
}

bool ValidateScissorIndexed(const Context *context,
                            GLuint index,
                            GLint left,
                            GLint bottom,
                            GLsizei width,
                            GLsizei height)
{
    return true;
}

bool ValidateScissorIndexedv(const Context *context, GLuint index, const GLint *v)
{
    return true;
}

bool ValidateVertexAttribL1d(const Context *context, GLuint index, GLdouble x)
{
    return true;
}

bool ValidateVertexAttribL1dv(const Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL2d(const Context *context, GLuint index, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateVertexAttribL2dv(const Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL3d(const Context *context,
                             GLuint index,
                             GLdouble x,
                             GLdouble y,
                             GLdouble z)
{
    return true;
}

bool ValidateVertexAttribL3dv(const Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL4d(const Context *context,
                             GLuint index,
                             GLdouble x,
                             GLdouble y,
                             GLdouble z,
                             GLdouble w)
{
    return true;
}

bool ValidateVertexAttribL4dv(const Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribLPointer(const Context *context,
                                  GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLsizei stride,
                                  const void *pointer)
{
    return true;
}

bool ValidateViewportArrayv(const Context *context, GLuint first, GLsizei count, const GLfloat *v)
{
    return true;
}

bool ValidateViewportIndexedf(const Context *context,
                              GLuint index,
                              GLfloat x,
                              GLfloat y,
                              GLfloat w,
                              GLfloat h)
{
    return true;
}

bool ValidateViewportIndexedfv(const Context *context, GLuint index, const GLfloat *v)
{
    return true;
}

}  // namespace gl
