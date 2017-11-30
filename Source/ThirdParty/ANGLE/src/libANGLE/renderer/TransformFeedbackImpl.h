//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TransformFeedbackImpl.h: Defines the abstract rx::TransformFeedbackImpl class.

#ifndef LIBANGLE_RENDERER_TRANSFORMFEEDBACKIMPL_H_
#define LIBANGLE_RENDERER_TRANSFORMFEEDBACKIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/TransformFeedback.h"

namespace rx
{

class TransformFeedbackImpl : angle::NonCopyable
{
  public:
    TransformFeedbackImpl(const gl::TransformFeedbackState &state) : mState(state) {}
    virtual ~TransformFeedbackImpl() { }

    virtual void begin(GLenum primitiveMode) = 0;
    virtual void end() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;

    virtual void bindGenericBuffer(const gl::BindingPointer<gl::Buffer> &binding) = 0;
    virtual void bindIndexedBuffer(size_t index,
                                   const gl::OffsetBindingPointer<gl::Buffer> &binding) = 0;

  protected:
    const gl::TransformFeedbackState &mState;
};

}

#endif // LIBANGLE_RENDERER_TRANSFORMFEEDBACKIMPL_H_
