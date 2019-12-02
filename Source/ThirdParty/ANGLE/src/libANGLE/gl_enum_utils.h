//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_enum_utils.h:
//   Utility functions for converting GLenums to string.

#ifndef LIBANGLE_GL_ENUM_UTILS_H_
#define LIBANGLE_GL_ENUM_UTILS_H_

#include <ostream>
#include <string>

#include "libANGLE/gl_enum_utils_autogen.h"

namespace gl
{
const char *GLbooleanToString(unsigned int value);
const char *GLenumToString(GLenumGroup enumGroup, unsigned int value);
std::string GLbitfieldToString(GLenumGroup enumGroup, unsigned int value);
void OutputGLenumString(std::ostream &out, GLenumGroup enumGroup, unsigned int value);
void OutputGLbitfieldString(std::ostream &out, GLenumGroup enumGroup, unsigned int value);

extern const char kUnknownGLenumString[];
}  // namespace gl

#endif  // LIBANGLE_GL_ENUM_UTILS_H_
