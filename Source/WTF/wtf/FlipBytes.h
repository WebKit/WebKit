/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <array>
#include <bit>
#include <wtf/Compiler.h>

namespace WTF {

constexpr bool needToFlipBytesIfLittleEndian(bool littleEndian)
{
#if CPU(BIG_ENDIAN)
    return littleEndian;
#else
    return !littleEndian;
#endif
}

template<typename T>
inline T flipBytes(T value)
{
#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
    if constexpr (std::is_integral_v<T>)
        return std::byteswap(value);
#endif

    auto byteRepresentation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(byteRepresentation);
    return std::bit_cast<T>(byteRepresentation);
}

template<typename T>
inline T flipBytesIfLittleEndian(T value, bool littleEndian)
{
    if (needToFlipBytesIfLittleEndian(littleEndian))
        return flipBytes(value);
    return value;
}

} // namespace WTF

using WTF::flipBytes;
using WTF::flipBytesIfLittleEndian;
using WTF::needToFlipBytesIfLittleEndian;
