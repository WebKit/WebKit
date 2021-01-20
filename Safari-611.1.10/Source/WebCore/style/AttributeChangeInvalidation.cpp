/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AttributeChangeInvalidation.h"

#include "ElementIterator.h"
#include "StyleInvalidationFunctions.h"

namespace WebCore {
namespace Style {

static bool mayBeAffectedByAttributeChange(const RuleFeatureSet& features, bool isHTML, const QualifiedName& attributeName)
{
    auto& nameSet = isHTML ? features.attributeCanonicalLocalNamesInRules : features.attributeLocalNamesInRules;
    return nameSet.contains(attributeName.localName());
}

void AttributeChangeInvalidation::invalidateStyle(const QualifiedName& attributeName, const AtomString& oldValue, const AtomString& newValue)
{
    if (newValue == oldValue)
        return;

    bool isHTML = m_element.isHTMLElement();

    bool shouldInvalidateCurrent = false;
    bool mayAffectStyleInShadowTree = false;

    auto attributeNameForLookups = attributeName.localName().convertToASCIILowercase();

    traverseRuleFeatures(m_element, [&] (const RuleFeatureSet& features, bool mayAffectShadowTree) {
        if (mayAffectShadowTree && mayBeAffectedByAttributeChange(features, isHTML, attributeName))
            mayAffectStyleInShadowTree = true;
        if (features.attributesAffectingHost.contains(attributeNameForLookups))
            shouldInvalidateCurrent = true;
        else if (features.contentAttributeNamesInRules.contains(attributeNameForLookups))
            shouldInvalidateCurrent = true;
    });

    if (mayAffectStyleInShadowTree) {
        // FIXME: More fine-grained invalidation.
        m_element.invalidateStyleForSubtree();
    }

    if (shouldInvalidateCurrent)
        m_element.invalidateStyle();

    auto& ruleSets = m_element.styleResolver().ruleSets();

    auto* invalidationRuleSets = ruleSets.attributeInvalidationRuleSets(attributeNameForLookups);
    if (!invalidationRuleSets)
        return;

    for (auto& invalidationRuleSet : *invalidationRuleSets) {
        for (auto* selector : invalidationRuleSet.invalidationSelectors) {
            bool oldMatches = !oldValue.isNull() && SelectorChecker::attributeSelectorMatches(m_element, attributeName, oldValue, *selector);
            bool newMatches = !newValue.isNull() && SelectorChecker::attributeSelectorMatches(m_element, attributeName, newValue, *selector);
            if (oldMatches != newMatches) {
                Invalidator::addToMatchElementRuleSets(m_matchElementRuleSets, invalidationRuleSet);
                break;
            }
        }
    }
}

void AttributeChangeInvalidation::invalidateStyleWithRuleSets()
{
    Invalidator::invalidateWithMatchElementRuleSets(m_element, m_matchElementRuleSets);
}


}
}
