//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TransformFeedbackGL.cpp: Implements the class methods for TransformFeedbackGL.

#include "libANGLE/renderer/gl/TransformFeedbackGL.h"

#include "common/debug.h"

namespace rx
{

TransformFeedbackGL::TransformFeedbackGL()
    : TransformFeedbackImpl()
{}

TransformFeedbackGL::~TransformFeedbackGL()
{}

void TransformFeedbackGL::begin(GLenum primitiveMode)
{
    UNIMPLEMENTED();
}

void TransformFeedbackGL::end()
{
    UNIMPLEMENTED();
}

void TransformFeedbackGL::pause()
{
    UNIMPLEMENTED();
}

void TransformFeedbackGL::resume()
{
    UNIMPLEMENTED();
}

void TransformFeedbackGL::bindGenericBuffer(const BindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();
}

void TransformFeedbackGL::bindIndexedBuffer(size_t index, const OffsetBindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();
}

}
