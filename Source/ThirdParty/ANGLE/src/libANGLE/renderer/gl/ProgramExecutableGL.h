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

class ProgramExecutableGL : public ProgramExecutableImpl
{
  public:
    ProgramExecutableGL(const gl::ProgramExecutable *executable);
    ~ProgramExecutableGL() override;

    void destroy(const gl::Context *context) override;

  private:
    friend class ProgramGL;

    void reset();
    void postLink(const FunctionsGL *functions,
                  const angle::FeaturesGL &features,
                  GLuint programID);

    std::vector<GLint> mUniformRealLocationMap;
    std::vector<GLuint> mUniformBlockRealLocationMap;

    bool mHasAppliedTransformFeedbackVaryings;

    GLint mClipDistanceEnabledUniformLocation;

    GLint mMultiviewBaseViewLayerIndexUniformLocation;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_PROGRAMEXECUTABLEGL_H_
