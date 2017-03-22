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

#include "DocumentRuleSets.h"
#include "ElementIterator.h"
#include "ShadowRoot.h"
#include "StyleInvalidationAnalysis.h"
#include "StyleResolver.h"
#include "StyleScope.h"

namespace WebCore {
namespace Style {

static bool mayBeAffectedByAttributeChange(DocumentRuleSets& ruleSets, bool isHTML, const QualifiedName& attributeName)
{
    auto& nameSet = isHTML ? ruleSets.features().attributeCanonicalLocalNamesInRules : ruleSets.features().attributeLocalNamesInRules;
    return nameSet.contains(attributeName.localName());
}

static bool mayBeAffectedByHostRules(const Element& element, const QualifiedName& attributeName)
{
    auto* shadowRoot = element.shadowRoot();
    if (!shadowRoot)
        return false;
    auto& shadowRuleSets = shadowRoot->styleScope().resolver().ruleSets();
    if (shadowRuleSets.authorStyle().hostPseudoClassRules().isEmpty())
        return false;

    return mayBeAffectedByAttributeChange(shadowRuleSets, element.isHTMLElement(), attributeName);
}

static bool mayBeAffectedBySlottedRules(const Element& element, const QualifiedName& attributeName)
{
    for (auto* shadowRoot : assignedShadowRootsIfSlotted(element)) {
        auto& ruleSets = shadowRoot->styleScope().resolver().ruleSets();
        if (ruleSets.authorStyle().slottedPseudoElementRules().isEmpty())
            continue;
        if (mayBeAffectedByAttributeChange(ruleSets, element.isHTMLElement(), attributeName))
            return true;
    }
    return false;
}

void AttributeChangeInvalidation::invalidateStyle(const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (newValue == oldValue)
        return;

    auto& ruleSets = m_element.styleResolver().ruleSets();
    bool isHTML = m_element.isHTMLElement();

    bool mayAffectStyle = mayBeAffectedByAttributeChange(ruleSets, isHTML, attributeName)
        || mayBeAffectedByHostRules(m_element, attributeName)
        || mayBeAffectedBySlottedRules(m_element, attributeName);

    if (!mayAffectStyle)
        return;

    if (!isHTML) {
        m_element.invalidateStyleForSubtree();
        return;
    }

    if (m_element.shadowRoot() && ruleSets.authorStyle().hasShadowPseudoElementRules()) {
        m_element.invalidateStyleForSubtree();
        return;
    }

    m_element.invalidateStyle();

    if (!childrenOfType<Element>(m_element).first())
        return;

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
    StyleInvalidationAnalysis invalidationAnalysis(*m_descendantInvalidationRuleSet);
    invalidationAnalysis.invalidateStyle(m_element);
}

}
}
