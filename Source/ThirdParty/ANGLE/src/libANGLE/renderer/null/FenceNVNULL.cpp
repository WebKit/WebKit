//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FenceNVNULL.cpp:
//    Implements the class methods for FenceNVNULL.
//

#include "libANGLE/renderer/null/FenceNVNULL.h"

#include "common/debug.h"

namespace rx
{

FenceNVNULL::FenceNVNULL() : FenceNVImpl()
{
}

FenceNVNULL::~FenceNVNULL()
{
}

gl::Error FenceNVNULL::set(GLenum condition)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceNVNULL::test(GLboolean *outFinished)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceNVNULL::finish()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
