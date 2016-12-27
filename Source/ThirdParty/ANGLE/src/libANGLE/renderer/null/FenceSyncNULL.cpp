//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FenceSyncNULL.cpp:
//    Implements the class methods for FenceSyncNULL.
//

#include "libANGLE/renderer/null/FenceSyncNULL.h"

#include "common/debug.h"

namespace rx
{

FenceSyncNULL::FenceSyncNULL() : FenceSyncImpl()
{
}

FenceSyncNULL::~FenceSyncNULL()
{
}

gl::Error FenceSyncNULL::set(GLenum condition, GLbitfield flags)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceSyncNULL::clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceSyncNULL::serverWait(GLbitfield flags, GLuint64 timeout)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceSyncNULL::getStatus(GLint *outResult)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
