/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3Common.h"
#include <wtf/StdLibExtras.h>

#if !ASSERT_ENABLED
IGNORE_RETURN_TYPE_WARNINGS_BEGIN
#endif

namespace JSC { namespace B3 {

static constexpr uint32_t tupleFlag = 1ul << (std::numeric_limits<uint32_t>::digits - 1);
static constexpr uint32_t tupleIndexMask = tupleFlag - 1;

enum TypeKind : uint32_t {
    Void,
    Int32,
    Int64,
    Float,
    Double,

    // Tuples are represented as the tupleFlag | with the tuple's index into Procedure's m_tuples table.
    Tuple = tupleFlag,
};

class Type {
public:
    constexpr Type() = default;
    constexpr Type(const Type&) = default;
    constexpr Type(TypeKind kind)
        : m_kind(kind)
    { }

    ~Type() = default;

    static Type tupleFromIndex(unsigned index) { ASSERT(!(index & tupleFlag)); return static_cast<TypeKind>(index | tupleFlag); }

    TypeKind kind() const { return m_kind & tupleFlag ? Tuple : m_kind; }
    uint32_t tupleIndex() const { ASSERT(m_kind & tupleFlag); return m_kind & tupleIndexMask; }
    uint32_t hash() const { return m_kind; }

    inline bool isInt() const;
    inline bool isFloat() const;
    inline bool isNumeric() const;
    inline bool isTuple() const;

    bool operator==(const TypeKind& otherKind) const { return kind() == otherKind; }
    bool operator==(const Type& type) const { return m_kind == type.m_kind; }
    bool operator!=(const TypeKind& otherKind) const { return !(*this == otherKind); }
    bool operator!=(const Type& type) const { return !(*this == type); }

private:
    TypeKind m_kind { Void };
};

static_assert(sizeof(TypeKind) == sizeof(Type));

inline bool Type::isInt() const
{
    return kind() == Int32 || kind() == Int64;
}

inline bool Type::isFloat() const
{
    return kind() == Float || kind() == Double;
}

inline bool Type::isNumeric() const
{
    return isInt() || isFloat();
}

inline bool Type::isTuple() const
{
    return kind() == Tuple;
}

inline Type pointerType()
{
    if (is32Bit())
        return Int32;
    return Int64;
}

inline size_t sizeofType(Type type)
{
    switch (type.kind()) {
    case Void:
    case Tuple:
        return 0;
    case Int32:
    case Float:
        return 4;
    case Int64:
    case Double:
        return 8;
    }
    ASSERT_NOT_REACHED();
}

} } // namespace JSC::B3

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::B3::Type);

} // namespace WTF

#if !ASSERT_ENABLED
IGNORE_RETURN_TYPE_WARNINGS_END
#endif

#endif // ENABLE(B3_JIT)
