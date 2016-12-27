//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES31.h: Validation functions for OpenGL ES 3.1 entry point parameters

#ifndef LIBANGLE_VALIDATION_ES31_H_
#define LIBANGLE_VALIDATION_ES31_H_

#include <GLES3/gl31.h>

namespace gl
{
class Context;

bool ValidateGetBooleani_v(Context *context, GLenum target, GLuint index, GLboolean *data);
bool ValidateGetBooleani_vRobustANGLE(Context *context,
                                      GLenum target,
                                      GLuint index,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLboolean *data);

}  // namespace gl

#endif  // LIBANGLE_VALIDATION_ES31_H_
