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

#include "config.h"
#include "CSSValueList.h"

#include "CSSPrimitiveValue.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator)
    : CSSValue(type)
{
    m_valueSeparator = separator;
}

CSSValueContainingVector::CSSValueContainingVector(ClassType type, ValueSeparator separator, CSSValueListBuilder values)
    : CSSValue(type)
    , m_values(WTFMove(values))
{
    m_valueSeparator = separator;
}

CSSValueList::CSSValueList(ValueSeparator separator)
    : CSSValueContainingVector(ValueListClass, separator)
{
}

CSSValueList::CSSValueList(ValueSeparator separator, CSSValueListBuilder values)
    : CSSValueContainingVector(ValueListClass, separator, WTFMove(values))
{
}

Ref<CSSValueList> CSSValueList::createCommaSeparated()
{
    return adoptRef(*new CSSValueList(CommaSeparator));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated()
{
    return adoptRef(*new CSSValueList(SpaceSeparator));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated()
{
    return adoptRef(*new CSSValueList(SlashSeparator));
}

Ref<CSSValueList> CSSValueList::createCommaSeparated(CSSValueListBuilder values)
{
    return adoptRef(*new CSSValueList(CommaSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createSpaceSeparated(CSSValueListBuilder values)
{
    return adoptRef(*new CSSValueList(SpaceSeparator, WTFMove(values)));
}

Ref<CSSValueList> CSSValueList::createSlashSeparated(CSSValueListBuilder values)
{
    return adoptRef(*new CSSValueList(SlashSeparator, WTFMove(values)));
}

bool CSSValueContainingVector::removeAll(CSSValue& value)
{
    return m_values.removeAllMatching([&value](auto& current) {
        return current->equals(value);
    }) > 0;
}

bool CSSValueContainingVector::removeAll(CSSValueID value)
{
    return m_values.removeAllMatching([value](auto& current) {
        return WebCore::isValueID(current, value);
    }) > 0;
}

bool CSSValueContainingVector::hasValue(CSSValue& otherValue) const
{
    for (auto& value : m_values) {
        if (value->equals(otherValue))
            return true;
    }
    return false;
}

bool CSSValueContainingVector::hasValue(CSSValueID otherValue) const
{
    for (auto& value : m_values) {
        if (WebCore::isValueID(value, otherValue))
            return true;
    }
    return false;
}

Ref<CSSValueList> CSSValueList::copy()
{
    return adoptRef(*new CSSValueList(separator(), values()));
}

void CSSValueContainingVector::serializeItems(StringBuilder& builder) const
{
    auto prefix = ""_s;
    auto separator = separatorCSSText();
    for (auto& value : values())
        builder.append(std::exchange(prefix, separator), value.get().cssText());
}

String CSSValueContainingVector::serializeItems() const
{
    StringBuilder result;
    serializeItems(result);
    return result.toString();
}

String CSSValueList::customCSSText() const
{
    return serializeItems();
}

bool CSSValueContainingVector::itemsEqual(const CSSValueContainingVector& other) const
{
    unsigned size = m_values.size();
    if (size != other.m_values.size())
        return false;
    for (unsigned i = 0; i < size; ++i) {
        if (!m_values[i].get().equals(other.m_values[i]))
            return false;
    }
    return true;
}

bool CSSValueList::equals(const CSSValueList& other) const
{
    return separator() == other.separator() && itemsEqual(other);
}

bool CSSValueContainingVector::containsSingleEqualItem(const CSSValue& other) const
{
    return m_values.size() == 1 && m_values[0].get().equals(other);
}

bool CSSValueContainingVector::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto& value : *this) {
        if (value.get().traverseSubresources(handler))
            return true;
    }
    return false;
}

} // namespace WebCore
