/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "PseudoClassChangeInvalidation.h"

#include "ElementChildIterator.h"
#include "StyleInvalidationFunctions.h"

namespace WebCore {
namespace Style {

void PseudoClassChangeInvalidation::computeInvalidation(CSSSelector::PseudoClassType pseudoClass)
{
    bool shouldInvalidateCurrent = false;
    bool mayAffectStyleInShadowTree = false;

    traverseRuleFeatures(m_element, [&] (const RuleFeatureSet& features, bool mayAffectShadowTree) {
        if (mayAffectShadowTree && features.pseudoClassRules.contains(pseudoClass))
            mayAffectStyleInShadowTree = true;
        if (m_element.shadowRoot() && features.pseudoClassesAffectingHost.contains(pseudoClass))
            shouldInvalidateCurrent = true;
    });

    if (mayAffectStyleInShadowTree) {
        // FIXME: We should do fine-grained invalidation for shadow tree.
        m_element.invalidateStyleForSubtree();
    }

    if (shouldInvalidateCurrent)
        m_element.invalidateStyle();

    auto& ruleSets = m_element.styleResolver().ruleSets();
    if (auto* invalidationRuleSets = ruleSets.pseudoClassInvalidationRuleSets(pseudoClass)) {
        for (auto& invalidationRuleSet : *invalidationRuleSets)
            Invalidator::addToMatchElementRuleSets(m_matchElementRuleSets, invalidationRuleSet);
    }
}

void PseudoClassChangeInvalidation::invalidateStyleWithRuleSets()
{
    Invalidator::invalidateWithMatchElementRuleSets(m_element, m_matchElementRuleSets);
}

}
}
