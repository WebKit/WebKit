/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "DocumentRuleSets.h"

#include "CSSStyleSheet.h"
#include "ExtensionStyleSheets.h"
#include "MediaQueryEvaluator.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"

namespace WebCore {

// For catching <rdar://problem/53413013>
bool DocumentRuleSets::s_isInvalidatingStyleWithRuleSets { false };

DocumentRuleSets::DocumentRuleSets(StyleResolver& styleResolver)
    : m_styleResolver(styleResolver)
{
    m_authorStyle = makeUnique<RuleSet>();
    m_authorStyle->disableAutoShrinkToFit();
}

DocumentRuleSets::~DocumentRuleSets()
{
    RELEASE_ASSERT(!s_isInvalidatingStyleWithRuleSets);
}

RuleSet* DocumentRuleSets::userAgentMediaQueryStyle() const
{
    // FIXME: We should have a separate types for document rule sets and shadow tree rule sets.
    if (m_isForShadowScope)
        return m_styleResolver.document().styleScope().resolver().ruleSets().userAgentMediaQueryStyle();

    updateUserAgentMediaQueryStyleIfNeeded();
    return m_userAgentMediaQueryStyle.get();
}

void DocumentRuleSets::updateUserAgentMediaQueryStyleIfNeeded() const
{
    if (!CSSDefaultStyleSheets::mediaQueryStyleSheet)
        return;

    auto ruleCount = CSSDefaultStyleSheets::mediaQueryStyleSheet->ruleCount();
    if (m_userAgentMediaQueryStyle && ruleCount == m_userAgentMediaQueryRuleCountOnUpdate)
        return;
    m_userAgentMediaQueryRuleCountOnUpdate = ruleCount;

#if !ASSERT_DISABLED
    bool hadViewportDependentMediaQueries = m_styleResolver.hasViewportDependentMediaQueries();
#endif

    // Media queries on user agent sheet need to evaluated in document context. They behave like author sheets in this respect.
    auto& mediaQueryEvaluator = m_styleResolver.mediaQueryEvaluator();
    m_userAgentMediaQueryStyle = makeUnique<RuleSet>();
    m_userAgentMediaQueryStyle->addRulesFromSheet(*CSSDefaultStyleSheets::mediaQueryStyleSheet, mediaQueryEvaluator, &m_styleResolver);

    // Viewport dependent queries are currently too inefficient to allow on UA sheet.
    ASSERT(!m_styleResolver.hasViewportDependentMediaQueries() || hadViewportDependentMediaQueries);
}

RuleSet* DocumentRuleSets::userStyle() const
{
    if (m_usesSharedUserStyle)
        return m_styleResolver.document().styleScope().resolver().ruleSets().userStyle();
    return m_userStyle.get();
}

void DocumentRuleSets::initializeUserStyle()
{
    auto& extensionStyleSheets = m_styleResolver.document().extensionStyleSheets();
    auto& mediaQueryEvaluator = m_styleResolver.mediaQueryEvaluator();
    auto tempUserStyle = makeUnique<RuleSet>();
    if (CSSStyleSheet* pageUserSheet = extensionStyleSheets.pageUserSheet())
        tempUserStyle->addRulesFromSheet(pageUserSheet->contents(), mediaQueryEvaluator, &m_styleResolver);
    collectRulesFromUserStyleSheets(extensionStyleSheets.injectedUserStyleSheets(), *tempUserStyle, mediaQueryEvaluator, m_styleResolver);
    collectRulesFromUserStyleSheets(extensionStyleSheets.documentUserStyleSheets(), *tempUserStyle, mediaQueryEvaluator, m_styleResolver);
    if (tempUserStyle->ruleCount() > 0 || tempUserStyle->pageRules().size() > 0)
        m_userStyle = WTFMove(tempUserStyle);
}

void DocumentRuleSets::collectRulesFromUserStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& userSheets, RuleSet& userStyle, const MediaQueryEvaluator& medium, StyleResolver& resolver)
{
    for (unsigned i = 0; i < userSheets.size(); ++i) {
        ASSERT(userSheets[i]->contents().isUserStyleSheet());
        userStyle.addRulesFromSheet(userSheets[i]->contents(), medium, &resolver);
    }
}

static std::unique_ptr<RuleSet> makeRuleSet(const Vector<RuleFeature>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    auto ruleSet = makeUnique<RuleSet>();
    for (size_t i = 0; i < size; ++i)
        ruleSet->addRule(rules[i].rule, rules[i].selectorIndex, rules[i].selectorListIndex);
    ruleSet->shrinkToFit();
    return ruleSet;
}

void DocumentRuleSets::resetAuthorStyle()
{
    m_isAuthorStyleDefined = true;
    m_authorStyle = makeUnique<RuleSet>();
    m_authorStyle->disableAutoShrinkToFit();
}

