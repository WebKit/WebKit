//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FenceSyncVk.cpp:
//    Implements the class methods for FenceSyncVk.
//

#include "libANGLE/renderer/vulkan/FenceSyncVk.h"

#include "common/debug.h"

namespace rx
{

FenceSyncVk::FenceSyncVk() : FenceSyncImpl()
{
}

FenceSyncVk::~FenceSyncVk()
{
}

gl::Error FenceSyncVk::set(GLenum condition, GLbitfield flags)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceSyncVk::clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceSyncVk::serverWait(GLbitfield flags, GLuint64 timeout)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FenceSyncVk::getStatus(GLint *outResult)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
