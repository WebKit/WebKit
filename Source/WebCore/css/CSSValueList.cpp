/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010, 2013 Apple Inc. All rights reserved.
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

#include "CSSParserValues.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSValueList::CSSValueList(ClassType classType, ValueListSeparator listSeparator)
    : CSSValue(classType)
{
    m_valueListSeparator = listSeparator;
}

CSSValueList::CSSValueList(ValueListSeparator listSeparator)
    : CSSValue(ValueListClass)
{
    m_valueListSeparator = listSeparator;
}

CSSValueList::CSSValueList(CSSParserValueList& parserValues)
    : CSSValue(ValueListClass)
{
    m_valueListSeparator = SpaceSeparator;
    m_values.reserveInitialCapacity(parserValues.size());
    for (unsigned i = 0, size = parserValues.size(); i < size; ++i) {
        RefPtr<CSSValue> value = parserValues.valueAt(i)->createCSSValue();
        ASSERT(value);
        m_values.uncheckedAppend(value.releaseNonNull());
    }
}

bool CSSValueList::removeAll(CSSValue* val)
{
    // FIXME: Why even take a pointer?
    if (!val)
        return false;

    bool found = false;
    for (unsigned i = 0; i < m_values.size(); ++i) {
        if (m_values[i].get().equals(*val)) {
            m_values.remove(i);
            found = true;
        }
    }

    return found;
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

PassRefPtr<CSSValueList> CSSValueList::copy()
{
    RefPtr<CSSValueList> newList;
    switch (m_valueListSeparator) {
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
    for (unsigned i = 0, size = m_values.size(); i < size; ++i)
        newList->append(m_values[i].get());
    return newList.release();
}

String CSSValueList::customCSSText() const
{
    StringBuilder result;
    String separator;
    switch (m_valueListSeparator) {
    case SpaceSeparator:
        separator = ASCIILiteral(" ");
        break;
    case CommaSeparator:
        separator = ASCIILiteral(", ");
        break;
    case SlashSeparator:
        separator = ASCIILiteral(" / ");
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    for (unsigned i = 0, size = m_values.size(); i < size; i++) {
        if (!result.isEmpty())
            result.append(separator);
        result.append(m_values[i].get().cssText());
    }

    return result.toString();
}

bool CSSValueList::equals(const CSSValueList& other) const
{
    if (m_valueListSeparator != other.m_valueListSeparator)
        return false;

    if (m_values.size() != other.m_values.size())
        return false;

    for (unsigned i = 0, size = m_values.size(); i < size; ++i) {
        if (!m_values[i].get().equals(other.m_values[i].get()))
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

void CSSValueList::addSubresourceStyleURLs(ListHashSet<URL>& urls, const StyleSheetContents* styleSheet) const
{
    for (unsigned i = 0, size = m_values.size(); i < size; ++i)
        m_values[i].get().addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValueList::hasFailedOrCanceledSubresources() const
{
    for (unsigned i = 0; i < m_values.size(); ++i) {
        if (m_values[i].get().hasFailedOrCanceledSubresources())
            return true;
    }
    return false;
}

CSSValueList::CSSValueList(const CSSValueList& cloneFrom)
    : CSSValue(cloneFrom.classType(), /* isCSSOMSafe */ true)
{
    m_valueListSeparator = cloneFrom.m_valueListSeparator;
    m_values.reserveInitialCapacity(cloneFrom.m_values.size());
    for (unsigned i = 0, size = cloneFrom.m_values.size(); i < size; ++i)
        m_values.uncheckedAppend(*cloneFrom.m_values[i]->cloneForCSSOM());
}

PassRefPtr<CSSValueList> CSSValueList::cloneForCSSOM() const
{
    return adoptRef(new CSSValueList(*this));
}

} // namespace WebCore
