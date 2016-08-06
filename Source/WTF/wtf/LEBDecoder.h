/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Compiler.h"
#include "Vector.h"
#include <algorithm>

#include "DataLog.h"

// This file contains a bunch of helper functions for decoding LEB numbers.
// See https://en.wikipedia.org/wiki/LEB128 for more information about the
// LEB format.

const size_t maxLEBByteLength = 5;

inline bool WARN_UNUSED_RETURN decodeUInt32(const Vector<uint8_t>& bytes, size_t& offset, uint32_t& result)
{
    ASSERT(bytes.size() > offset);
    result = 0;
    unsigned shift = 0;
    size_t last = std::min(maxLEBByteLength, bytes.size() - offset - 1);
    for (unsigned i = 0; true; ++i) {
        uint8_t byte = bytes[offset++];
        result |= (byte & 0x7f) << shift;
        shift += 7;
        if (!(byte & 0x80))
            return true;
        if (i == last)
            return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

inline bool WARN_UNUSED_RETURN decodeInt32(const Vector<uint8_t>& bytes, size_t& offset, int32_t& result)
{
    ASSERT(bytes.size() > offset);
    result = 0;
    unsigned shift = 0;
    size_t last = std::min(maxLEBByteLength, bytes.size() - offset - 1);
    uint8_t byte;
    for (unsigned i = 0; true; ++i) {
        byte = bytes[offset++];
        result |= (byte & 0x7f) << shift;
        shift += 7;
        if (!(byte & 0x80))
            break;
        if (i == last)
            return false;
    }

    if (shift < 32 && (byte & 0x40))
        result |= ((-1u) << shift);
    return true;
}
