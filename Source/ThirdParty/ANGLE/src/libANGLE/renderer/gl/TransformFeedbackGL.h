//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TransformFeedbackGL.h: Defines the class interface for TransformFeedbackGL.

#ifndef LIBANGLE_RENDERER_GL_TRANSFORMFEEDBACKGL_H_
#define LIBANGLE_RENDERER_GL_TRANSFORMFEEDBACKGL_H_

#include "libANGLE/renderer/TransformFeedbackImpl.h"

namespace rx
{

class FunctionsGL;
class StateManagerGL;

class TransformFeedbackGL : public TransformFeedbackImpl
{
  public:
    TransformFeedbackGL(const FunctionsGL *functions,
                        StateManagerGL *stateManager,
                        size_t maxTransformFeedbackBufferBindings);
    ~TransformFeedbackGL() override;

    void begin(GLenum primitiveMode) override;
    void end() override;
    void pause() override;
    void resume() override;

    void bindGenericBuffer(const BindingPointer<gl::Buffer> &binding) override;
    void bindIndexedBuffer(size_t index, const OffsetBindingPointer<gl::Buffer> &binding) override;

    GLuint getTransformFeedbackID() const;

    void syncActiveState(bool active, GLenum primitiveMode) const;
    void syncPausedState(bool paused) const;

  private:
    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    GLuint mTransformFeedbackID;

    mutable bool mIsActive;
    mutable bool mIsPaused;

    std::vector<OffsetBindingPointer<gl::Buffer>> mCurrentIndexedBuffers;
};

}

#endif // LIBANGLE_RENDERER_GL_TRANSFORMFEEDBACKGL_H_
