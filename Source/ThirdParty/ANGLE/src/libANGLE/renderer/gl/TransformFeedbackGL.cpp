//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TransformFeedbackGL.cpp: Implements the class methods for TransformFeedbackGL.

#include "libANGLE/renderer/gl/TransformFeedbackGL.h"

#include "common/debug.h"
#include "libANGLE/Data.h"
#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace rx
{

TransformFeedbackGL::TransformFeedbackGL(const FunctionsGL *functions,
                                         StateManagerGL *stateManager,
                                         size_t maxTransformFeedbackBufferBindings)
    : TransformFeedbackImpl(),
      mFunctions(functions),
      mStateManager(stateManager),
      mTransformFeedbackID(0),
      mIsActive(false),
      mIsPaused(false),
      mCurrentIndexedBuffers(maxTransformFeedbackBufferBindings)
{
    mFunctions->genTransformFeedbacks(1, &mTransformFeedbackID);
}

TransformFeedbackGL::~TransformFeedbackGL()
{
    mStateManager->deleteTransformFeedback(mTransformFeedbackID);
    mTransformFeedbackID = 0;

    for (auto &bufferBinding : mCurrentIndexedBuffers)
    {
        bufferBinding.set(nullptr);
    }
}

void TransformFeedbackGL::begin(GLenum primitiveMode)
{
    // Do not begin directly, StateManagerGL will handle beginning and resuming transform feedback.
}

void TransformFeedbackGL::end()
{
    syncActiveState(false, GL_NONE);
}

void TransformFeedbackGL::pause()
{
    syncPausedState(true);
}

void TransformFeedbackGL::resume()
{
    // Do not resume directly, StateManagerGL will handle beginning and resuming transform feedback.
}

void TransformFeedbackGL::bindGenericBuffer(const BindingPointer<gl::Buffer> &binding)
{
}

void TransformFeedbackGL::bindIndexedBuffer(size_t index, const OffsetBindingPointer<gl::Buffer> &binding)
{
    // Directly bind buffer (not through the StateManager methods) because the buffer bindings are
    // tracked per transform feedback object
    if (binding != mCurrentIndexedBuffers[index])
    {
        mStateManager->bindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedbackID);
        if (binding.get() != nullptr)
        {
            const BufferGL *bufferGL = GetImplAs<BufferGL>(binding.get());
            if (binding.getSize() != 0)
            {
                mFunctions->bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,
                                            static_cast<GLuint>(index), bufferGL->getBufferID(),
                                            binding.getOffset(), binding.getSize());
            }
            else
            {
                mFunctions->bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(index),
                                           bufferGL->getBufferID());
            }
        }
        else
        {
            mFunctions->bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(index), 0);
        }

        mCurrentIndexedBuffers[index] = binding;
    }
}

GLuint TransformFeedbackGL::getTransformFeedbackID() const
{
    return mTransformFeedbackID;
}

void TransformFeedbackGL::syncActiveState(bool active, GLenum primitiveMode) const
{
    if (mIsActive != active)
    {
        mIsActive = active;
        mIsPaused = false;

        mStateManager->bindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedbackID);
        if (mIsActive)
        {
            mFunctions->beginTransformFeedback(primitiveMode);
        }
        else
        {
            mFunctions->endTransformFeedback();
        }
    }
}

void TransformFeedbackGL::syncPausedState(bool paused) const
{
    if (mIsActive && mIsPaused != paused)
    {
        mIsPaused = paused;

        mStateManager->bindTransformFeedback(GL_TRANSFORM_FEEDBACK, mTransformFeedbackID);
        if (mIsPaused)
        {
            mFunctions->pauseTransformFeedback();
        }
        else
        {
            mFunctions->resumeTransformFeedback();
        }
    }
}

}
