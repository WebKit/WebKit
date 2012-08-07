/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "CSSStyleRule.h"

#include "CSSParser.h"
#include "CSSSelector.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "MemoryInstrumentation.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StylePropertySet.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

typedef HashMap<const CSSStyleRule*, String> SelectorTextCache;
static SelectorTextCache& selectorTextCache()
{
    DEFINE_STATIC_LOCAL(SelectorTextCache, cache, ());
    return cache;
}

CSSStyleRule::CSSStyleRule(StyleRule* styleRule, CSSStyleSheet* parent)
    : CSSRule(parent, CSSRule::STYLE_RULE)
    , m_styleRule(styleRule)
{
}

CSSStyleRule::~CSSStyleRule()
{
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->clearParentRule();

    if (hasCachedSelectorText()) {
        selectorTextCache().remove(this);
        setHasCachedSelectorText(false);
    }
}

CSSStyleDeclaration* CSSStyleRule::style() const
{
    if (!m_propertiesCSSOMWrapper) {
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_styleRule->mutableProperties(), const_cast<CSSStyleRule*>(this));
    }
    return m_propertiesCSSOMWrapper.get();
}

String CSSStyleRule::generateSelectorText() const
{
    StringBuilder builder;
    for (CSSSelector* s = m_styleRule->selectorList().first(); s; s = CSSSelectorList::next(s)) {
        if (s != m_styleRule->selectorList().first())
            builder.append(", ");
        builder.append(s->selectorText());
    }
    return builder.toString();
}

String CSSStyleRule::selectorText() const
{
    if (hasCachedSelectorText()) {
        ASSERT(selectorTextCache().contains(this));
        return selectorTextCache().get(this);
    }

    ASSERT(!selectorTextCache().contains(this));
    String text = generateSelectorText();
    selectorTextCache().set(this, text);
    setHasCachedSelectorText(true);
    return text;
}

void CSSStyleRule::setSelectorText(const String& selectorText)
{
    CSSParser p(parserContext());
    CSSSelectorList selectorList;
    p.parseSelector(selectorText, selectorList);
    if (!selectorList.first())
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    String oldSelectorText = this->selectorText();
    m_styleRule->wrapperAdoptSelectorList(selectorList);

    if (hasCachedSelectorText()) {
        ASSERT(selectorTextCache().contains(this));
        selectorTextCache().set(this, generateSelectorText());
    }
}

String CSSStyleRule::cssText() const
{
    String result = selectorText();

    result += " { ";
    result += m_styleRule->properties()->asText();
    result += "}";

    return result;
}

void CSSStyleRule::reattach(StyleRule* rule)
{
    m_styleRule = rule;
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_styleRule->mutableProperties());
}

void CSSStyleRule::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    CSSRule::reportBaseClassMemoryUsage(memoryObjectInfo);
    info.addInstrumentedMember(m_styleRule);
    info.addInstrumentedMember(m_propertiesCSSOMWrapper);
}

} // namespace WebCore
