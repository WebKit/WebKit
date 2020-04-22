//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PathNULL.cpp:
//    Implements the class methods for PathNULL.
//

#include "libANGLE/renderer/null/PathNULL.h"

#include "common/debug.h"

namespace rx
{
PathNULL::PathNULL() : PathImpl() {}

PathNULL::~PathNULL() {}

angle::Result PathNULL::setCommands(GLsizei numCommands,
                                    const GLubyte *commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const void *coords)
{
    return angle::Result::Continue;
}

void PathNULL::setPathParameter(GLenum pname, GLfloat value) {}
}  // namespace rx