void DocumentRuleSets::resetUserAgentMediaQueryStyle()
{
    m_userAgentMediaQueryStyle = nullptr;
}

void DocumentRuleSets::appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& styleSheets, MediaQueryEvaluator* medium, InspectorCSSOMWrappers& inspectorCSSOMWrappers, StyleResolver* resolver)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    for (auto& cssSheet : styleSheets) {
        ASSERT(!cssSheet->disabled());
        if (cssSheet->mediaQueries() && !medium->evaluate(*cssSheet->mediaQueries(), resolver))
            continue;
        m_authorStyle->addRulesFromSheet(cssSheet->contents(), *medium, resolver);
        inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet.get());
    }
    m_authorStyle->shrinkToFit();
    collectFeatures();
}

void DocumentRuleSets::collectFeatures() const
{
    RELEASE_ASSERT(!s_isInvalidatingStyleWithRuleSets);

    m_features.clear();
    // Collect all ids and rules using sibling selectors (:first-child and similar)
    // in the current set of stylesheets. Style sharing code uses this information to reject
    // sharing candidates.
    if (CSSDefaultStyleSheets::defaultStyle)
        m_features.add(CSSDefaultStyleSheets::defaultStyle->features());
    m_defaultStyleVersionOnFeatureCollection = CSSDefaultStyleSheets::defaultStyleVersion;

    if (auto* userAgentMediaQueryStyle = this->userAgentMediaQueryStyle())
        m_features.add(userAgentMediaQueryStyle->features());

    if (m_authorStyle)
        m_features.add(m_authorStyle->features());
    if (auto* userStyle = this->userStyle())
        m_features.add(userStyle->features());

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);

    m_classInvalidationRuleSets.clear();
    m_attributeInvalidationRuleSets.clear();
    m_cachedHasComplexSelectorsForStyleAttribute = WTF::nullopt;

    m_features.shrinkToFit();
}

static Vector<InvalidationRuleSet>* ensureInvalidationRuleSets(const AtomString& key, HashMap<AtomString, std::unique_ptr<Vector<InvalidationRuleSet>>>& ruleSetMap, const HashMap<AtomString, std::unique_ptr<Vector<RuleFeature>>>& ruleFeatures)
{
    return ruleSetMap.ensure(key, [&] () -> std::unique_ptr<Vector<InvalidationRuleSet>> {
        auto* features = ruleFeatures.get(key);
        if (!features)
            return nullptr;

        std::array<std::unique_ptr<RuleSet>, matchElementCount> matchElementArray;
        std::array<Vector<const CSSSelector*>, matchElementCount> invalidationSelectorArray;
        for (auto& feature : *features) {
            auto arrayIndex = static_cast<unsigned>(*feature.matchElement);
            auto& ruleSet = matchElementArray[arrayIndex];
            if (!ruleSet)
                ruleSet = makeUnique<RuleSet>();
            ruleSet->addRule(feature.rule, feature.selectorIndex, feature.selectorListIndex);
            if (feature.invalidationSelector)
                invalidationSelectorArray[arrayIndex].append(feature.invalidationSelector);
        }
        auto invalidationRuleSets = makeUnique<Vector<InvalidationRuleSet>>();
        for (unsigned i = 0; i < matchElementArray.size(); ++i) {
            if (matchElementArray[i])
                invalidationRuleSets->append({ static_cast<MatchElement>(i), WTFMove(matchElementArray[i]), WTFMove(invalidationSelectorArray[i]) });
        }
        return invalidationRuleSets;
    }).iterator->value.get();
}

const Vector<InvalidationRuleSet>* DocumentRuleSets::classInvalidationRuleSets(const AtomString& className) const
{
    return ensureInvalidationRuleSets(className, m_classInvalidationRuleSets, m_features.classRules);
}

const Vector<InvalidationRuleSet>* DocumentRuleSets::attributeInvalidationRuleSets(const AtomString& attributeName) const
{
    return ensureInvalidationRuleSets(attributeName, m_attributeInvalidationRuleSets, m_features.attributeRules);
}

bool DocumentRuleSets::hasComplexSelectorsForStyleAttribute() const
{
    auto compute = [&] {
        auto* ruleSets = attributeInvalidationRuleSets(HTMLNames::styleAttr->localName());
        if (!ruleSets)
            return false;
        for (auto& ruleSet : *ruleSets) {
            if (ruleSet.matchElement != MatchElement::Subject)
                return true;
        }
        return false;
    };

    if (!m_cachedHasComplexSelectorsForStyleAttribute)
        m_cachedHasComplexSelectorsForStyleAttribute = compute();

    return *m_cachedHasComplexSelectorsForStyleAttribute;
}

} // namespace WebCore
