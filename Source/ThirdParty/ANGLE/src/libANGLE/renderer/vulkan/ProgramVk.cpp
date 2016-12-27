//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramVk.cpp:
//    Implements the class methods for ProgramVk.
//

#include "libANGLE/renderer/vulkan/ProgramVk.h"

#include "common/debug.h"

namespace rx
{

ProgramVk::ProgramVk(const gl::ProgramState &state) : ProgramImpl(state)
{
}

ProgramVk::~ProgramVk()
{
}

LinkResult ProgramVk::load(gl::InfoLog &infoLog, gl::BinaryInputStream *stream)
{
    UNIMPLEMENTED();
    return LinkResult(false, gl::Error(GL_INVALID_OPERATION));
}

gl::Error ProgramVk::save(gl::BinaryOutputStream *stream)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

void ProgramVk::setBinaryRetrievableHint(bool retrievable)
{
    UNIMPLEMENTED();
}

LinkResult ProgramVk::link(const gl::ContextState &data, gl::InfoLog &infoLog)
{
    UNIMPLEMENTED();
    return LinkResult(false, gl::Error(GL_INVALID_OPERATION));
}

GLboolean ProgramVk::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    UNIMPLEMENTED();
    return GLboolean();
}

void ProgramVk::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix2x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix3x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix2x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix4x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix3x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix4x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    UNIMPLEMENTED();
}

bool ProgramVk::getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const
{
    UNIMPLEMENTED();
    return bool();
}

bool ProgramVk::getUniformBlockMemberInfo(const std::string &memberUniformName,
                                          sh::BlockMemberInfo *memberInfoOut) const
{
    UNIMPLEMENTED();
    return bool();
}

void ProgramVk::setPathFragmentInputGen(const std::string &inputName,
                                        GLenum genMode,
                                        GLint components,
                                        const GLfloat *coeffs)
{
    UNIMPLEMENTED();
}

}  // namespace rx
