//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QueryNULL.cpp:
//    Implements the class methods for QueryNULL.
//

#include "libANGLE/renderer/null/QueryNULL.h"

#include "common/debug.h"

namespace rx
{

QueryNULL::QueryNULL(GLenum type) : QueryImpl(type)
{
}

QueryNULL::~QueryNULL()
{
}

gl::Error QueryNULL::begin()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::end()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::queryCounter()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::getResult(GLint *params)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::getResult(GLuint *params)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::getResult(GLint64 *params)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::getResult(GLuint64 *params)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error QueryNULL::isResultAvailable(bool *available)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
