//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/TransformFeedback.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Program.h"
#include "libANGLE/renderer/ImplFactory.h"
#include "libANGLE/renderer/TransformFeedbackImpl.h"

namespace gl
{

TransformFeedback::TransformFeedback(rx::ImplFactory *implFactory, GLuint id, const Caps &caps)
    : RefCountObject(id),
      mImplementation(implFactory->createTransformFeedback()),
      mLabel(),
      mActive(false),
      mPrimitiveMode(GL_NONE),
      mPaused(false),
      mProgram(nullptr),
      mGenericBuffer(),
      mIndexedBuffers(caps.maxTransformFeedbackSeparateAttributes)
{
    ASSERT(mImplementation != nullptr);
}

TransformFeedback::~TransformFeedback()
{
    if (mProgram)
    {
        mProgram->release();
        mProgram = nullptr;
    }
    mGenericBuffer.set(nullptr);
    for (size_t i = 0; i < mIndexedBuffers.size(); i++)
    {
        mIndexedBuffers[i].set(nullptr);
    }

    SafeDelete(mImplementation);
}

void TransformFeedback::setLabel(const std::string &label)
{
    mLabel = label;
}

const std::string &TransformFeedback::getLabel() const
{
    return mLabel;
}

void TransformFeedback::begin(GLenum primitiveMode, Program *program)
{
    mActive = true;
    mPrimitiveMode = primitiveMode;
    mPaused = false;
    mImplementation->begin(primitiveMode);
    bindProgram(program);
}

void TransformFeedback::end()
{
    mActive = false;
    mPrimitiveMode = GL_NONE;
    mPaused = false;
    mImplementation->end();
    if (mProgram)
    {
        mProgram->release();
        mProgram = nullptr;
    }
}

void TransformFeedback::pause()
{
    mPaused = true;
    mImplementation->pause();
}

void TransformFeedback::resume()
{
    mPaused = false;
    mImplementation->resume();
}

bool TransformFeedback::isActive() const
{
    return mActive;
}

bool TransformFeedback::isPaused() const
{
    return mPaused;
}

GLenum TransformFeedback::getPrimitiveMode() const
{
    return mPrimitiveMode;
}

void TransformFeedback::bindProgram(Program *program)
{
    if (mProgram != program)
    {
        if (mProgram != nullptr)
        {
            mProgram->release();
        }
        mProgram = program;
        if (mProgram != nullptr)
        {
            mProgram->addRef();
        }
    }
}

bool TransformFeedback::hasBoundProgram(GLuint program) const
{
    return mProgram != nullptr && mProgram->id() == program;
}

void TransformFeedback::bindGenericBuffer(Buffer *buffer)
{
    mGenericBuffer.set(buffer);
    mImplementation->bindGenericBuffer(mGenericBuffer);
}

void TransformFeedback::detachBuffer(GLuint bufferName)
{
    for (size_t index = 0; index < mIndexedBuffers.size(); index++)
    {
        if (mIndexedBuffers[index].id() == bufferName)
        {
            mIndexedBuffers[index].set(nullptr);
            mImplementation->bindIndexedBuffer(index, mIndexedBuffers[index]);
        }
    }

    if (mGenericBuffer.id() == bufferName)
    {
        mGenericBuffer.set(nullptr);
        mImplementation->bindGenericBuffer(mGenericBuffer);
    }
}

const BindingPointer<Buffer> &TransformFeedback::getGenericBuffer() const
{
    return mGenericBuffer;
}

void TransformFeedback::bindIndexedBuffer(size_t index, Buffer *buffer, size_t offset, size_t size)
{
    ASSERT(index < mIndexedBuffers.size());
    mIndexedBuffers[index].set(buffer, offset, size);
    mImplementation->bindIndexedBuffer(index, mIndexedBuffers[index]);
}

const OffsetBindingPointer<Buffer> &TransformFeedback::getIndexedBuffer(size_t index) const
{
    ASSERT(index < mIndexedBuffers.size());
    return mIndexedBuffers[index];
}

size_t TransformFeedback::getIndexedBufferCount() const
{
    return mIndexedBuffers.size();
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
