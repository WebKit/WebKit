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
#include "StyleScopeRuleSets.h"

#include "CSSStyleSheet.h"
#include "ExtensionStyleSheets.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"

namespace WebCore {
namespace Style {

ScopeRuleSets::ScopeRuleSets(Resolver& styleResolver)
    : m_styleResolver(styleResolver)
{
    m_authorStyle = RuleSet::create();
    m_authorStyle->disableAutoShrinkToFit();
}

ScopeRuleSets::~ScopeRuleSets()
{
    RELEASE_ASSERT(!m_isInvalidatingStyleWithRuleSets);
}

RuleSet* ScopeRuleSets::userAgentMediaQueryStyle() const
{
    updateUserAgentMediaQueryStyleIfNeeded();
    return m_userAgentMediaQueryStyle.get();
}

void ScopeRuleSets::updateUserAgentMediaQueryStyleIfNeeded() const
{
    if (!UserAgentStyle::mediaQueryStyleSheet)
        return;

    auto ruleCount = UserAgentStyle::mediaQueryStyleSheet->ruleCount();
    if (m_userAgentMediaQueryStyle && ruleCount == m_userAgentMediaQueryRuleCountOnUpdate)
        return;
    m_userAgentMediaQueryRuleCountOnUpdate = ruleCount;

    // Media queries on user agent sheet need to evaluated in document context. They behave like author sheets in this respect.
    auto& mediaQueryEvaluator = m_styleResolver.mediaQueryEvaluator();
    m_userAgentMediaQueryStyle = RuleSet::create();
    
    m_userAgentMediaQueryStyle->addRulesFromSheet(*UserAgentStyle::mediaQueryStyleSheet, nullptr, mediaQueryEvaluator, m_styleResolver);
}

RuleSet* ScopeRuleSets::userStyle() const
{
    if (m_usesSharedUserStyle)
        return m_styleResolver.document().styleScope().resolver().ruleSets().userStyle();
    return m_userStyle.get();
}

void ScopeRuleSets::initializeUserStyle()
{
    auto& extensionStyleSheets = m_styleResolver.document().extensionStyleSheets();
    auto& mediaQueryEvaluator = m_styleResolver.mediaQueryEvaluator();
    auto tempUserStyle = RuleSet::create();
    if (CSSStyleSheet* pageUserSheet = extensionStyleSheets.pageUserSheet())
        tempUserStyle->addRulesFromSheet(pageUserSheet->contents(), nullptr, mediaQueryEvaluator, m_styleResolver);
    auto* page = m_styleResolver.document().page();
    if (!extensionStyleSheets.injectedUserStyleSheets().isEmpty() && page && page->mainFrame().loader().client().shouldEnableInAppBrowserPrivacyProtections())
        m_styleResolver.document().addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user style sheet for non-app bound domain."_s);
    else {
        collectRulesFromUserStyleSheets(extensionStyleSheets.injectedUserStyleSheets(), tempUserStyle.get(), mediaQueryEvaluator);
        if (page && !extensionStyleSheets.injectedUserStyleSheets().isEmpty())
            page->mainFrame().loader().client().notifyPageOfAppBoundBehavior();
    }
    collectRulesFromUserStyleSheets(extensionStyleSheets.documentUserStyleSheets(), tempUserStyle.get(), mediaQueryEvaluator);
    if (tempUserStyle->ruleCount() > 0 || tempUserStyle->pageRules().size() > 0)
        m_userStyle = WTFMove(tempUserStyle);
}

void ScopeRuleSets::collectRulesFromUserStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& userSheets, RuleSet& userStyle, const MediaQueryEvaluator& medium)
{
    for (unsigned i = 0; i < userSheets.size(); ++i) {
        ASSERT(userSheets[i]->contents().isUserStyleSheet());
        userStyle.addRulesFromSheet(userSheets[i]->contents(), nullptr, medium, m_styleResolver);
    }
}

static RefPtr<RuleSet> makeRuleSet(const Vector<RuleFeature>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    auto ruleSet = RuleSet::create();
    for (size_t i = 0; i < size; ++i)
        ruleSet->addRule(*rules[i].styleRule, rules[i].selectorIndex, rules[i].selectorListIndex);
    ruleSet->shrinkToFit();
    return ruleSet;
}

void ScopeRuleSets::resetAuthorStyle()
{
    m_isAuthorStyleDefined = true;
    m_authorStyle = RuleSet::create();
    m_authorStyle->disableAutoShrinkToFit();
}

void ScopeRuleSets::resetUserAgentMediaQueryStyle()
{
    m_userAgentMediaQueryStyle = nullptr;
}

bool ScopeRuleSets::hasViewportDependentMediaQueries() const
{
    if (m_authorStyle->hasViewportDependentMediaQueries())
        return true;
    if (m_userStyle && m_userStyle->hasViewportDependentMediaQueries())
        return true;
    if (m_userAgentMediaQueryStyle && m_userAgentMediaQueryStyle->hasViewportDependentMediaQueries())
        return true;

    return false;
}

Optional<DynamicMediaQueryEvaluationChanges> ScopeRuleSets::evaluateDynamicMediaQueryRules(const MediaQueryEvaluator& evaluator)
{
    Optional<DynamicMediaQueryEvaluationChanges> evaluationChanges;

    auto evaluate = [&](auto* ruleSet) {
        if (!ruleSet)
            return;
        if (auto changes = ruleSet->evaluateDynamicMediaQueryRules(evaluator)) {
            if (evaluationChanges)
                evaluationChanges->append(WTFMove(*changes));
            else
                evaluationChanges = changes;
        }
    };

    evaluate(&authorStyle());
    evaluate(userStyle());
    evaluate(userAgentMediaQueryStyle());

    return evaluationChanges;
}

void ScopeRuleSets::appendAuthorStyleSheets(const Vector<RefPtr<CSSStyleSheet>>& styleSheets, MediaQueryEvaluator* medium, InspectorCSSOMWrappers& inspectorCSSOMWrappers)
{
    for (auto& cssSheet : styleSheets) {
        ASSERT(!cssSheet->disabled());
        m_authorStyle->addRulesFromSheet(cssSheet->contents(), cssSheet->mediaQueries(), *medium, m_styleResolver);
        inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet.get());
    }

