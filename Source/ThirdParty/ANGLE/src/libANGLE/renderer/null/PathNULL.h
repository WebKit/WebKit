//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PathNULL.h:
//    Defines the class interface for PathNULL, implementing PathImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_PATHNULL_H_
#define LIBANGLE_RENDERER_NULL_PATHNULL_H_

#include "libANGLE/renderer/PathImpl.h"

namespace rx
{

class PathNULL : public PathImpl
{
  public:
    PathNULL();
    ~PathNULL() override;

    gl::Error setCommands(GLsizei numCommands,
                          const GLubyte *commands,
                          GLsizei numCoords,
                          GLenum coordType,
                          const void *coords) override;

    void setPathParameter(GLenum pname, GLfloat value) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_PATHNULL_H_
