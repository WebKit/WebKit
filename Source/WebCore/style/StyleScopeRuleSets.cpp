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

#include "CSSPropertyParser.h"
#include "CSSSelectorParser.h"
#include "CSSStyleSheet.h"
#include "CSSViewTransitionRule.h"
#include "DeclarationOrigin.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "ExtensionStyleSheets.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "RuleSetBuilder.h"
#include "StyleDocumentScope.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <ranges>
#include <wtf/MainThread.h>
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

ScopeRuleSets::ScopeRuleSets(Resolver& styleResolver)
    : m_authorStyle(RuleSet::create())
    , m_styleResolver(styleResolver)
{
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

    RuleSetBuilder builder(*m_userAgentMediaQueryStyle, mediaQueryEvaluator, &m_styleResolver);
    SUPPRESS_UNCOUNTED_ARG builder.addRulesFromSheet(*UserAgentStyle::mediaQueryStyleSheet);
}

RuleSet* ScopeRuleSets::dynamicViewTransitionsStyle() const
{
    return m_dynamicViewTransitionsStyle.get();
}

RuleSet* ScopeRuleSets::userStyle() const
{
    if (m_usesSharedUserStyle)
        return m_styleResolver.document().styleScope().resolver().ruleSets().userStyle();
    return m_userStyle.get();
}

