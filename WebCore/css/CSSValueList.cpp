/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

namespace WebCore {

CSSValueList::CSSValueList(bool isSpaceSeparated)
    : m_isSpaceSeparated(isSpaceSeparated)
{
}

CSSValueList::CSSValueList(CSSParserValueList* list)
    : m_isSpaceSeparated(true)
{
    if (list) {
        unsigned s = list->size();
        for (unsigned i = 0; i < s; ++i) {
            CSSParserValue* v = list->valueAt(i);
            append(v->createCSSValue());
        }
    }
}

CSSValueList::~CSSValueList()
{
}

CSSValue* CSSValueList::item(unsigned index)
{
    if (index >= m_values.size())
        return 0;
    return m_values[index].get();
}

unsigned short CSSValueList::cssValueType() const
{
    return CSS_VALUE_LIST;
}

void CSSValueList::append(PassRefPtr<CSSValue> val)
{
    m_values.append(val);
}

void CSSValueList::prepend(PassRefPtr<CSSValue> val)
{
    m_values.prepend(val);
}

String CSSValueList::cssText() const
{
    String result = "";

    unsigned size = m_values.size();
    for (unsigned i = 0; i < size; i++) {
        if (!result.isEmpty()) {
            if (m_isSpaceSeparated)
                result += " ";
            else
                result += ", ";
        }
        result += m_values[i]->cssText();
    }

    return result;
}

CSSParserValueList* CSSValueList::createParserValueList() const
{
    unsigned s = m_values.size();
    if (!s)
        return 0;
    CSSParserValueList* result = new CSSParserValueList;
    for (unsigned i = 0; i < s; ++i)
        result->addValue(m_values[i]->parserValue());
    return result;
}

void CSSValueList::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const CSSStyleSheet* styleSheet)
{
    size_t size = m_values.size();
    for (size_t i = 0; i < size; ++i)
        m_values[i]->addSubresourceStyleURLs(urls, styleSheet);
}

} // namespace WebCore
