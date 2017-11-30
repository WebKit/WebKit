//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackVk.cpp:
//    Implements the class methods for TransformFeedbackVk.
//

#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"

#include "common/debug.h"

namespace rx
{

TransformFeedbackVk::TransformFeedbackVk(const gl::TransformFeedbackState &state)
    : TransformFeedbackImpl(state)
{
}

TransformFeedbackVk::~TransformFeedbackVk()
{
}

void TransformFeedbackVk::begin(GLenum primitiveMode)
{
    UNIMPLEMENTED();
}

void TransformFeedbackVk::end()
{
    UNIMPLEMENTED();
}

void TransformFeedbackVk::pause()
{
    UNIMPLEMENTED();
}

void TransformFeedbackVk::resume()
{
    UNIMPLEMENTED();
}

void TransformFeedbackVk::bindGenericBuffer(const gl::BindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();
}

void TransformFeedbackVk::bindIndexedBuffer(size_t index,
                                            const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();
}

}  // namespace rx
