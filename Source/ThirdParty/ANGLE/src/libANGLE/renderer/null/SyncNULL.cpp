//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncNULL.cpp:
//    Implements the class methods for SyncNULL.
//

#include "libANGLE/renderer/null/SyncNULL.h"

#include "common/debug.h"

namespace rx
{

SyncNULL::SyncNULL() : SyncImpl()
{
}

SyncNULL::~SyncNULL()
{
}

gl::Error SyncNULL::set(GLenum condition, GLbitfield flags)
{
    return gl::NoError();
}

gl::Error SyncNULL::clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult)
{
    *outResult = GL_ALREADY_SIGNALED;
    return gl::NoError();
}

gl::Error SyncNULL::serverWait(GLbitfield flags, GLuint64 timeout)
{
    return gl::NoError();
}

gl::Error SyncNULL::getStatus(GLint *outResult)
{
    *outResult = GL_SIGNALED;
    return gl::NoError();
}

}  // namespace rx
