//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TransformFeedbackD3D.cpp is a no-op implementation for both the D3D9 and D3D11 renderers.

#include "libANGLE/renderer/d3d/d3d11/TransformFeedback11.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"

namespace rx
{

TransformFeedback11::TransformFeedback11(const gl::TransformFeedbackState &state,
                                         Renderer11 *renderer)
    : TransformFeedbackImpl(state),
      mRenderer(renderer),
      mIsDirty(true),
      mBuffers(state.getIndexedBuffers().size(), nullptr),
      mBufferOffsets(state.getIndexedBuffers().size(), 0),
      mSerial(mRenderer->generateSerial())
{
}

TransformFeedback11::~TransformFeedback11()
{
}

void TransformFeedback11::begin(GLenum primitiveMode)
{
    // Reset all the cached offsets to the binding offsets
    mIsDirty = true;
    for (size_t bindingIdx = 0; bindingIdx < mBuffers.size(); bindingIdx++)
    {
        const auto &binding = mState.getIndexedBuffer(bindingIdx);
        if (binding.get() != nullptr)
        {
            mBufferOffsets[bindingIdx] = static_cast<UINT>(binding.getOffset());
        }
        else
        {
            mBufferOffsets[bindingIdx] = 0;
        }
    }
}

void TransformFeedback11::end()
{
    if (mRenderer->getWorkarounds().flushAfterEndingTransformFeedback)
    {
        mRenderer->getDeviceContext()->Flush();
    }
}

void TransformFeedback11::pause()
{
}

void TransformFeedback11::resume()
{
}

void TransformFeedback11::bindGenericBuffer(const gl::BindingPointer<gl::Buffer> &binding)
{
}

void TransformFeedback11::bindIndexedBuffer(size_t index,
                                            const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    mIsDirty              = true;
    mBufferOffsets[index] = static_cast<UINT>(binding.getOffset());
}

void TransformFeedback11::onApply()
{
    mIsDirty = false;

    // Change all buffer offsets to -1 so that if any of them need to be re-applied, the are set to
    // append
    std::fill(mBufferOffsets.begin(), mBufferOffsets.end(), -1);
}

bool TransformFeedback11::isDirty() const
{
    return mIsDirty;
}

UINT TransformFeedback11::getNumSOBuffers() const
{
    return static_cast<UINT>(mBuffers.size());
}

gl::ErrorOrResult<const std::vector<ID3D11Buffer *> *> TransformFeedback11::getSOBuffers(
    const gl::Context *context)
{
    for (size_t bindingIdx = 0; bindingIdx < mBuffers.size(); bindingIdx++)
    {
        const auto &binding = mState.getIndexedBuffer(bindingIdx);
        if (binding.get() != nullptr)
        {
            Buffer11 *storage = GetImplAs<Buffer11>(binding.get());
            ANGLE_TRY_RESULT(storage->getBuffer(context, BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK),
                             mBuffers[bindingIdx]);
        }
    }

    return &mBuffers;
}

const std::vector<UINT> &TransformFeedback11::getSOBufferOffsets() const
{
    return mBufferOffsets;
}

Serial TransformFeedback11::getSerial() const
{
    return mSerial;
}

}  // namespace rx
