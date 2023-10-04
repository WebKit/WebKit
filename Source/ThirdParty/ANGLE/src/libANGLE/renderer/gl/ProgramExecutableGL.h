//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableGL.h: Implementation of ProgramExecutableImpl.

#ifndef LIBANGLE_RENDERER_GL_PROGRAMEXECUTABLEGL_H_
#define LIBANGLE_RENDERER_GL_PROGRAMEXECUTABLEGL_H_

#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/renderer/ProgramExecutableImpl.h"

namespace angle
{
struct FeaturesGL;
}  // namespace angle

namespace rx
{
class FunctionsGL;
class StateManagerGL;

class ProgramExecutableGL : public ProgramExecutableImpl
{
  public:
    ProgramExecutableGL(const gl::ProgramExecutable *executable);
    ~ProgramExecutableGL() override;

    void destroy(const gl::Context *context) override;

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
    void setUniformMatrix2fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix3fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix4fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix2x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix2x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;

    void getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const override;
    void getUniformiv(const gl::Context *context, GLint location, GLint *params) const override;
    void getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const override;

    void updateEnabledClipDistances(uint8_t enabledClipDistancesPacked) const;

    void enableLayeredRenderingPath(int baseViewIndex) const;

    GLuint getProgramID() const { return mProgramID; }

  private:
    friend class ProgramGL;

    void reset();
    void postLink(const FunctionsGL *functions,
                  StateManagerGL *stateManager,
                  const angle::FeaturesGL &features,
                  GLuint programID);

    // Helper function, makes it simpler to type.
    GLint uniLoc(GLint glLocation) const { return mUniformRealLocationMap[glLocation]; }

    std::vector<GLint> mUniformRealLocationMap;
    std::vector<GLuint> mUniformBlockRealLocationMap;

    bool mHasAppliedTransformFeedbackVaryings;

    GLint mClipDistanceEnabledUniformLocation;

    GLint mMultiviewBaseViewLayerIndexUniformLocation;

    // The program for which the executable was built
    GLuint mProgramID;
    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_PROGRAMEXECUTABLEGL_H_
