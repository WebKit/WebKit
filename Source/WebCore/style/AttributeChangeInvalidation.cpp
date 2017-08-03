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
#include "StyleInvalidator.h"

namespace WebCore {
namespace Style {

static bool mayBeAffectedByAttributeChange(const RuleFeatureSet& features, bool isHTML, const QualifiedName& attributeName)
{
    auto& nameSet = isHTML ? features.attributeCanonicalLocalNamesInRules : features.attributeLocalNamesInRules;
    return nameSet.contains(attributeName.localName());
}

void AttributeChangeInvalidation::invalidateStyle(const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (newValue == oldValue)
        return;

    bool isHTML = m_element.isHTMLElement();

    bool mayAffectStyle = false;
    bool mayAffectStyleInShadowTree = false;

    traverseRuleFeatures(m_element, [&] (const RuleFeatureSet& features, bool mayAffectShadowTree) {
        if (!mayBeAffectedByAttributeChange(features, isHTML, attributeName))
            return;
        mayAffectStyle = true;
        if (mayAffectShadowTree)
            mayAffectStyleInShadowTree = true;
    });

    if (!mayAffectStyle)
        return;

    if (!isHTML) {
        m_element.invalidateStyleForSubtree();
        return;
    }

    if (mayAffectStyleInShadowTree) {
        // FIXME: More fine-grained invalidation.
        m_element.invalidateStyleForSubtree();
        return;
    }

    m_element.invalidateStyle();

    if (!childrenOfType<Element>(m_element).first())
        return;

    auto& ruleSets = m_element.styleResolver().ruleSets();
    auto* attributeRules = ruleSets.ancestorAttributeRulesForHTML(attributeName.localName());
    if (!attributeRules)
        return;

    // Check if descendants may be affected by this attribute change.
    for (auto* selector : attributeRules->attributeSelectors) {
        bool oldMatches = oldValue.isNull() ? false : SelectorChecker::attributeSelectorMatches(m_element, attributeName, oldValue, *selector);
        bool newMatches = newValue.isNull() ? false : SelectorChecker::attributeSelectorMatches(m_element, attributeName, newValue, *selector);

        if (oldMatches != newMatches) {
            m_descendantInvalidationRuleSet = attributeRules->ruleSet.get();
            return;
        }
    }
}

void AttributeChangeInvalidation::invalidateDescendants()
{
    if (!m_descendantInvalidationRuleSet)
        return;
    Invalidator invalidator(*m_descendantInvalidationRuleSet);
    invalidator.invalidateStyle(m_element);
}

}
}
