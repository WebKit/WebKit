//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramNULL.cpp:
//    Implements the class methods for ProgramNULL.
//

#include "libANGLE/renderer/null/ProgramNULL.h"

#include "common/debug.h"

namespace rx
{

ProgramNULL::ProgramNULL(const gl::ProgramState &state) : ProgramImpl(state)
{
}

ProgramNULL::~ProgramNULL()
{
}

LinkResult ProgramNULL::load(const ContextImpl *contextImpl,
                             gl::InfoLog &infoLog,
                             gl::BinaryInputStream *stream)
{
    return true;
}

gl::Error ProgramNULL::save(gl::BinaryOutputStream *stream)
{
    return gl::NoError();
}

void ProgramNULL::setBinaryRetrievableHint(bool retrievable)
{
}

void ProgramNULL::setSeparable(bool separable)
{
}

LinkResult ProgramNULL::link(ContextImpl *contextImpl,
                             const gl::VaryingPacking &packing,
                             gl::InfoLog &infoLog)
{
    return true;
}

GLboolean ProgramNULL::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    return GL_TRUE;
}

void ProgramNULL::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
}

void ProgramNULL::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
}

void ProgramNULL::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
}

void ProgramNULL::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
}

void ProgramNULL::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
}

void ProgramNULL::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
}

void ProgramNULL::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
}

void ProgramNULL::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
}

void ProgramNULL::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
}

void ProgramNULL::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
}

void ProgramNULL::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
}

void ProgramNULL::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
}

void ProgramNULL::setUniformMatrix2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix2x3fv(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix3x2fv(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix2x4fv(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix4x2fv(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix3x4fv(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
}

void ProgramNULL::setUniformMatrix4x3fv(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
}

void ProgramNULL::setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
}

bool ProgramNULL::getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const
{
    // TODO(geofflang): Compute reasonable sizes?
    *sizeOut = 0;
    return true;
}

bool ProgramNULL::getUniformBlockMemberInfo(const std::string &memberUniformName,
                                            sh::BlockMemberInfo *memberInfoOut) const
{
    // TODO(geofflang): Compute reasonable values?
    return true;
}

void ProgramNULL::setPathFragmentInputGen(const std::string &inputName,
                                          GLenum genMode,
                                          GLint components,
                                          const GLfloat *coeffs)
{
}

}  // namespace rx
