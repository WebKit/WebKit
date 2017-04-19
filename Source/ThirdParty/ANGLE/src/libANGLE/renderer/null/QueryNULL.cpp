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
    return gl::NoError();
}

gl::Error QueryNULL::end()
{
    return gl::NoError();
}

gl::Error QueryNULL::queryCounter()
{
    return gl::NoError();
}

gl::Error QueryNULL::getResult(GLint *params)
{
    *params = 0;
    return gl::NoError();
}

gl::Error QueryNULL::getResult(GLuint *params)
{
    *params = 0;
    return gl::NoError();
}

gl::Error QueryNULL::getResult(GLint64 *params)
{
    *params = 0;
    return gl::NoError();
}

gl::Error QueryNULL::getResult(GLuint64 *params)
{
    *params = 0;
    return gl::NoError();
}

gl::Error QueryNULL::isResultAvailable(bool *available)
{
    *available = true;
    return gl::NoError();
}

}  // namespace rx
