/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "CSSIdentValue.h"
#include <array>
#include <unicode/umachine.h>

namespace WebCore {

using CSSValueListBuilder = Vector<Ref<CSSValue>, 4>;

class CSSValueContainingVector : public CSSValue {
public:
    unsigned size() const;
    const CSSValue& operator[](unsigned index) const;

    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = const CSSValue&;
        using difference_type = ptrdiff_t;
        using pointer = const CSSValue*;
        using reference = const CSSValue&;

        const CSSValue& operator*() const { return vector[index]; }
        iterator& operator++() { ++index; return *this; }
        constexpr bool operator==(const iterator& other) const { return index == other.index; }
        constexpr bool operator!=(const iterator& other) const { return index != other.index; }

        const CSSValueContainingVector& vector;
        unsigned index { 0 };
    };
    using const_iterator = iterator;

    iterator begin() const { return { *this, 0 }; }
    iterator end() const { return { *this, size() }; }

    bool hasValue(CSSValue&) const;
    bool hasValue(CSSValueID) const;

    void serializeItems(StringBuilder&) const;
    String serializeItems() const;

    bool itemsEqual(const CSSValueContainingVector&) const;
    bool containsSingleEqualItem(const CSSValue&) const;

    using CSSValue::separator;
    using CSSValue::separatorCSSText;

    bool customTraverseSubresources(const Function<bool(const CachedResource&)>&) const;

    CSSValueListBuilder copyValues() const;

    // Consider removing these functions and having callers use size() and operator[] instead.
    unsigned length() const { return size(); }
    const CSSValue* item(unsigned index) const { return index < size() ? &(*this)[index] : nullptr; }
    const CSSValue* itemWithoutBoundsCheck(unsigned index) const { return &(*this)[index]; }

protected:
    // Encode values in the scalar.
    static constexpr uint8_t SizeMinusOneShift = PayloadShift;
    static constexpr uint8_t SizeMinusOneBits = 2;
    static constexpr uintptr_t SizeMinusOneMask = (1 << SizeMinusOneBits) - 1;
    static constexpr uint8_t ValueIDsShift = PayloadShift + SizeMinusOneBits;

    CSSValueContainingVector(Type, ValueSeparator, CSSValueListBuilder&&);
    CSSValueContainingVector(Type, ValueSeparator);
    CSSValueContainingVector(Type, ValueSeparator, Ref<CSSValue>&&);
    CSSValueContainingVector(Type, ValueSeparator, Ref<CSSValue>&&, Ref<CSSValue>&&);
    CSSValueContainingVector(Type, ValueSeparator, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
    CSSValueContainingVector(Type, ValueSeparator, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
    ~CSSValueContainingVector();

private:
    unsigned m_size { 0 };
    std::array<const CSSValue*, 4> m_inlineStorage;
    const CSSValue** m_additionalStorage;
};

class CSSValueList : public CSSValueContainingVector {
public:
    static Ref<CSSValueList> create(UChar separator, CSSValueListBuilder&&);

    static Ref<CSSValueList> createCommaSeparated(CSSValueListBuilder&&);
    static Ref<CSSValueList> createCommaSeparated(Ref<CSSValue>&&); // FIXME: Upgrade callers to not use a list at all.

    static Ref<CSSValueList> createSpaceSeparated(CSSValueListBuilder&&);
    static Ref<CSSValueList> createSpaceSeparated(Span<const CSSValueID>);
    static Ref<CSSValueList> createSpaceSeparated(); // FIXME: Get rid of the caller that needs to create an empty list.
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>&&); // FIXME: Upgrade callers to not use a list at all.
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>&&, Ref<CSSValue>&&); // FIXME: Should some of these be CSSValuePair instead?
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
    static Ref<CSSValueList> createSpaceSeparated(CSSValueID); // FIXME: Upgrade callers to not use a list at all.
    static Ref<CSSValueList> createSpaceSeparated(CSSValueID, CSSValueID); // FIXME: Should some of these be CSSValuePair instead?
    static Ref<CSSValueList> createSpaceSeparated(CSSValueID, CSSValueID, CSSValueID);
    static Ref<CSSValueList> createSpaceSeparated(CSSValueID, CSSValueID, CSSValueID, CSSValueID);

    static Ref<CSSValueList> createSlashSeparated(CSSValueListBuilder&&);
    static Ref<CSSValueList> createSlashSeparated(Ref<CSSValue>&&); // FIXME: Upgrade callers to not use a list at all.
    static Ref<CSSValueList> createSlashSeparated(Ref<CSSValue>&&, Ref<CSSValue>&&z);

    String customCSSText() const;
    bool equals(const CSSValueList&) const;

private:
    static CSSValueList& listScalar(unsigned size, uintptr_t shiftedValues);
    static uintptr_t shiftedValueID(CSSValueID, unsigned index);

    CSSValueList(ValueSeparator, CSSValueListBuilder&&);
    explicit CSSValueList(ValueSeparator);
    CSSValueList(ValueSeparator, Ref<CSSValue>&&);
    CSSValueList(ValueSeparator, Ref<CSSValue>&&, Ref<CSSValue>&&);
    CSSValueList(ValueSeparator, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
    CSSValueList(ValueSeparator, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&, Ref<CSSValue>&&);
};

inline CSSValueContainingVector::~CSSValueContainingVector()
{
    for (auto& value : *this)
        value.deref();
    if (m_size > m_inlineStorage.size())
        fastFree(m_additionalStorage);
}

inline unsigned CSSValueContainingVector::size() const
{
    if (hasScalarInPointer())
        return (scalar() >> SizeMinusOneShift & SizeMinusOneMask) + 1;
    return opaque(this)->m_size;
}

inline const CSSValue& CSSValueContainingVector::operator[](unsigned index) const
{
    ASSERT(index < size());
    if (hasScalarInPointer())
        return CSSIdentValue::create(static_cast<CSSValueID>(scalar() >> (ValueIDsShift + index * IdentBits) & IdentMask)).get();
    auto& self = *opaque(this);
    unsigned maxInlineSize = self.m_inlineStorage.size();
    if (index < maxInlineSize)
        return *self.m_inlineStorage[index];
    return *self.m_additionalStorage[index - maxInlineSize];
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueContainingVector, containsVector())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueList, isValueList())
