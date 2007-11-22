/**
 * This file is part of the DOM implementation for KDE.
 *
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
#include "StyleList.h"

namespace WebCore {

CSSRuleList::CSSRuleList()
{
}

CSSRuleList::CSSRuleList(StyleList* list, bool omitCharsetRules)
{
    if (list) {
        unsigned len = list->length();
        for (unsigned i = 0; i < len; ++i) {
            StyleBase* style = list->item(i);
            if (style->isRule() && !(omitCharsetRules && style->isCharsetRule()))
                append(static_cast<CSSRule*>(style));
        }
    }
}

CSSRuleList::~CSSRuleList()
{
    CSSRule* rule;
    while (!m_lstCSSRules.isEmpty() && (rule = m_lstCSSRules.take(0)))
        rule->deref();
}

void CSSRuleList::deleteRule(unsigned index)
{
    CSSRule* rule = m_lstCSSRules.take(index);
    if (rule)
        rule->deref();
    // FIXME: Throw INDEX_SIZE_ERR exception here if !rule
}

void CSSRuleList::append(CSSRule* rule)
{
    insertRule(rule, m_lstCSSRules.count()) ;
}

unsigned CSSRuleList::insertRule(CSSRule* rule, unsigned index)
{
    if (rule && m_lstCSSRules.insert(index, rule)) {
        rule->ref();
        return index;
    }

    // FIXME: Should throw INDEX_SIZE_ERR exception instead!
    return 0;
}

} // namespace WebCore