RuleSet* ScopeRuleSets::styleForDeclarationOrigin(DeclarationOrigin origin)
{
    switch (origin) {
    case DeclarationOrigin::Author:
        return m_authorStyle.get();

    case DeclarationOrigin::User:
        return userStyle();

    case DeclarationOrigin::UserAgent:
        return userAgentMediaQueryStyle();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void ScopeRuleSets::initializeUserStyle()
{
    CheckedRef extensionStyleSheets = protect(m_styleResolver.document())->extensionStyleSheets();
    auto& mediaQueryEvaluator = m_styleResolver.mediaQueryEvaluator();

    auto userStyle = RuleSet::create();

    if (RefPtr pageUserSheet = extensionStyleSheets->pageUserSheet()) {
        RuleSetBuilder builder(userStyle, mediaQueryEvaluator, &m_styleResolver);
        builder.addRulesFromSheet(protect(pageUserSheet->contents()));
    }

#if ENABLE(APP_BOUND_DOMAINS)
    RefPtr page = m_styleResolver.document().page();
    RefPtr localMainFrame = page ? dynamicDowncast<LocalFrame>(page->mainFrame()) : nullptr;
    if (!extensionStyleSheets->injectedUserStyleSheets().isEmpty() && page && localMainFrame && localMainFrame->loader().client().shouldEnableInAppBrowserPrivacyProtections())
        protect(m_styleResolver.document())->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user style sheet for non-app bound domain."_s);
    else {
        collectRulesFromUserStyleSheets(extensionStyleSheets->injectedUserStyleSheets(), userStyle, mediaQueryEvaluator);
        if (page && localMainFrame && !extensionStyleSheets->injectedUserStyleSheets().isEmpty())
            localMainFrame->loader().client().notifyPageOfAppBoundBehavior();
    }
#else
    collectRulesFromUserStyleSheets(extensionStyleSheets->injectedUserStyleSheets(), userStyle, mediaQueryEvaluator);
#endif
    collectRulesFromUserStyleSheets(extensionStyleSheets->documentUserStyleSheets(), userStyle, mediaQueryEvaluator);

    if (userStyle->ruleCount() > 0 || userStyle->pageRules().size() > 0)
        m_userStyle = WTF::move(userStyle);
}

void ScopeRuleSets::collectRulesFromUserStyleSheets(const Vector<Ref<CSSStyleSheet>>& userSheets, RuleSet& userStyle, const MQ::MediaQueryEvaluator& mediaQueryEvaluator)
{
    RuleSetBuilder builder(userStyle, mediaQueryEvaluator, &m_styleResolver);
    for (auto& sheet : userSheets) {
        ASSERT(sheet->contents().isUserStyleSheet());
        builder.addRulesFromSheet(protect(sheet->contents()));
    }
}

void ScopeRuleSets::resetAuthorStyle()
{
    m_isAuthorStyleDefined = true;
    m_authorStyle = RuleSet::create();
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

bool ScopeRuleSets::hasContainerQueries() const
{
    if (m_authorStyle->hasContainerQueries())
        return true;
    if (m_userStyle && m_userStyle->hasContainerQueries())
        return true;
    if (m_userAgentMediaQueryStyle && m_userAgentMediaQueryStyle->hasContainerQueries())
        return true;

    return false;
}

bool ScopeRuleSets::hasScopeRules() const
{
    if (m_authorStyle->hasScopeRules())
        return true;
    if (m_userStyle && m_userStyle->hasScopeRules())
        return true;
    if (m_userAgentMediaQueryStyle && m_userAgentMediaQueryStyle->hasScopeRules())
        return true;

    return false;
}

RefPtr<StyleRuleViewTransition> ScopeRuleSets::viewTransitionRule() const
{
    if (auto viewTransitionRule = m_authorStyle->viewTransitionRule())
        return viewTransitionRule;
    if (m_userStyle && m_userStyle->viewTransitionRule())
        return m_userStyle->viewTransitionRule();
    if (m_userAgentMediaQueryStyle && m_userAgentMediaQueryStyle->viewTransitionRule())
        return m_userAgentMediaQueryStyle->viewTransitionRule();
    return nullptr;
}

std::optional<DynamicMediaQueryEvaluationChanges> ScopeRuleSets::evaluateDynamicMediaQueryRules(const MQ::MediaQueryEvaluator& evaluator)
{
    std::optional<DynamicMediaQueryEvaluationChanges> evaluationChanges;

    auto evaluate = [&](auto* ruleSet) {
        if (!ruleSet)
            return;
        if (auto changes = ruleSet->evaluateDynamicMediaQueryRules(evaluator)) {
            if (evaluationChanges)
                evaluationChanges->append(WTF::move(*changes));
            else
                evaluationChanges = changes;
        }
    };

    evaluate(&authorStyle());
    evaluate(userStyle());
    evaluate(userAgentMediaQueryStyle());

    return evaluationChanges;
}

void ScopeRuleSets::appendAuthorStyleSheets(std::span<const Ref<CSSStyleSheet>> styleSheets, MQ::MediaQueryEvaluator* mediaQueryEvaluator, InspectorCSSOMWrappers& inspectorCSSOMWrappers)
{
    RuleSetBuilder builder(*m_authorStyle, *mediaQueryEvaluator, &m_styleResolver, RuleSetBuilder::ShrinkToFit::Enable);

    RefPtr<CSSStyleSheet> previous;
    for (auto& cssSheet : styleSheets) {
        ASSERT(!cssSheet->disabled());
        // In some cases, we have many identical <style> tags. To avoid pathological behavior, we check the one-previous <style> and skip adding a new one when
        // the content is exact same to the previous one.
        if (previous) {
            if (&previous->contents() == &cssSheet->contents() && previous->mediaQueries().isEmpty() && cssSheet->mediaQueries().isEmpty()) {
                inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet);
                continue;
            }
        }

        builder.addRulesFromSheet(protect(cssSheet->contents()), cssSheet->mediaQueries());
        inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet);
        previous = cssSheet.ptr();
    }

    collectFeatures();
}

void ScopeRuleSets::collectFeatures() const
{
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(!m_isInvalidatingStyleWithRuleSets);

    m_features.clear();
    if (UserAgentStyle::defaultStyle)
        m_features.add(UserAgentStyle::defaultStyle->features());
    m_defaultStyleVersionOnFeatureCollection = UserAgentStyle::defaultStyleVersion;

    if (RefPtr userAgentMediaQueryStyle = this->userAgentMediaQueryStyle())
        m_features.add(userAgentMediaQueryStyle->features());

    if (m_authorStyle)
        m_features.add(m_authorStyle->features());
    if (RefPtr userStyle = this->userStyle())
        m_features.add(userStyle->features());

    m_idInvalidationRuleSets.clear();
    m_classInvalidationRuleSets.clear();
    m_attributeInvalidationRuleSets.clear();
    m_pseudoClassInvalidationRuleSets.clear();
    m_hasPseudoClassInvalidationRuleSets.clear();

    m_customPropertyNamesInStyleContainerQueries = std::nullopt;

    m_cachedSelectorsForStyleAttribute = std::nullopt;

    m_features.shrinkToFit();
}

// Classifies a :has() argument for sibling-combinator invalidation. Recurses into logical
// :is()/:where()/:not()/:has(); other pseudo-classes are leaf "non-logical" pseudos.
struct HasArgumentSiblingInfo {
    bool hasSiblingCombinator { false }; // + or ~
    bool hasPositionalPseudo { false }; // :nth-child(), :first-child, ... (sibling-relative)
    bool hasNonLogicalPseudo { false }; // any non-logical pseudo-class (positional, stateful, etc.)

    OptionSet<HasArgumentProperty> properties() const
    {
        OptionSet<HasArgumentProperty> result;
        if (hasSiblingCombinator || hasPositionalPseudo)
            result.add(HasArgumentProperty::OrderSensitive);
        if (hasSiblingCombinator && !hasNonLogicalPseudo)
            result.add(HasArgumentProperty::StructuralSibling);
        return result;
    }
};

static void scanHasArgument(const CSSSelector& complexSelector, HasArgumentSiblingInfo& info)
{
    for (const CSSSelector* simpleSelector = &complexSelector; simpleSelector; simpleSelector = simpleSelector->precedingInComplexSelector()) {
        auto relation = simpleSelector->relation();
        if (relation == CSSSelector::Relation::DirectAdjacent || relation == CSSSelector::Relation::IndirectAdjacent)
            info.hasSiblingCombinator = true;
        if (simpleSelector->match() == CSSSelector::Match::PseudoClass && !isLogicalCombinationPseudoClass(simpleSelector->pseudoClass())) {
            info.hasNonLogicalPseudo = true;
            if (pseudoClassIsRelativeToSiblings(simpleSelector->pseudoClass()))
                info.hasPositionalPseudo = true;
        }
        if (const CSSSelectorList* selectorList = simpleSelector->selectorList()) {
            for (const auto& subSelector : *selectorList)
                scanHasArgument(subSelector, info);
        }
    }
}

static OptionSet<HasArgumentProperty> hasArgumentProperties(const CSSSelectorList& argument)
{
    HasArgumentSiblingInfo info;
    for (const auto& complexSelector : argument)
        scanHasArgument(complexSelector, info);
    return info.properties();
}

template<typename KeyType, typename Hash, typename HashTraits>
static Vector<InvalidationRuleSet>* ensureInvalidationRuleSets(const KeyType& key, HashMap<KeyType, std::unique_ptr<Vector<InvalidationRuleSet>>, Hash, HashTraits>& ruleSetMap, const HashMap<KeyType, std::unique_ptr<RuleFeatureVector>, Hash, HashTraits>& ruleFeatures)
{
    return ruleSetMap.ensure(key, [&] () -> std::unique_ptr<Vector<InvalidationRuleSet>> {
        auto* features = ruleFeatures.get(key);
        if (!features)
            return nullptr;

        struct RuleSetKey {
            MatchElement matchElement;
            IsNegation isNegation;
            const CSSSelectorList* invalidationSelector { nullptr };
            const CSSSelectorList* scopeSelector { nullptr };

            unsigned hash() const
            {
                Hasher hasher;
                add(hasher, matchElement.relation, matchElement.hasRelation, isNegation);
                if (invalidationSelector)
                    add(hasher, *invalidationSelector);
                if (scopeSelector)
                    add(hasher, *scopeSelector);
                return hasher.hash();
            }
            bool operator==(const RuleSetKey& other) const
            {
                return matchElement == other.matchElement
                    && isNegation == other.isNegation
                    && arePointingToEqualData(invalidationSelector, other.invalidationSelector)
                    && arePointingToEqualData(scopeSelector, other.scopeSelector);
            }
        };

        HashMap<GenericHashKey<RuleSetKey>, RefPtr<RuleSet>> ruleSetMap;

        for (auto& feature : *features) {
            auto key = GenericHashKey<RuleSetKey> { { feature.matchElement, feature.isNegation, &feature.invalidationSelector, &feature.scopeSelector } };

            auto& ruleSet = ruleSetMap.ensure(key, [] {
                return RuleSet::create();
            }).iterator->value;

            ruleSet->addRule(protect(*feature.styleRule), feature.selectorIndex, feature.selectorListIndex);
        }

        return makeUnique<Vector<InvalidationRuleSet>>(WTF::map(ruleSetMap, [](auto& entry) {
            auto& key = entry.key.key();
            entry.value->shrinkToFit();
            auto invalidationSelector = [&] {
                if (!key.invalidationSelector->isEmpty() && !key.scopeSelector->isEmpty())
                    return CSSSelectorParser::makeHasArgumentWithScope(key.invalidationSelector->first(), key.scopeSelector->first());
                if (!key.invalidationSelector->isEmpty())
                    return CSSSelectorList { *key.invalidationSelector };
                return CSSSelectorList { };
            }();
            // Null scopeSelector means scope-breaking; otherwise wrap a copy so it outlives this cache.
            RefPtr<const RefCountedCSSSelectorList> scopeSelector;
            if (!key.scopeSelector->isEmpty())
                scopeSelector = RefCountedCSSSelectorList::create(CSSSelectorList { *key.scopeSelector });
            return InvalidationRuleSet {
                WTF::move(entry.value),
                WTF::move(invalidationSelector),
                key.matchElement,
                key.isNegation,
                hasArgumentProperties(*key.invalidationSelector),
                WTF::move(scopeSelector)
            };
        }));
    }).iterator->value.get();
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::idInvalidationRuleSets(const AtomString& id) const
{
    return ensureInvalidationRuleSets(id, m_idInvalidationRuleSets, m_features.idRules);
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::classInvalidationRuleSets(const AtomString& className) const
{
    return ensureInvalidationRuleSets(className, m_classInvalidationRuleSets, m_features.classRules);
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::attributeInvalidationRuleSets(const AtomString& attributeName) const
{
    return ensureInvalidationRuleSets(attributeName, m_attributeInvalidationRuleSets, m_features.attributeRules);
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::pseudoClassInvalidationRuleSets(const PseudoClassInvalidationKey& pseudoClassKey) const
{
    return ensureInvalidationRuleSets(pseudoClassKey, m_pseudoClassInvalidationRuleSets, m_features.pseudoClassRules);
}

const Vector<InvalidationRuleSet>* ScopeRuleSets::hasPseudoClassInvalidationRuleSets(const PseudoClassInvalidationKey& key) const
{
    return ensureInvalidationRuleSets(key, m_hasPseudoClassInvalidationRuleSets, m_features.hasPseudoClassRules);
}

const HashSet<AtomString>& ScopeRuleSets::customPropertyNamesInStyleContainerQueries() const
{
    if (!m_customPropertyNamesInStyleContainerQueries) {
        HashSet<AtomString> propertyNames;

        auto collectPropertyNames = [&](auto* ruleSet) {
            if (!ruleSet)
                return;
            for (auto query : ruleSet->containerQueryRules()) {
                traverseFeatures(query->containerQuery().condition, [&](auto& containerFeature) {
                    if (isCustomPropertyName(containerFeature.name))
                        propertyNames.add(containerFeature.name);
                });
            }
        };

        collectPropertyNames(&authorStyle());
        collectPropertyNames(userStyle());

        m_customPropertyNamesInStyleContainerQueries = propertyNames;
    }
    return *m_customPropertyNamesInStyleContainerQueries;
}

SelectorsForStyleAttribute ScopeRuleSets::selectorsForStyleAttribute() const
{
    auto compute = [&] {
        auto* ruleSets = attributeInvalidationRuleSets(HTMLNames::styleAttr->localName());
        if (!ruleSets)
            return SelectorsForStyleAttribute::None;
        for (auto& ruleSet : *ruleSets) {
            if (ruleSet.matchElement.relation != MatchElement::Relation::Subject)
                return SelectorsForStyleAttribute::NonSubjectPosition;
        }
        return SelectorsForStyleAttribute::SubjectPositionOnly;
    };

    if (!m_cachedSelectorsForStyleAttribute)
        m_cachedSelectorsForStyleAttribute = compute();

    return *m_cachedSelectorsForStyleAttribute;
}

bool ScopeRuleSets::hasMatchingUserOrAuthorStyle(NOESCAPE const WTF::Function<bool(RuleSet&)>& predicate)
{
    if (m_authorStyle && predicate(protect(*m_authorStyle)))
        return true;

    if (RefPtr userStyle = this->userStyle(); userStyle && predicate(*userStyle))
        return true;

    return false;
}

} // namespace Style
} // namespace WebCore
