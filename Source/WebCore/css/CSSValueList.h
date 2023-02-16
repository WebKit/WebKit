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

#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include <wtf/Function.h>
#include <wtf/Vector.h>

namespace WebCore {

using CSSValueListBuilder = Vector<Ref<CSSValue>, 4>;

class CSSValueContainingVector : public CSSValue {
public:
    // Remove these functions; use size() and operator[] instead.
    size_t length() const { return m_values.size(); }
    CSSValue* item(size_t index) { return index < m_values.size() ? m_values[index].ptr() : nullptr; }
    const CSSValue* item(size_t index) const { return index < m_values.size() ? m_values[index].ptr() : nullptr; }
    CSSValue* itemWithoutBoundsCheck(size_t index) { return m_values[index].ptr(); }
    const CSSValue* itemWithoutBoundsCheck(size_t index) const { ASSERT(index < m_values.size()); return m_values[index].ptr(); }

    const CSSValue& operator[](size_t index) const { return m_values[index]; }
    using const_iterator = CSSValueListBuilder::const_iterator;
    using iterator = const_iterator;
    iterator begin() const { return m_values.begin(); }
    iterator end() const { return m_values.end(); }
    size_t size() const { return m_values.size(); }

    // FIXME: Remove these functions and instead always construct with a CSSValueListBuilder.
    void append(Ref<CSSValue>&&);
    void prepend(Ref<CSSValue>&&);
    bool removeAll(CSSValue&);
    bool removeAll(CSSValueID);

    bool hasValue(CSSValue&) const;
    bool hasValue(CSSValueID) const;

    void serializeItems(StringBuilder&) const;
    String serializeItems() const;

    bool itemsEqual(const CSSValueContainingVector&) const;
    bool containsSingleEqualItem(const CSSValue&) const;

    using CSSValue::separator;
    using CSSValue::separatorCSSText;

    bool customTraverseSubresources(const Function<bool(const CachedResource&)>&) const;

protected:
    CSSValueContainingVector(ClassType, ValueSeparator);
    CSSValueContainingVector(ClassType, ValueSeparator, CSSValueListBuilder);

    const CSSValueListBuilder& values() const { return m_values; }

private:
    CSSValueListBuilder m_values;
};

class CSSValueList : public CSSValueContainingVector {
public:
    static Ref<CSSValueList> createCommaSeparated();
    static Ref<CSSValueList> createSpaceSeparated();
    static Ref<CSSValueList> createSlashSeparated();
    static Ref<CSSValueList> createCommaSeparated(CSSValueListBuilder);
    static Ref<CSSValueList> createSpaceSeparated(CSSValueListBuilder);
    static Ref<CSSValueList> createSlashSeparated(CSSValueListBuilder);

    Ref<CSSValueList> copy();

    String customCSSText() const;
    bool equals(const CSSValueList&) const;

private:
    explicit CSSValueList(ValueSeparator);
    CSSValueList(ValueSeparator, CSSValueListBuilder);
};

inline void CSSValueContainingVector::append(Ref<CSSValue>&& value)
{
    m_values.append(WTFMove(value));
}

inline void CSSValueContainingVector::prepend(Ref<CSSValue>&& value)
{
    m_values.insert(0, WTFMove(value));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueContainingVector, containsVector())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueList, isValueList())
