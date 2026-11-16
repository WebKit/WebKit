/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/CacheableIdentifier.h>
#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/JSGlobalObjectFunctions.h>
#include <JavaScriptCore/PrivateName.h>
#include <wtf/dtoa.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class PropertyName {
public:
    PropertyName()
        : m_impl(nullptr)
    {
    }

    // FIXME: Make PropertyName const-correct.
    PropertyName(const UniquedStringImpl* propertyName)
        : m_impl(const_cast<UniquedStringImpl*>(propertyName))
    {
    }

    PropertyName(UniquedStringImpl* propertyName)
        : m_impl(propertyName)
    {
    }

    PropertyName(const Identifier& propertyName)
        : PropertyName(propertyName.impl())
    {
    }

    // Defined in PropertyNameInlines.h.
    PropertyName(const CacheableIdentifier&);

    PropertyName(const PrivateName& propertyName)
        : m_impl(&propertyName.uid())
    {
        ASSERT(m_impl);
        ASSERT(m_impl->isSymbol());
    }

    bool isNull() const { return !m_impl; }

    bool isSymbol() const
    {
        return m_impl && m_impl->isSymbol();
    }

    bool isPrivateName() const
    {
        return isSymbol() && static_cast<const SymbolImpl*>(m_impl)->isPrivate();
    }

    UniquedStringImpl* uid() const
    {
        return m_impl;
    }

    AtomStringImpl* publicName() const
    {
        return (!m_impl || m_impl->isSymbol()) ? nullptr : static_cast<AtomStringImpl*>(m_impl);
    }

    void dump(PrintStream& out) const
    {
        if (m_impl)
            out.print(m_impl);
        else
            out.print("<null property name>");
    }

private:
    UniquedStringImpl* m_impl;
};
static_assert(sizeof(PropertyName) == sizeof(UniquedStringImpl*), "UniquedStringImpl* and PropertyName should be compatible to invoke easily from JIT code.");

inline bool operator==(PropertyName a, const Identifier& b)
{
    return a.uid() == b.impl();
}

inline bool operator==(PropertyName a, PropertyName b)
{
    return a.uid() == b.uid();
}

inline bool operator==(PropertyName a, const char* b)
{
    return equal(a.uid(), b);
}

ALWAYS_INLINE std::optional<uint32_t> parseIndex(PropertyName propertyName)
{
    auto uid = propertyName.uid();
    if (!uid)
        return std::nullopt;
    if (uid->isSymbol())
        return std::nullopt;
    return parseIndex(*uid);
}

template<typename CharacterType>
ALWAYS_INLINE std::optional<bool> fastIsCanonicalNumericIndexString(std::span<const CharacterType> characters)
{
    auto* rawCharacters = characters.data();
    auto length = characters.size();
    ASSERT(length >= 1);
    auto first = rawCharacters[0];
    if (length == 1)
        return isASCIIDigit(first);
    auto second = rawCharacters[1];
    if (first == '-') {
        // -Infinity case should go to the slow path. -NaN cannot exist since it becomes NaN.
        if (!isASCIIDigit(second) && (length != strlen("-Infinity") || second != 'I'))
            return false;
        if (length == 2) // Including -0, and it should be accepted.
            return true;
    } else if (!isASCIIDigit(first)) {
        // Infinity and NaN should go to the slow path.
        if (!(length == strlen("Infinity") && first == 'I') && !(length == strlen("NaN") && first == 'N'))
            return false;
    }
    return std::nullopt;
}

// https://www.ecma-international.org/ecma-262/9.0/index.html#sec-canonicalnumericindexstring
// An integer-indexed exotic object can be longer than MAX_ARRAY_INDEX, so a key denoting an index
// parseIndex() cannot hold still has to reach an element. Such a key always arrives as a string, so
// callers that must serve it pass integerIndex to have it reported. A canonical numeric index string
// covers far more than that (negatives, fractions, NaN, values past uint64_t), so anything outside the
// reportable range is still a canonical numeric index string with no index to report.
// integerIndex is a pointer rather than a reference so that the callers who only classify the key,
// which include a property lookup fast path, compile to exactly what they did before.
ALWAYS_INLINE bool isCanonicalNumericIndexString(UniquedStringImpl* propertyName, std::optional<uint64_t>* integerIndex = nullptr)
{
    if (integerIndex)
        *integerIndex = std::nullopt;
    if (!propertyName)
        return false;
    if (propertyName->isSymbol())
        return false;
    if (!propertyName->length())
        return false;

    auto fastResult = propertyName->is8Bit()
        ? fastIsCanonicalNumericIndexString(propertyName->span8())
        : fastIsCanonicalNumericIndexString(propertyName->span16());
    if (fastResult)
        return *fastResult;

    double index = jsToNumber(propertyName);
    NumberToStringBuffer buffer;
    auto span = WTF::numberToStringAndSize(index, buffer);
    if (!equal(propertyName, byteCast<Latin1Character>(span)))
        return false;

    if (integerIndex) {
        // Anything up to MAX_ARRAY_INDEX reached the caller through parseIndex() instead. The round trip
        // through uint64_t rejects a fractional value, which is canonical too ("4294967296.5") but is not
        // an integer index.
        constexpr double smallestReportedIndex = static_cast<double>(MAX_ARRAY_INDEX) + 1;
        constexpr double firstIndexPastUInt64 = static_cast<double>(std::numeric_limits<uint64_t>::max());
        if (index >= smallestReportedIndex && index < firstIndexPastUInt64) {
            uint64_t candidate = static_cast<uint64_t>(index);
            if (static_cast<double>(candidate) == index)
                *integerIndex = candidate;
        }
    }
    return true;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
