/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <wtf/HashTraits.h>

namespace WTF {
class PrintStream;
}

namespace JSC {

class BytecodeIndex {
public:
    BytecodeIndex() = default;
    BytecodeIndex(WTF::HashTableDeletedValueType)
        : m_offset(deletedValue().asBits())
    {
    }
    explicit BytecodeIndex(uint32_t bytecodeOffset)
        : m_offset(bytecodeOffset)
    { }

    uint32_t offset() const { return m_offset; }
    uint32_t asBits() const { return m_offset; }

    unsigned hash() const { return WTF::intHash(m_offset); }
    static BytecodeIndex deletedValue() { return fromBits(invalidOffset - 1); }
    bool isHashTableDeletedValue() const { return *this == deletedValue(); }

    static BytecodeIndex fromBits(uint32_t bits);

    // Comparison operators.
    explicit operator bool() const { return m_offset != invalidOffset && m_offset != deletedValue().offset(); }
    bool operator ==(const BytecodeIndex& other) const { return asBits() == other.asBits(); }
    bool operator !=(const BytecodeIndex& other) const { return !(*this == other); }

    bool operator <(const BytecodeIndex& other) const { return asBits() < other.asBits(); }
    bool operator >(const BytecodeIndex& other) const { return asBits() > other.asBits(); }
    bool operator <=(const BytecodeIndex& other) const { return asBits() <= other.asBits(); }
    bool operator >=(const BytecodeIndex& other) const { return asBits() >= other.asBits(); }


    void dump(WTF::PrintStream&) const;

private:
    static constexpr uint32_t invalidOffset = std::numeric_limits<uint32_t>::max();

    uint32_t m_offset { invalidOffset };
};

inline BytecodeIndex BytecodeIndex::fromBits(uint32_t bits)
{
    BytecodeIndex result;
    result.m_offset = bits;
    return result;
}

struct BytecodeIndexHash {
    static unsigned hash(const BytecodeIndex& key) { return key.hash(); }
    static bool equal(const BytecodeIndex& a, const BytecodeIndex& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::BytecodeIndex> {
    typedef JSC::BytecodeIndexHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::BytecodeIndex> : SimpleClassHashTraits<JSC::BytecodeIndex> {
    static constexpr bool emptyValueIsZero = false;
};

} // namespace WTF