    m_authorStyle->shrinkToFit();
    collectFeatures();
}

void ScopeRuleSets::collectFeatures() const
{
    RELEASE_ASSERT(!m_isInvalidatingStyleWithRuleSets);

    m_features.clear();
    // Collect all ids and rules using sibling selectors (:first-child and similar)
    // in the current set of stylesheets. Style sharing code uses this information to reject
    // sharing candidates.
    if (UserAgentStyle::defaultStyle)
        m_features.add(UserAgentStyle::defaultStyle->features());
    m_defaultStyleVersionOnFeatureCollection = UserAgentStyle::defaultStyleVersion;

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
    m_pseudoClassInvalidationRuleSets.clear();

    m_cachedHasComplexSelectorsForStyleAttribute = WTF::nullopt;

    m_features.shrinkToFit();
}

template<typename KeyType, typename RuleFeatureType, typename Hash, typename HashTraits>
static Vector<InvalidationRuleSet>* ensureInvalidationRuleSets(const KeyType& key, HashMap<KeyType, std::unique_ptr<Vector<InvalidationRuleSet>>, Hash, HashTraits>& ruleSetMap, const HashMap<KeyType, std::unique_ptr<Vector<RuleFeatureType>>, Hash, HashTraits>& ruleFeatures)
{
    return ruleSetMap.ensure(key, [&] () -> std::unique_ptr<Vector<InvalidationRuleSet>> {
        auto* features = ruleFeatures.get(key);
        if (!features)
            return nullptr;

        std::array<RefPtr<RuleSet>, matchElementCount> matchElementArray;
        std::array<Vector<const CSSSelector*>, matchElementCount> invalidationSelectorArray;
        for (auto& feature : *features) {
            auto arrayIndex = static_cast<unsigned>(*feature.matchElement);
            auto& ruleSet = matchElementArray[arrayIndex];
            if (!ruleSet)
                ruleSet = RuleSet::create();
            ruleSet->addRule(*feature.styleRule, feature.selectorIndex, feature.selectorListIndex);
            if constexpr (std::is_same<RuleFeatureType, RuleFeatureWithInvalidationSelector>::value) {
                if (feature.invalidationSelector)
                    invalidationSelectorArray[arrayIndex].append(feature.invalidationSelector);
            }
        }
        auto invalidationRuleSets = makeUnique<Vector<InvalidationRuleSet>>();
        for (unsigned i = 0; i < matchElementArray.size(); ++i) {
            if (matchElementArray[i])
                invalidationRuleSets->append({ static_cast<MatchElement>(i), *matchElementArray[i], WTFMove(invalidationSelectorArray[i]) });
        }
        return invalidationRuleSets;
    }).iterator->value.get();
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::classInvalidationRuleSets(const AtomString& className) const
{
    return ensureInvalidationRuleSets(className, m_classInvalidationRuleSets, m_features.classRules);
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::attributeInvalidationRuleSets(const AtomString& attributeName) const
{
    return ensureInvalidationRuleSets(attributeName, m_attributeInvalidationRuleSets, m_features.attributeRules);
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::pseudoClassInvalidationRuleSets(CSSSelector::PseudoClassType pseudoClass) const
{
    return ensureInvalidationRuleSets(pseudoClass, m_pseudoClassInvalidationRuleSets, m_features.pseudoClassRules);
}

bool ScopeRuleSets::hasComplexSelectorsForStyleAttribute() const
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

} // namespace Style
} // namespace WebCore
