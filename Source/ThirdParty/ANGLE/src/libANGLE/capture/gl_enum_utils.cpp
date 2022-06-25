//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_enum_utils.cpp:
//   Utility functions for converting GLenums to string.

#include "libANGLE/capture/gl_enum_utils.h"

#include "common/bitset_utils.h"

#include <iomanip>
#include <sstream>

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

    if (enumGroup == GLenumGroup::Boolean)
    {
        // If an unknown enum was submitted as GLboolean, just write out the value.
        if (enumStr == kUnknownGLenumString)
        {
            out << value;
        }
        else
        {
            out << enumStr;
        }

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
    return GLenumToString(GLenumGroup::Boolean, value);
}

std::string GLbitfieldToString(GLenumGroup enumGroup, unsigned int value)
{
    std::stringstream st;

    if (value == 0)
    {
        return "0";
    }

    const angle::BitSet<32> bitSet(value);
    bool first = true;
    for (const auto index : bitSet)
    {
        if (!first)
        {
            st << " | ";
        }
        first = false;

        unsigned int mask = 1u << index;
        OutputGLenumString(st, enumGroup, mask);
    }

    return st.str();
}

const char *GLinternalFormatToString(unsigned int format)
{
    return GLenumToString(gl::GLenumGroup::InternalFormat, format);
}
}  // namespace gl
