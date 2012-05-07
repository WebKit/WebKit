/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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
#include "CSSPageRule.h"

#include "CSSParser.h"
#include "CSSSelector.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StylePropertySet.h"
#include "StyleRule.h"
#include <wtf/Vector.h>

namespace WebCore {

CSSPageRule::CSSPageRule(StyleRulePage* pageRule, CSSStyleSheet* parent)
    : CSSRule(parent, CSSRule::PAGE_RULE)
    , m_pageRule(pageRule)
{
}

CSSPageRule::~CSSPageRule()
{
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->clearParentRule();
}

CSSStyleDeclaration* CSSPageRule::style() const
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_pageRule->properties(), const_cast<CSSPageRule*>(this));
    return m_propertiesCSSOMWrapper.get();
}

String CSSPageRule::selectorText() const
{
    String text = "@page";
    const CSSSelector* selector = m_pageRule->selector();
    if (selector) {
        String pageSpecification = selector->selectorText();
        if (!pageSpecification.isEmpty() && pageSpecification != starAtom)
            text += " " + pageSpecification;
    }
    return text;
}

void CSSPageRule::setSelectorText(const String& selectorText)
{
    CSSParser parser(parserContext());
    CSSSelectorList selectorList;
    parser.parseSelector(selectorText, selectorList);
    if (!selectorList.first())
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    String oldSelectorText = this->selectorText();
    m_pageRule->wrapperAdoptSelectorList(selectorList);
}

String CSSPageRule::cssText() const
{
    String result = selectorText();
    result += " { ";
    result += m_pageRule->properties()->asText();
    result += "}";
    return result;
}

void CSSPageRule::reattach(StyleRulePage* rule)
{
    ASSERT(rule);
    m_pageRule = rule;
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_pageRule->properties());
}

} // namespace WebCore
