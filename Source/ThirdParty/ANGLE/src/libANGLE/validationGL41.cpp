//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL41.cpp: Validation functions for OpenGL 4.1 entry point parameters

#include "libANGLE/validationGL41_autogen.h"

namespace gl
{

bool ValidateDepthRangeArrayv(Context *context, GLuint first, GLsizei count, const GLdouble *v)
{
    return true;
}

bool ValidateDepthRangeIndexed(Context *context, GLuint index, GLdouble n, GLdouble f)
{
    return true;
}

bool ValidateGetDoublei_v(Context *context, GLenum target, GLuint index, GLdouble *data)
{
    return true;
}

bool ValidateGetFloati_v(Context *context, GLenum target, GLuint index, GLfloat *data)
{
    return true;
}

bool ValidateGetVertexAttribLdv(Context *context, GLuint index, GLenum pname, GLdouble *params)
{
    return true;
}

bool ValidateProgramUniform1d(Context *context,
                              ShaderProgramID program,
                              GLint location,
                              GLdouble v0)
{
    return true;
}

bool ValidateProgramUniform1dv(Context *context,
                               ShaderProgramID program,
                               GLint location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform2d(Context *context,
                              ShaderProgramID program,
                              GLint location,
                              GLdouble v0,
                              GLdouble v1)
{
    return true;
}

bool ValidateProgramUniform2dv(Context *context,
                               ShaderProgramID program,
                               GLint location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform3d(Context *context,
                              ShaderProgramID program,
                              GLint location,
                              GLdouble v0,
                              GLdouble v1,
                              GLdouble v2)
{
    return true;
}

bool ValidateProgramUniform3dv(Context *context,
                               ShaderProgramID program,
                               GLint location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniform4d(Context *context,
                              ShaderProgramID program,
                              GLint location,
                              GLdouble v0,
                              GLdouble v1,
                              GLdouble v2,
                              GLdouble v3)
{
    return true;
}

bool ValidateProgramUniform4dv(Context *context,
                               ShaderProgramID program,
                               GLint location,
                               GLsizei count,
                               const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2dv(Context *context,
                                     ShaderProgramID program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2x3dv(Context *context,
                                       ShaderProgramID program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix2x4dv(Context *context,
                                       ShaderProgramID program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3dv(Context *context,
                                     ShaderProgramID program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3x2dv(Context *context,
                                       ShaderProgramID program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix3x4dv(Context *context,
                                       ShaderProgramID program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4dv(Context *context,
                                     ShaderProgramID program,
                                     GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4x2dv(Context *context,
                                       ShaderProgramID program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateProgramUniformMatrix4x3dv(Context *context,
                                       ShaderProgramID program,
                                       GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLdouble *value)
{
    return true;
}

bool ValidateScissorArrayv(Context *context, GLuint first, GLsizei count, const GLint *v)
{
    return true;
}

bool ValidateScissorIndexed(Context *context,
                            GLuint index,
                            GLint left,
                            GLint bottom,
                            GLsizei width,
                            GLsizei height)
{
    return true;
}

bool ValidateScissorIndexedv(Context *context, GLuint index, const GLint *v)
{
    return true;
}

bool ValidateVertexAttribL1d(Context *context, GLuint index, GLdouble x)
{
    return true;
}

bool ValidateVertexAttribL1dv(Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL2d(Context *context, GLuint index, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateVertexAttribL2dv(Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL3d(Context *context, GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateVertexAttribL3dv(Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribL4d(Context *context,
                             GLuint index,
                             GLdouble x,
                             GLdouble y,
                             GLdouble z,
                             GLdouble w)
{
    return true;
}

bool ValidateVertexAttribL4dv(Context *context, GLuint index, const GLdouble *v)
{
    return true;
}

bool ValidateVertexAttribLPointer(Context *context,
                                  GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLsizei stride,
                                  const void *pointer)
{
    return true;
}

bool ValidateViewportArrayv(Context *context, GLuint first, GLsizei count, const GLfloat *v)
{
    return true;
}

bool ValidateViewportIndexedf(Context *context,
                              GLuint index,
                              GLfloat x,
                              GLfloat y,
                              GLfloat w,
                              GLfloat h)
{
    return true;
}

bool ValidateViewportIndexedfv(Context *context, GLuint index, const GLfloat *v)
{
    return true;
}

}  // namespace gl
