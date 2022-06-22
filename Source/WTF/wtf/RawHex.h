/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

namespace WTF {

// For printing integral values in hex.
// See also the printInternal function for RaswHex in PrintStream.cpp.

class RawHex {
public:
    RawHex() = default;

    explicit RawHex(int8_t value)
        : RawHex(static_cast<uint8_t>(value))
    { }
    explicit RawHex(uint8_t value)
        : RawHex(static_cast<uintptr_t>(value))
    { }

    explicit RawHex(int16_t value)
        : RawHex(static_cast<uint16_t>(value))
    { }
    explicit RawHex(uint16_t value)
        : RawHex(static_cast<uintptr_t>(value))
    { }

#if CPU(ADDRESS64) || OS(DARWIN)
    // These causes build errors for CPU(ADDRESS32) on some ports because int32_t
    // is already handled by intptr_t, and uint32_t is handled by uintptr_t.
    explicit RawHex(int32_t value)
        : RawHex(static_cast<uint32_t>(value))
    { }
    explicit RawHex(uint32_t value)
        : RawHex(static_cast<uintptr_t>(value))
    { }
#endif

#if CPU(ADDRESS32) || OS(DARWIN)
    // These causes build errors for CPU(ADDRESS64) on some ports because int64_t
    // is already handled by intptr_t, and uint64_t is handled by uintptr_t.
    explicit RawHex(int64_t value)
        : RawHex(static_cast<uint64_t>(value))
    { }
#if CPU(ADDRESS64) // on OS(DARWIN)
    explicit RawHex(uint64_t value)
        : RawHex(static_cast<uintptr_t>(value))
    { }
#else
    explicit RawHex(uint64_t value)
        : m_is64Bit(true)
        , m_u64(value)
    { }
#endif
#endif // CPU(ADDRESS64)

    explicit RawHex(intptr_t value)
        : RawHex(static_cast<uintptr_t>(value))
    { }
    explicit RawHex(uintptr_t value)
        : m_ptr(reinterpret_cast<void*>(value))
    { }

    const void* ptr() const { return m_ptr; }

#if !CPU(ADDRESS64)
    bool is64Bit() const { return m_is64Bit; }
    uint64_t u64() const { return m_u64; }
#endif

private:
#if !CPU(ADDRESS64)
    bool m_is64Bit { false };
#endif
    union {
        const void* m_ptr { nullptr };
#if !CPU(ADDRESS64)
        uint64_t m_u64;
#endif
    };
};

} // namespace WTF

using WTF::RawHex;
