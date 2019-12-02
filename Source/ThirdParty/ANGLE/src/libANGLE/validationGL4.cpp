//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL4.cpp: Validation functions for OpenGL 4.0 entry point parameters

#include "libANGLE/validationGL4_autogen.h"

namespace gl
{

bool ValidateBeginQueryIndexed(Context *context, GLenum target, GLuint index, QueryID id)
{
    return true;
}

bool ValidateDrawTransformFeedback(Context *context, GLenum mode, TransformFeedbackID id)
{
    return true;
}

bool ValidateDrawTransformFeedbackStream(Context *context,
                                         GLenum mode,
                                         TransformFeedbackID id,
                                         GLuint stream)
{
    return true;
}

bool ValidateEndQueryIndexed(Context *context, GLenum target, GLuint index)
{
    return true;
}

bool ValidateGetActiveSubroutineName(Context *context,
                                     ShaderProgramID program,
                                     GLenum shadertype,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei *length,
                                     GLchar *name)
{
    return true;
}

bool ValidateGetActiveSubroutineUniformName(Context *context,
                                            ShaderProgramID program,
                                            GLenum shadertype,
                                            GLuint index,
                                            GLsizei bufsize,
                                            GLsizei *length,
                                            GLchar *name)
{
    return true;
}

bool ValidateGetActiveSubroutineUniformiv(Context *context,
                                          ShaderProgramID program,
                                          GLenum shadertype,
                                          GLuint index,
                                          GLenum pname,
                                          GLint *values)
{
    return true;
}

bool ValidateGetProgramStageiv(Context *context,
                               ShaderProgramID program,
                               GLenum shadertype,
                               GLenum pname,
                               GLint *values)
{
    return true;
}

bool ValidateGetQueryIndexediv(Context *context,
                               GLenum target,
                               GLuint index,
                               GLenum pname,
                               GLint *params)
{
    return true;
}

bool ValidateGetSubroutineIndex(Context *context,
                                ShaderProgramID program,
                                GLenum shadertype,
                                const GLchar *name)
{
    return true;
}

bool ValidateGetSubroutineUniformLocation(Context *context,
                                          ShaderProgramID program,
                                          GLenum shadertype,
                                          const GLchar *name)
{
    return true;
}

bool ValidateGetUniformSubroutineuiv(Context *context,
                                     GLenum shadertype,
                                     GLint location,
                                     GLuint *params)
{
    return true;
}

bool ValidateGetUniformdv(Context *context,
                          ShaderProgramID program,
                          GLint location,
                          GLdouble *params)
{
    return true;
}

bool ValidatePatchParameterfv(Context *context, GLenum pname, const GLfloat *values)
{
    return true;
}

bool ValidateUniform1d(Context *context, GLint location, GLdouble x)
{
    return true;
}

bool ValidateUniform1dv(Context *context, GLint location, GLsizei count, const GLdouble *value)
{
    return true;
}

bool ValidateUniform2d(Context *context, GLint location, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateUniform2dv(Context *context, GLint location, GLsizei count, const GLdouble *value)
{
    return true;
}

bool ValidateUniform3d(Context *context, GLint location, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateUniform3dv(Context *context, GLint location, GLsizei count, const GLdouble *value)
{
    return true;
}

bool ValidateUniform4d(Context *context,
                       GLint location,
                       GLdouble x,
                       GLdouble y,
                       GLdouble z,
                       GLdouble w)
{
    return true;
}

bool ValidateUniform4dv(Context *context, GLint location, GLsizei count, const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2dv(Context *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2x3dv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2x4dv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3dv(Context *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3x2dv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3x4dv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4dv(Context *context,
                              GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4x2dv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4x3dv(Context *context,
                                GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformSubroutinesuiv(Context *context,
                                   GLenum shadertype,
                                   GLsizei count,
                                   const GLuint *indices)
{
    return true;
}

}  // namespace gl
