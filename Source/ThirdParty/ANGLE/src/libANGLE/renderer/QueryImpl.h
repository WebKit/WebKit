//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryImpl.h: Defines the abstract rx::QueryImpl class.

#ifndef LIBANGLE_RENDERER_QUERYIMPL_H_
#define LIBANGLE_RENDERER_QUERYIMPL_H_

#include "libANGLE/Error.h"

#include "common/angleutils.h"

#include <GLES2/gl2.h>

namespace rx
{

class QueryImpl : angle::NonCopyable
{
  public:
    explicit QueryImpl(GLenum type) { mType = type; }
    virtual ~QueryImpl() { }

    virtual gl::Error begin() = 0;
    virtual gl::Error end() = 0;
    virtual gl::Error queryCounter() = 0;
    virtual gl::Error getResult(GLint *params) = 0;
    virtual gl::Error getResult(GLuint *params) = 0;
    virtual gl::Error getResult(GLint64 *params) = 0;
    virtual gl::Error getResult(GLuint64 *params) = 0;
    virtual gl::Error isResultAvailable(bool *available) = 0;

    GLenum getType() const { return mType;  }

  private:
    GLenum mType;
};

}

#endif // LIBANGLE_RENDERER_QUERYIMPL_H_
