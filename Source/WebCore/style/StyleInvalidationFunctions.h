/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "DocumentRuleSets.h"
#include "HTMLSlotElement.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleScope.h"

namespace WebCore {
namespace Style {

template <typename TraverseFunction>
inline void traverseRuleFeaturesInShadowTree(Element& element, TraverseFunction&& function)
{
    if (!element.shadowRoot())
        return;
    auto& shadowRuleSets = element.shadowRoot()->styleScope().resolver().ruleSets();
    auto& authorStyle = shadowRuleSets.authorStyle();
    bool hasHostPseudoClassRulesMatchingInShadowTree = authorStyle.hasHostPseudoClassRulesMatchingInShadowTree();
    if (authorStyle.hostPseudoClassRules().isEmpty() && !hasHostPseudoClassRulesMatchingInShadowTree)
        return;
    function(shadowRuleSets.features(), hasHostPseudoClassRulesMatchingInShadowTree);
}

template <typename TraverseFunction>
inline void traverseRuleFeaturesForSlotted(Element& element, TraverseFunction&& function)
{
    auto assignedShadowRoots = assignedShadowRootsIfSlotted(element);
    for (auto& assignedShadowRoot : assignedShadowRoots) {
        auto& ruleSets = assignedShadowRoot->styleScope().resolver().ruleSets();
        if (ruleSets.authorStyle().slottedPseudoElementRules().isEmpty())
            continue;
        function(ruleSets.features(), false);
    }
}

template <typename TraverseFunction>
inline void traverseRuleFeatures(Element& element, TraverseFunction&& function)
{
    auto& ruleSets = element.styleResolver().ruleSets();

    auto mayAffectShadowTree = [&] {
        if (element.shadowRoot() && ruleSets.authorStyle().hasShadowPseudoElementRules())
            return true;
        if (is<HTMLSlotElement>(element) && !ruleSets.authorStyle().slottedPseudoElementRules().isEmpty())
            return true;
        return false;
    };

    function(ruleSets.features(), mayAffectShadowTree());

    traverseRuleFeaturesInShadowTree(element, function);
    traverseRuleFeaturesForSlotted(element, function);

    // Ensure that the containing tree resolver also exists so it doesn't get created in the middle of invalidation.
    if (element.isInShadowTree())
        Style::Scope::forNode(*element.containingShadowRoot()->host()).resolver();
}

}
}

