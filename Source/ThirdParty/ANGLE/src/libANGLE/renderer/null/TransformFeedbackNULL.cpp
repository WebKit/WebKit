//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackNULL.cpp:
//    Implements the class methods for TransformFeedbackNULL.
//

#include "libANGLE/renderer/null/TransformFeedbackNULL.h"

#include "common/debug.h"

namespace rx
{

TransformFeedbackNULL::TransformFeedbackNULL(const gl::TransformFeedbackState &state)
    : TransformFeedbackImpl(state)
{
}

TransformFeedbackNULL::~TransformFeedbackNULL()
{
}

void TransformFeedbackNULL::begin(GLenum primitiveMode)
{
}

void TransformFeedbackNULL::end()
{
}

void TransformFeedbackNULL::pause()
{
}

void TransformFeedbackNULL::resume()
{
}

void TransformFeedbackNULL::bindGenericBuffer(const gl::BindingPointer<gl::Buffer> &binding)
{
}

void TransformFeedbackNULL::bindIndexedBuffer(size_t index,
                                              const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
}

}  // namespace rx
