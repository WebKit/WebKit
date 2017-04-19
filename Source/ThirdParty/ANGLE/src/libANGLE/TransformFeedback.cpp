//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/TransformFeedback.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/Caps.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Program.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/TransformFeedbackImpl.h"

namespace gl
{

TransformFeedbackState::TransformFeedbackState(size_t maxIndexedBuffers)
    : mLabel(),
      mActive(false),
      mPrimitiveMode(GL_NONE),
      mPaused(false),
      mProgram(nullptr),
      mGenericBuffer(),
      mIndexedBuffers(maxIndexedBuffers)
{
}

const BindingPointer<Buffer> &TransformFeedbackState::getGenericBuffer() const
{
    return mGenericBuffer;
}

const OffsetBindingPointer<Buffer> &TransformFeedbackState::getIndexedBuffer(size_t idx) const
{
    return mIndexedBuffers[idx];
}

const std::vector<OffsetBindingPointer<Buffer>> &TransformFeedbackState::getIndexedBuffers() const
{
    return mIndexedBuffers;
}

TransformFeedback::TransformFeedback(rx::GLImplFactory *implFactory, GLuint id, const Caps &caps)
    : RefCountObject(id),
      mState(caps.maxTransformFeedbackSeparateAttributes),
      mImplementation(implFactory->createTransformFeedback(mState))
{
    ASSERT(mImplementation != nullptr);
}

void TransformFeedback::destroy(const Context *context)
{
    if (mState.mProgram)
    {
        mState.mProgram->release(context);
        mState.mProgram = nullptr;
    }
}

TransformFeedback::~TransformFeedback()
{
    ASSERT(!mState.mProgram);
    mState.mGenericBuffer.set(nullptr);
    for (size_t i = 0; i < mState.mIndexedBuffers.size(); i++)
    {
        mState.mIndexedBuffers[i].set(nullptr);
    }

    SafeDelete(mImplementation);
}

void TransformFeedback::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &TransformFeedback::getLabel() const
{
    return mState.mLabel;
}

void TransformFeedback::begin(const Context *context, GLenum primitiveMode, Program *program)
{
    mState.mActive        = true;
    mState.mPrimitiveMode = primitiveMode;
    mState.mPaused        = false;
    mImplementation->begin(primitiveMode);
    bindProgram(context, program);
}

void TransformFeedback::end(const Context *context)
{
    mState.mActive        = false;
    mState.mPrimitiveMode = GL_NONE;
    mState.mPaused        = false;
    mImplementation->end();
    if (mState.mProgram)
    {
        mState.mProgram->release(context);
        mState.mProgram = nullptr;
    }
}

void TransformFeedback::pause()
{
    mState.mPaused = true;
    mImplementation->pause();
}

void TransformFeedback::resume()
{
    mState.mPaused = false;
    mImplementation->resume();
}

bool TransformFeedback::isActive() const
{
    return mState.mActive;
}

bool TransformFeedback::isPaused() const
{
    return mState.mPaused;
}

GLenum TransformFeedback::getPrimitiveMode() const
{
    return mState.mPrimitiveMode;
}

void TransformFeedback::bindProgram(const Context *context, Program *program)
{
    if (mState.mProgram != program)
    {
        if (mState.mProgram != nullptr)
        {
            mState.mProgram->release(context);
        }
        mState.mProgram = program;
        if (mState.mProgram != nullptr)
        {
            mState.mProgram->addRef();
        }
    }
}

bool TransformFeedback::hasBoundProgram(GLuint program) const
{
    return mState.mProgram != nullptr && mState.mProgram->id() == program;
}

void TransformFeedback::bindGenericBuffer(Buffer *buffer)
{
    mState.mGenericBuffer.set(buffer);
    mImplementation->bindGenericBuffer(mState.mGenericBuffer);
}

void TransformFeedback::detachBuffer(GLuint bufferName)
{
    for (size_t index = 0; index < mState.mIndexedBuffers.size(); index++)
    {
        if (mState.mIndexedBuffers[index].id() == bufferName)
        {
            mState.mIndexedBuffers[index].set(nullptr);
            mImplementation->bindIndexedBuffer(index, mState.mIndexedBuffers[index]);
        }
    }

    if (mState.mGenericBuffer.id() == bufferName)
    {
        mState.mGenericBuffer.set(nullptr);
        mImplementation->bindGenericBuffer(mState.mGenericBuffer);
    }
}

const BindingPointer<Buffer> &TransformFeedback::getGenericBuffer() const
{
    return mState.mGenericBuffer;
}

void TransformFeedback::bindIndexedBuffer(size_t index, Buffer *buffer, size_t offset, size_t size)
{
    ASSERT(index < mState.mIndexedBuffers.size());
    mState.mIndexedBuffers[index].set(buffer, offset, size);
    mImplementation->bindIndexedBuffer(index, mState.mIndexedBuffers[index]);
}

const OffsetBindingPointer<Buffer> &TransformFeedback::getIndexedBuffer(size_t index) const
{
    ASSERT(index < mState.mIndexedBuffers.size());
    return mState.mIndexedBuffers[index];
}

size_t TransformFeedback::getIndexedBufferCount() const
{
    return mState.mIndexedBuffers.size();
}

rx::TransformFeedbackImpl *TransformFeedback::getImplementation()
{
    return mImplementation;
}

const rx::TransformFeedbackImpl *TransformFeedback::getImplementation() const
{
    return mImplementation;
}

}
