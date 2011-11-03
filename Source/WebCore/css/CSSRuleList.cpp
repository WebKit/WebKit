/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006 Apple Computer, Inc.
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
#include "CSSRuleList.h"

#include "CSSRule.h"
#include "CSSStyleSheet.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSRuleList::CSSRuleList()
{
}

CSSRuleList::CSSRuleList(CSSStyleSheet* styleSheet, bool omitCharsetRules)
    : m_styleSheet(styleSheet)
{
    if (styleSheet && omitCharsetRules) {
        m_styleSheet = 0;
        for (unsigned i = 0; i < styleSheet->length(); ++i) {
            CSSRule* rule = styleSheet->item(i);
            if (!rule->isCharsetRule())
                append(static_cast<CSSRule*>(rule));
        }
    }
}

CSSRuleList::~CSSRuleList()
{
}

unsigned CSSRuleList::length() const
{
    return m_styleSheet ? m_styleSheet->length() : m_lstCSSRules.size();
}

CSSRule* CSSRuleList::item(unsigned index) const
{
    if (m_styleSheet)
        return m_styleSheet->item(index);

    if (index < m_lstCSSRules.size())
        return m_lstCSSRules[index].get();
    return 0;
}

void CSSRuleList::deleteRule(unsigned index)
{
    ASSERT(!m_styleSheet);

    if (index >= m_lstCSSRules.size())
        return;

    m_lstCSSRules.remove(index);
}

void CSSRuleList::append(CSSRule* rule)
{
    ASSERT(!m_styleSheet);

    if (!rule)
        return;

    m_lstCSSRules.append(rule);
}

unsigned CSSRuleList::insertRule(CSSRule* rule, unsigned index)
{
    ASSERT(!m_styleSheet);

    if (!rule || index > m_lstCSSRules.size())
        return 0;

    m_lstCSSRules.insert(index, rule);
    return index;
}

String CSSRuleList::rulesText() const
{
    StringBuilder result;

    for (unsigned index = 0; index < length(); ++index) {
        result.append("  ");
        result.append(item(index)->cssText());
        result.append("\n");
    }

    return result.toString();
}

} // namespace WebCore
