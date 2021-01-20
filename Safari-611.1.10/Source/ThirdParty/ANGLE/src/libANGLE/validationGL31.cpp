//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL31.cpp: Validation functions for OpenGL 3.1 entry point parameters

#include "libANGLE/validationGL31_autogen.h"

namespace gl
{

bool ValidateGetActiveUniformName(const Context *context,
                                  ShaderProgramID program,
                                  GLuint uniformIndex,
                                  GLsizei bufSize,
                                  const GLsizei *length,
                                  const GLchar *uniformName)
{
    return true;
}

bool ValidatePrimitiveRestartIndex(const Context *context, GLuint index)
{
    return true;
}

}  // namespace gl
