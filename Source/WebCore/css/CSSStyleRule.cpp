/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
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
#include "CSSRuleList.h"
#include "CSSStyleSheet.h"
#include "DeclaredStylePropertyMap.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "RuleSet.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

typedef HashMap<const CSSStyleRule*, String> SelectorTextCache;
static SelectorTextCache& selectorTextCache()
{
    static NeverDestroyed<SelectorTextCache> cache;
    return cache;
}

CSSStyleRule::CSSStyleRule(StyleRuleWithNesting& styleRule, CSSStyleSheet* parent)
    : CSSGroupingRule(styleRule, parent)
    , m_styleRule(styleRule)
    , m_styleMap(DeclaredStylePropertyMap::create(*this))
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

CSSStyleDeclaration& CSSStyleRule::style()
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_styleRule->mutableProperties(), *this);
    return *m_propertiesCSSOMWrapper;
}

StylePropertyMap& CSSStyleRule::styleMap()
{
    return m_styleMap.get();
}

StyleRule& CSSStyleRule::styleRule() const { return m_styleRule.get(); }

String CSSStyleRule::generateSelectorText() const
{
    return m_styleRule->originalSelectorList().selectorsText();
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
    // FIXME: getMatchedCSSRules can return CSSStyleRules that are missing parent stylesheet pointer while
    // referencing StyleRules that are part of stylesheet. Disallow mutations in this case.
    if (!parentStyleSheet())
        return;

    ALWAYS_LOG_WITH_STREAM(stream << "parse " << selectorText);
    CSSParser p(parserContext());
    auto* sheet = parentStyleSheet();
    auto isNestedContext = hasStyleRuleAscendant() ? CSSParserEnum::IsNestedContext::Yes : CSSParserEnum::IsNestedContext::No;
    auto selectorList = p.parseSelector(selectorText, sheet ? &sheet->contents() : nullptr, isNestedContext);
    if (!selectorList)
        return;

    // NOTE: The selector list has to fit into RuleData. <http://webkit.org/b/118369>
    if (selectorList->componentCount() > Style::RuleData::maximumSelectorComponentCount)
        return;

    ALWAYS_LOG_WITH_STREAM(stream << "add " << selectorText);
    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_styleRule->wrapperAdoptSelectorList(WTFMove(*selectorList));

    if (hasCachedSelectorText()) {
        selectorTextCache().remove(this);
        setHasCachedSelectorText(false);
    }
}

Vector<RefPtr<StyleRuleBase>> CSSStyleRule::nestedRules() const
{
    return m_styleRule->childRules();
}

String CSSStyleRule::cssText() const
{
    StringBuilder builder;
    builder.append(selectorText());
    builder.append(" {");

    auto declsString = m_styleRule->properties().asText();
    StringBuilder decls;
    StringBuilder rules;

    decls.append(declsString);
    cssTextForDeclsAndRules(decls, rules);

    if (decls.isEmpty() && rules.isEmpty()) {
        builder.append(" }");
        return builder.toString();
    }

    if (rules.isEmpty()) {
        builder.append(' ');
        builder.append(decls);
        builder.append(" }");
        return builder.toString();
    }

    if (decls.isEmpty()) {
        builder.append(rules);
        builder.append("\n}");
        return builder.toString();
    }

    builder.append("\n  ");
    builder.append(decls);
    builder.append(rules);
    builder.append("\n}");
    return builder.toString();
}

void CSSStyleRule::reattach(StyleRuleBase& rule)
{
    ASSERT(rule.isStyleRuleWithNesting());
    m_styleRule = downcast<StyleRuleWithNesting>(rule);
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_styleRule->mutableProperties());
}

} // namespace WebCore
