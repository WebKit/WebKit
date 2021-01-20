//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL4.cpp: Validation functions for OpenGL 4.0 entry point parameters

#include "libANGLE/validationGL4_autogen.h"

namespace gl
{

bool ValidateBeginQueryIndexed(const Context *context, GLenum target, GLuint index, QueryID id)
{
    return true;
}

bool ValidateDrawTransformFeedback(const Context *context, GLenum mode, TransformFeedbackID id)
{
    return true;
}

bool ValidateDrawTransformFeedbackStream(const Context *context,
                                         GLenum mode,
                                         TransformFeedbackID id,
                                         GLuint stream)
{
    return true;
}

bool ValidateEndQueryIndexed(const Context *context, GLenum target, GLuint index)
{
    return true;
}

bool ValidateGetActiveSubroutineName(const Context *context,
                                     ShaderProgramID program,
                                     GLenum shadertype,
                                     GLuint index,
                                     GLsizei bufsize,
                                     const GLsizei *length,
                                     const GLchar *name)
{
    return true;
}

bool ValidateGetActiveSubroutineUniformName(const Context *context,
                                            ShaderProgramID program,
                                            GLenum shadertype,
                                            GLuint index,
                                            GLsizei bufsize,
                                            const GLsizei *length,
                                            const GLchar *name)
{
    return true;
}

bool ValidateGetActiveSubroutineUniformiv(const Context *context,
                                          ShaderProgramID program,
                                          GLenum shadertype,
                                          GLuint index,
                                          GLenum pname,
                                          const GLint *values)
{
    return true;
}

bool ValidateGetProgramStageiv(const Context *context,
                               ShaderProgramID program,
                               GLenum shadertype,
                               GLenum pname,
                               const GLint *values)
{
    return true;
}

bool ValidateGetQueryIndexediv(const Context *context,
                               GLenum target,
                               GLuint index,
                               GLenum pname,
                               const GLint *params)
{
    return true;
}

bool ValidateGetSubroutineIndex(const Context *context,
                                ShaderProgramID program,
                                GLenum shadertype,
                                const GLchar *name)
{
    return true;
}

bool ValidateGetSubroutineUniformLocation(const Context *context,
                                          ShaderProgramID program,
                                          GLenum shadertype,
                                          const GLchar *name)
{
    return true;
}

bool ValidateGetUniformSubroutineuiv(const Context *context,
                                     GLenum shadertype,
                                     GLint location,
                                     const GLuint *params)
{
    return true;
}

bool ValidateGetUniformdv(const Context *context,
                          ShaderProgramID program,
                          UniformLocation location,
                          const GLdouble *params)
{
    return true;
}

bool ValidatePatchParameterfv(const Context *context, GLenum pname, const GLfloat *values)
{
    return true;
}

bool ValidateUniform1d(const Context *context, UniformLocation location, GLdouble x)
{
    return true;
}

bool ValidateUniform1dv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniform2d(const Context *context, UniformLocation location, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateUniform2dv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniform3d(const Context *context,
                       UniformLocation location,
                       GLdouble x,
                       GLdouble y,
                       GLdouble z)
{
    return true;
}

bool ValidateUniform3dv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniform4d(const Context *context,
                       UniformLocation location,
                       GLdouble x,
                       GLdouble y,
                       GLdouble z,
                       GLdouble w)
{
    return true;
}

bool ValidateUniform4dv(const Context *context,
                        UniformLocation location,
                        GLsizei count,
                        const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2dv(const Context *context,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2x3dv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix2x4dv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3dv(const Context *context,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3x2dv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix3x4dv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4dv(const Context *context,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4x2dv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformMatrix4x3dv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLdouble *value)
{
    return true;
}

bool ValidateUniformSubroutinesuiv(const Context *context,
                                   GLenum shadertype,
                                   GLsizei count,
                                   const GLuint *indices)
{
    return true;
}

}  // namespace gl
