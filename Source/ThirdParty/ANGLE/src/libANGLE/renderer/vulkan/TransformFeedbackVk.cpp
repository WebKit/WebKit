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
{}

TransformFeedbackVk::~TransformFeedbackVk() {}

angle::Result TransformFeedbackVk::begin(const gl::Context *context,
                                         gl::PrimitiveMode primitiveMode)
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result TransformFeedbackVk::end(const gl::Context *context)
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result TransformFeedbackVk::pause(const gl::Context *context)
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result TransformFeedbackVk::resume(const gl::Context *context)
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result TransformFeedbackVk::bindGenericBuffer(const gl::Context *context,
                                                     const gl::BindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result TransformFeedbackVk::bindIndexedBuffer(
    const gl::Context *context,
    size_t index,
    const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

}  // namespace rx
