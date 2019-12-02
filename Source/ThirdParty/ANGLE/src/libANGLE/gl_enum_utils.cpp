//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_enum_utils.cpp:
//   Utility functions for converting GLenums to string.

#include "libANGLE/gl_enum_utils.h"

#include <iomanip>

namespace gl
{
const char kUnknownGLenumString[] = "EnumUnknown";

void OutputGLenumString(std::ostream &out, GLenumGroup enumGroup, unsigned int value)
{
    const char *enumStr = GLenumToString(enumGroup, value);
    if (enumStr != kUnknownGLenumString)
    {
        out << enumStr;
        return;
    }

    if (enumGroup != GLenumGroup::DefaultGroup)
    {
        // Retry with the "Default" group
        enumStr = GLenumToString(GLenumGroup::DefaultGroup, value);
        if (enumStr != kUnknownGLenumString)
        {
            out << enumStr;
            return;
        }
    }

    out << std::hex << "0x" << std::setfill('0') << std::setw(4) << value << std::dec;
}

void OutputGLbitfieldString(std::ostream &out, GLenumGroup enumGroup, unsigned int value)
{
    out << GLbitfieldToString(enumGroup, value);
}

const char *GLbooleanToString(unsigned int value)
{
    switch (value)
    {
        case 0x0:
            return "GL_FALSE";
        case 0x1:
            return "GL_TRUE";
        default:
            return kUnknownGLenumString;
    }
}
}  // namespace gl
