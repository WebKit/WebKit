//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramImpl.h: Defines the abstract rx::ProgramImpl class.

#ifndef LIBANGLE_RENDERER_PROGRAMIMPL_H_
#define LIBANGLE_RENDERER_PROGRAMIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/BinaryStream.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Program.h"
#include "libANGLE/Shader.h"

#include <map>

namespace gl
{
class VaryingPacking;
}

namespace sh
{
struct BlockMemberInfo;
}

namespace rx
{
class ContextImpl;

using LinkResult = gl::ErrorOrResult<bool>;

class ProgramImpl : angle::NonCopyable
{
  public:
    ProgramImpl(const gl::ProgramState &state) : mState(state) {}
    virtual ~ProgramImpl() {}
    virtual void destroy(const ContextImpl *contextImpl) {}

    virtual LinkResult load(const ContextImpl *contextImpl,
                            gl::InfoLog &infoLog,
                            gl::BinaryInputStream *stream)  = 0;
    virtual gl::Error save(gl::BinaryOutputStream *stream) = 0;
    virtual void setBinaryRetrievableHint(bool retrievable) = 0;
    virtual void setSeparable(bool separable)               = 0;

    virtual LinkResult link(ContextImpl *contextImpl,
                            const gl::VaryingPacking &packing,
                            gl::InfoLog &infoLog) = 0;
    virtual GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) = 0;

    virtual void setUniform1fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform2fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform3fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform4fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform1iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform2iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform3iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform4iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform1uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniform2uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniform3uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniform4uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;

    // TODO: synchronize in syncState when dirty bits exist.
    virtual void setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding) = 0;

    // May only be called after a successful link operation.
    // Return false for inactive blocks.
    virtual bool getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const = 0;

    // May only be called after a successful link operation.
    // Returns false for inactive members.
    virtual bool getUniformBlockMemberInfo(const std::string &memberUniformName,
                                           sh::BlockMemberInfo *memberInfoOut) const = 0;
    // CHROMIUM_path_rendering
    // Set parameters to control fragment shader input variable interpolation
    virtual void setPathFragmentInputGen(const std::string &inputName,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat *coeffs) = 0;

  protected:
    const gl::ProgramState &mState;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_PROGRAMIMPL_H_
