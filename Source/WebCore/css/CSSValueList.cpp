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

#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSValueList::CSSValueList(ClassType classType, ValueSeparator listSeparator)
    : CSSValue(classType)
{
    m_valueSeparator = listSeparator;
}

CSSValueList::CSSValueList(ValueSeparator listSeparator)
    : CSSValue(ValueListClass)
{
    m_valueSeparator = listSeparator;
}

bool CSSValueList::removeAll(CSSValue* value)
{
    // FIXME: Why even take a pointer?
    if (!value)
        return false;

    return m_values.removeAllMatching([value](auto& current) {
        return current->equals(*value);
    }) > 0;
}

bool CSSValueList::hasValue(CSSValue* val) const
{
    // FIXME: Why even take a pointer?
    if (!val)
        return false;

    for (unsigned i = 0, size = m_values.size(); i < size; ++i) {
        if (m_values[i].get().equals(*val))
            return true;
    }
    return false;
}

Ref<CSSValueList> CSSValueList::copy()
{
    RefPtr<CSSValueList> newList;
    switch (separator()) {
    case SpaceSeparator:
        newList = createSpaceSeparated();
        break;
    case CommaSeparator:
        newList = createCommaSeparated();
        break;
    case SlashSeparator:
        newList = createSlashSeparated();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    for (auto& value : m_values)
        newList->append(value.get());
    return newList.releaseNonNull();
}

String CSSValueList::customCSSText() const
{
    auto prefix = ""_s;
    auto separator = separatorCSSText();
    StringBuilder result;
    for (auto& value : m_values)
        result.append(std::exchange(prefix, separator), value.get().cssText());
    return result.toString();
}

bool CSSValueList::equals(const CSSValueList& other) const
{
    if (separator() != other.separator())
        return false;

    if (m_values.size() != other.m_values.size())
        return false;

    for (unsigned i = 0, size = m_values.size(); i < size; ++i) {
        if (!m_values[i].get().equals(other.m_values[i]))
            return false;
    }
    return true;
}

bool CSSValueList::equals(const CSSValue& other) const
{
    if (m_values.size() != 1)
        return false;

    return m_values[0].get().equals(other);
}

bool CSSValueList::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto& value : m_values) {
        if (value.get().traverseSubresources(handler))
            return true;
    }
    return false;
}

} // namespace WebCore
