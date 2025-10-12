/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include <WebCore/CSSValue.h>
#include <array>
#include <ranges>
#include <unicode/umachine.h>
#include <wtf/MallocSpan.h>

namespace WebCore {

static constexpr size_t CSSValueListBuilderInlineCapacity = 4;
using CSSValueListBuilder = Vector<Ref<CSSValue>, CSSValueListBuilderInlineCapacity>;

class CSSValueContainingVector : public CSSValue {
public:
    unsigned size() const { return m_size; }
    const CSSValue& operator[](unsigned index) const LIFETIME_BOUND;

    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = const CSSValue&;
        using difference_type = ptrdiff_t;
        using pointer = const CSSValue*;
        using reference = const CSSValue&;

        const CSSValue& operator*() const { return (*vector)[index]; }
        iterator& operator++() { ++index; return *this; }
        iterator operator++(int)
        {
            auto result = *this;
            ++*this;
            return result;
        }
        constexpr bool operator==(const iterator& other) const { return index == other.index; }

        const CSSValueContainingVector* vector { nullptr };
        unsigned index { 0 };
    };
    using const_iterator = iterator;

    iterator begin() const LIFETIME_BOUND { return { this, 0 }; }
    iterator end() const LIFETIME_BOUND { return { this, size() }; }

    bool hasValue(CSSValue&) const;
    bool hasValue(CSSValueID) const;

    void serializeItems(StringBuilder&, const CSS::SerializationContext&) const;
    String serializeItems(const CSS::SerializationContext&) const;

    bool itemsEqual(const CSSValueContainingVector&) const;
    bool containsSingleEqualItem(const CSSValue&) const;

    using CSSValue::separator;
    using CSSValue::separatorCSSText;

    bool customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>&) const;

    CSSValueListBuilder copyValues() const;

    // Consider removing these functions and having callers use size() and operator[] instead.
    unsigned length() const { return size(); }
    const CSSValue* item(unsigned index) const LIFETIME_BOUND { return index < size() ? &(*this)[index] : nullptr; }
    RefPtr<const CSSValue> protectedItem(unsigned index) const { return item(index); }
    const CSSValue* itemWithoutBoundsCheck(unsigned index) const LIFETIME_BOUND { return &(*this)[index]; }

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

protected:
    friend bool CSSValue::addHash(Hasher&) const;

    CSSValueContainingVector(ClassType, ValueSeparator);
    CSSValueContainingVector(ClassType, ValueSeparator, CSSValueListBuilder);
    CSSValueContainingVector(ClassType, ValueSeparator, Ref<CSSValue>);
    CSSValueContainingVector(ClassType, ValueSeparator, Ref<CSSValue>, Ref<CSSValue>);
    CSSValueContainingVector(ClassType, ValueSeparator, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>);
    CSSValueContainingVector(ClassType, ValueSeparator, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>);
    ~CSSValueContainingVector();

    bool addDerivedHash(Hasher&) const;

private:
    unsigned m_size { 0 };
    std::array<const CSSValue*, 4> m_inlineStorage;
    MallocSpan<const CSSValue*> m_additionalStorage;
};
static_assert(std::ranges::range<CSSValueContainingVector>);

class CSSValueList final : public CSSValueContainingVector {
public:
    static Ref<CSSValueList> create(char16_t separator, CSSValueListBuilder);

    static Ref<CSSValueList> createCommaSeparated(CSSValueListBuilder);
    static Ref<CSSValueList> createCommaSeparated(Ref<CSSValue>); // FIXME: Upgrade callers to not use a list at all.

    static Ref<CSSValueList> createSpaceSeparated(CSSValueListBuilder);
    static Ref<CSSValueList> createSpaceSeparated(); // FIXME: Get rid of the caller that needs to create an empty list.
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>); // FIXME: Upgrade callers to not use a list at all.
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>, Ref<CSSValue>);
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>);
    static Ref<CSSValueList> createSpaceSeparated(Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>);

    static Ref<CSSValueList> createSlashSeparated(CSSValueListBuilder);
    static Ref<CSSValueList> createSlashSeparated(Ref<CSSValue>); // FIXME: Upgrade callers to not use a list at all.
    static Ref<CSSValueList> createSlashSeparated(Ref<CSSValue>, Ref<CSSValue>);

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSValueList&) const;

private:
    friend void add(Hasher&, const CSSValueList&);

    explicit CSSValueList(ValueSeparator);
    CSSValueList(ValueSeparator, CSSValueListBuilder);
    CSSValueList(ValueSeparator, Ref<CSSValue>);
    CSSValueList(ValueSeparator, Ref<CSSValue>, Ref<CSSValue>);
    CSSValueList(ValueSeparator, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>);
    CSSValueList(ValueSeparator, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>, Ref<CSSValue>);
};

inline CSSValueContainingVector::~CSSValueContainingVector()
{
    for (auto& value : *this)
        value.deref();
}

inline const CSSValue& CSSValueContainingVector::operator[](unsigned index) const
{
    unsigned maxInlineSize = m_inlineStorage.size();
    if (index < maxInlineSize) {
        ASSERT(index < m_size);
        return *m_inlineStorage[index];
    }
    ASSERT(index < m_size);
    return *m_additionalStorage[index - maxInlineSize];
}

void add(Hasher&, const CSSValueContainingVector&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueContainingVector, containsVector())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueList, isValueList())
