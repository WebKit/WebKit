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

class CSSValueList : public CSSValue {
public:
    typedef Vector<Ref<CSSValue>, 4>::iterator iterator;
    typedef Vector<Ref<CSSValue>, 4>::const_iterator const_iterator;

    static Ref<CSSValueList> createCommaSeparated()
    {
        return adoptRef(*new CSSValueList(CommaSeparator));
    }
    static Ref<CSSValueList> createSpaceSeparated()
    {
        return adoptRef(*new CSSValueList(SpaceSeparator));
    }
    static Ref<CSSValueList> createSlashSeparated()
    {
        return adoptRef(*new CSSValueList(SlashSeparator));
    }

    Ref<CSSValueList> copy();

    size_t length() const { return m_values.size(); }
    CSSValue* item(size_t index) { return index < m_values.size() ? m_values[index].ptr() : nullptr; }
    const CSSValue* item(size_t index) const { return index < m_values.size() ? m_values[index].ptr() : nullptr; }
    CSSValue* itemWithoutBoundsCheck(size_t index) { return m_values[index].ptr(); }
    const CSSValue* itemWithoutBoundsCheck(size_t index) const { ASSERT(index < m_values.size()); return m_values[index].ptr(); }

    const_iterator begin() const { return m_values.begin(); }
    const_iterator end() const { return m_values.end(); }
    iterator begin() { return m_values.begin(); }
    iterator end() { return m_values.end(); }
    size_t size() const { return m_values.size(); }

    void append(Ref<CSSValue>&&);
    void prepend(Ref<CSSValue>&&);
    bool removeAll(CSSValue&);
    bool removeAll(CSSValueID);
    bool hasValue(CSSValue&) const;
    bool hasValue(CSSValueID) const;

    String customCSSText() const;
    bool equals(const CSSValueList&) const;
    bool equals(const CSSValue&) const;

    bool customTraverseSubresources(const Function<bool(const CachedResource&)>&) const;

    using CSSValue::separator;
    using CSSValue::separatorCSSText;

protected:
    CSSValueList(ClassType, ValueSeparator);

private:
    explicit CSSValueList(ValueSeparator);

    Vector<Ref<CSSValue>, 4> m_values;
};

inline void CSSValueList::append(Ref<CSSValue>&& value)
{
    m_values.append(WTFMove(value));
}

inline void CSSValueList::prepend(Ref<CSSValue>&& value)
{
    m_values.insert(0, WTFMove(value));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSValueList, isValueList())
