//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImmutableString.cpp: Wrapper for static or pool allocated char arrays, that are guaranteed to be
// valid and unchanged for the duration of the compilation.
//

#include "compiler/translator/ImmutableString.h"

std::ostream &operator<<(std::ostream &os, const sh::ImmutableString &str)
{
    return os.write(str.data(), str.length());
}

#if defined(_MSC_VER)
#    pragma warning(disable : 4309)  // truncation of constant value
#endif

namespace sh
{

template <>
const size_t ImmutableString::FowlerNollVoHash<4>::kFnvPrime = 16777619u;

template <>
const size_t ImmutableString::FowlerNollVoHash<4>::kFnvOffsetBasis = 0x811c9dc5u;

template <>
const size_t ImmutableString::FowlerNollVoHash<8>::kFnvPrime =
    static_cast<size_t>(1099511628211ull);

template <>
const size_t ImmutableString::FowlerNollVoHash<8>::kFnvOffsetBasis =
    static_cast<size_t>(0xcbf29ce484222325ull);

uint32_t ImmutableString::mangledNameHash() const
{
    const char *dataPtr              = data();
    uint32_t hash                    = static_cast<uint32_t>(FowlerNollVoHash<4>::kFnvOffsetBasis);
    const uint32_t kMaxSixBitValue   = (1u << 6) - 1u;
    uint32_t parenLocation           = kMaxSixBitValue;
    uint32_t hasArrayOrBlockParamBit = 0u;
    uint32_t index                   = 0;
    while (dataPtr[index] != '\0')
    {
        hash = hash ^ dataPtr[index];
        hash = hash * static_cast<uint32_t>(FowlerNollVoHash<4>::kFnvPrime);
        if (dataPtr[index] == '(')
        {
            // We should only reach here once, since this function should not be called with invalid
            // mangled names.
            ASSERT(parenLocation == kMaxSixBitValue);
            parenLocation = index;
        }
        else if (dataPtr[index] == '{' || dataPtr[index] == '[')
        {
            hasArrayOrBlockParamBit = 1u;
        }
        ++index;
    }
    // Should not be called with strings longer than 63 characters.
    ASSERT(index <= kMaxSixBitValue);
    return ((hash >> 13) ^ (hash & 0x1fff)) | (index << 19) | (parenLocation << 25) |
           (hasArrayOrBlockParamBit << 31);
}

}  // namespace sh
