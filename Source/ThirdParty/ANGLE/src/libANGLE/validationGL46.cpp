//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL46.cpp: Validation functions for OpenGL 4.6 entry point parameters

#include "libANGLE/validationGL46_autogen.h"

namespace gl
{

bool ValidateMultiDrawArraysIndirectCount(Context *context,
                                          GLenum mode,
                                          const void *indirect,
                                          GLintptr drawcount,
                                          GLsizei maxdrawcount,
                                          GLsizei stride)
{
    return true;
}

bool ValidateMultiDrawElementsIndirectCount(Context *context,
                                            GLenum mode,
                                            GLenum type,
                                            const void *indirect,
                                            GLintptr drawcount,
                                            GLsizei maxdrawcount,
                                            GLsizei stride)
{
    return true;
}

bool ValidatePolygonOffsetClamp(Context *context, GLfloat factor, GLfloat units, GLfloat clamp)
{
    return true;
}

bool ValidateSpecializeShader(Context *context,
                              GLuint shader,
                              const GLchar *pEntryPoint,
                              GLuint numSpecializationConstants,
                              const GLuint *pConstantIndex,
                              const GLuint *pConstantValue)
{
    return true;
}

}  // namespace gl
