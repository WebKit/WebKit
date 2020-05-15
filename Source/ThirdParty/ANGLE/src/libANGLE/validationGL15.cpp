//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL15.cpp: Validation functions for OpenGL 1.5 entry point parameters

#include "libANGLE/validationGL15_autogen.h"

namespace gl
{

bool ValidateGetBufferSubData(const Context *context,
                              GLenum target,
                              GLintptr offset,
                              GLsizeiptr size,
                              const void *data)
{
    return true;
}

bool ValidateGetQueryObjectiv(const Context *context, QueryID id, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateMapBuffer(const Context *context, BufferBinding targetPacked, GLenum access)
{
    return true;
}

}  // namespace gl
