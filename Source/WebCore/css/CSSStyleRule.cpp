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

#include "CSSGroupingRule.h"
#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleSheet.h"
#include "DeclaredStylePropertyMap.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "RuleSet.h"
#include "StyleProperties.h"
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

CSSStyleRule::CSSStyleRule(StyleRule& styleRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_styleRule(styleRule)
    , m_styleMap(DeclaredStylePropertyMap::create(*this))
    , m_childRuleCSSOMWrappers(0)
{
}

CSSStyleRule::CSSStyleRule(StyleRuleWithNesting& styleRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_styleRule(styleRule)
    , m_styleMap(DeclaredStylePropertyMap::create(*this))
    , m_childRuleCSSOMWrappers(styleRule.nestedRules().size())
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

String CSSStyleRule::generateSelectorText() const
{
    if (m_styleRule->isStyleRuleWithNesting())
        return downcast<StyleRuleWithNesting>(m_styleRule.get()).originalSelectorList().selectorsText();

    return m_styleRule->selectorList().selectorsText();
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

    CSSParser p(parserContext());
    auto isNestedContext = hasStyleRuleAncestor() ? CSSParserEnum::IsNestedContext::Yes : CSSParserEnum::IsNestedContext::No;
    auto* sheet = parentStyleSheet();
    auto selectorList = p.parseSelector(selectorText, sheet ? &sheet->contents() : nullptr, isNestedContext);
    if (!selectorList)
        return;

    // NOTE: The selector list has to fit into RuleData. <http://webkit.org/b/118369>
    if (selectorList->componentCount() > Style::RuleData::maximumSelectorComponentCount)
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    if (m_styleRule->isStyleRuleWithNesting())
        downcast<StyleRuleWithNesting>(m_styleRule).wrapperAdoptOriginalSelectorList(WTFMove(*selectorList));
    else
        m_styleRule->wrapperAdoptSelectorList(WTFMove(*selectorList));

    if (hasCachedSelectorText()) {
        selectorTextCache().remove(this);
        setHasCachedSelectorText(false);
    }
}

Vector<Ref<StyleRuleBase>> CSSStyleRule::nestedRules() const
{
    if (m_styleRule->isStyleRuleWithNesting())
        return downcast<StyleRuleWithNesting>(m_styleRule.get()).nestedRules();

    return { };
}

// https://w3c.github.io/csswg-drafts/cssom-1/#serialize-a-css-rule
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

void CSSStyleRule::cssTextForDeclsAndRules(StringBuilder&, StringBuilder& rules) const
{
    for (unsigned index = 0 ; index < nestedRules().size() ; index++)
        rules.append("\n  ", item(index)->cssText());
}

// FIXME: share all methods below with CSSGroupingRule.

void CSSStyleRule::reattach(StyleRuleBase& rule)
{
    if (rule.isStyleRuleWithNesting())
        m_styleRule = downcast<StyleRuleWithNesting>(rule);        
    else
        m_styleRule = downcast<StyleRule>(rule);
        
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_styleRule->mutableProperties());
}

ExceptionOr<unsigned> CSSStyleRule::insertRule(const String& ruleString, unsigned index)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == nestedRules().size());

    if (index > nestedRules().size())
        return Exception { IndexSizeError };

    auto* styleSheet = parentStyleSheet();
    RefPtr<StyleRuleBase> newRule = CSSParser::parseRule(parserContext(), styleSheet ? &styleSheet->contents() : nullptr, ruleString, CSSParserEnum::IsNestedContext::Yes);
    if (!newRule)
        return Exception { SyntaxError };
    // We only accepts style rule or group rule (@media,...) inside style rules.
    if (!newRule->isStyleRule() && !newRule->isGroupRule())
        return Exception { HierarchyRequestError };

    if (!m_styleRule->isStyleRuleWithNesting()) {
        // Call the parent rule (or parent stylesheet if top-level or nothing if it's an orphaned rule) to transform the current StyleRule to StyleRuleWithNesting.
        RefPtr<StyleRuleWithNesting> styleRuleWithNesting;
        if (auto parent = parentRule())
            styleRuleWithNesting = parent->prepareChildStyleRuleForNesting(m_styleRule);
        else if (auto parent = parentStyleSheet())
            styleRuleWithNesting = parent->prepareChildStyleRuleForNesting(WTFMove(m_styleRule.get()));
        else
            styleRuleWithNesting = StyleRuleWithNesting::create(WTFMove(m_styleRule.get()));
        ASSERT(styleRuleWithNesting);
        m_styleRule = *styleRuleWithNesting;
    }

    CSSStyleSheet::RuleMutationScope mutationScope(this);
    ASSERT(m_styleRule->isStyleRuleWithNesting());
    downcast<StyleRuleWithNesting>(m_styleRule).nestedRules().insert(index, newRule.releaseNonNull());
    m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());
    return index;
}

ExceptionOr<void> CSSStyleRule::deleteRule(unsigned index)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == nestedRules().size());

    if (index >= nestedRules().size()) {
        // IndexSizeError: Raised if the specified index does not correspond to a
        // rule in the media rule list.
        return Exception { IndexSizeError };
    }

    ASSERT(m_styleRule->isStyleRuleWithNesting());
    auto& rules = downcast<StyleRuleWithNesting>(m_styleRule).nestedRules();
    
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    rules.remove(index);

    if (m_childRuleCSSOMWrappers[index])
        m_childRuleCSSOMWrappers[index]->setParentRule(nullptr);
    m_childRuleCSSOMWrappers.remove(index);

    return { };
}

unsigned CSSStyleRule::length() const
{
    return nestedRules().size();
}

CSSRule* CSSStyleRule::item(unsigned index) const
{
    if (index >= length())
        return nullptr;

    ASSERT(m_childRuleCSSOMWrappers.size() == nestedRules().size());

    auto& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = nestedRules()[index]->createCSSOMWrapper(const_cast<CSSStyleRule&>(*this));

    return rule.get();
}

CSSRuleList& CSSStyleRule::cssRules() const
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = makeUniqueWithoutRefCountedCheck<LiveCSSRuleList<CSSStyleRule>>(const_cast<CSSStyleRule&>(*this));

    return *m_ruleListCSSOMWrapper;
}

} // namespace WebCore
