//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramGL.h: Defines the class interface for ProgramGL.

#ifndef LIBANGLE_RENDERER_GL_PROGRAMGL_H_
#define LIBANGLE_RENDERER_GL_PROGRAMGL_H_

#include <string>
#include <vector>

#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/renderer/ProgramImpl.h"

namespace rx
{

class FunctionsGL;
class StateManagerGL;

struct SamplerBindingGL
{
    GLenum textureType;
    std::vector<GLuint> boundTextureUnits;
};

class ProgramGL : public ProgramImpl
{
  public:
    ProgramGL(const gl::ProgramState &data,
              const FunctionsGL *functions,
              const WorkaroundsGL &workarounds,
              StateManagerGL *stateManager,
              bool enablePathRendering);
    ~ProgramGL() override;

    LinkResult load(gl::InfoLog &infoLog, gl::BinaryInputStream *stream) override;
    gl::Error save(gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;

    LinkResult link(const gl::ContextState &data, gl::InfoLog &infoLog) override;
    GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) override;

    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform1iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform2iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform3iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform4iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;
    void setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) override;

    void setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding) override;

    bool getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const override;
    bool getUniformBlockMemberInfo(const std::string &memberUniformName,
                                   sh::BlockMemberInfo *memberInfoOut) const override;

    void setPathFragmentInputGen(const std::string &inputName,
                                 GLenum genMode,
                                 GLint components,
                                 const GLfloat *coeffs) override;

    GLuint getProgramID() const;
    const std::vector<SamplerBindingGL> &getAppliedSamplerUniforms() const;

  private:
    void preLink();
    bool checkLinkStatus(gl::InfoLog &infoLog);
    void postLink();

    // Helper function, makes it simpler to type.
    GLint uniLoc(GLint glLocation) const { return mUniformRealLocationMap[glLocation]; }

    const FunctionsGL *mFunctions;
    const WorkaroundsGL &mWorkarounds;
    StateManagerGL *mStateManager;

    std::vector<GLint> mUniformRealLocationMap;
    std::vector<GLuint> mUniformBlockRealLocationMap;

    // An array of the samplers that are used by the program
    std::vector<SamplerBindingGL> mSamplerBindings;

    // A map from a mData.getUniforms() index to a mSamplerBindings index.
    std::vector<size_t> mUniformIndexToSamplerIndex;

    struct PathRenderingFragmentInput
    {
        std::string name;
        GLint location;
    };
    std::vector<PathRenderingFragmentInput> mPathRenderingFragmentInputs;

    bool mEnablePathRendering;

    GLuint mProgramID;
};

}

#endif // LIBANGLE_RENDERER_GL_PROGRAMGL_H_
