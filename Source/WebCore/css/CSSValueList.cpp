/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
#include "PlatformString.h"
#include <wtf/PassOwnPtr.h>
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

CSSValueList::CSSValueList(CSSParserValueList* list)
    : CSSValue(ValueListClass)
{
    m_valueListSeparator = SpaceSeparator;
    if (list) {
        size_t size = list->size();
        for (unsigned i = 0; i < size; ++i)
            append(list->valueAt(i)->createCSSValue());
    }
}

void CSSValueList::append(PassRefPtr<CSSValue> val)
{
    m_values.append(val);
}

void CSSValueList::prepend(PassRefPtr<CSSValue> val)
{
    m_values.prepend(val);
}

bool CSSValueList::removeAll(CSSValue* val)
{
    bool found = false;
    // FIXME: we should be implementing operator== to CSSValue and its derived classes
    // to make comparison more flexible and fast.
    for (size_t index = 0; index < m_values.size(); index++) {
        if (m_values.at(index)->cssText() == val->cssText()) {
            m_values.remove(index);
            found = true;
        }
    }

    return found;
}

bool CSSValueList::hasValue(CSSValue* val) const
{
    // FIXME: we should be implementing operator== to CSSValue and its derived classes
    // to make comparison more flexible and fast.
    for (size_t index = 0; index < m_values.size(); index++) {
        if (m_values.at(index)->cssText() == val->cssText())
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
    for (size_t index = 0; index < m_values.size(); index++)
        newList->append(m_values[index]);
    return newList.release();
}

String CSSValueList::customCssText() const
{
    StringBuilder result;
    String separator;
    switch (m_valueListSeparator) {
    case SpaceSeparator:
        separator = " ";
        break;
    case CommaSeparator:
        separator = ", ";
        break;
    case SlashSeparator:
        separator = " / ";
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    unsigned size = m_values.size();
    for (unsigned i = 0; i < size; i++) {
        if (!result.isEmpty())
            result.append(separator);
        result.append(m_values[i]->cssText());
    }

    return result.toString();
}

void CSSValueList::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const StyleSheetInternal* styleSheet)
{
    size_t size = m_values.size();
    for (size_t i = 0; i < size; ++i)
        m_values[i]->addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValueList::hasFailedOrCanceledSubresources() const
{
    for (unsigned i = 0; i < m_values.size(); ++i) {
        if (m_values[i]->hasFailedOrCanceledSubresources())
            return true;
    }
    return false;
}

CSSValueList::CSSValueList(const CSSValueList& cloneFrom)
    : CSSValue(cloneFrom.classType(), /* isCSSOMSafe */ true)
{
    m_valueListSeparator = cloneFrom.m_valueListSeparator;
    m_values.resize(cloneFrom.m_values.size());
    for (unsigned i = 0; i < m_values.size(); ++i)
        m_values[i] = cloneFrom.m_values[i]->cloneForCSSOM();
}

PassRefPtr<CSSValueList> CSSValueList::cloneForCSSOM() const
{
    return adoptRef(new CSSValueList(*this));
}

} // namespace WebCore
