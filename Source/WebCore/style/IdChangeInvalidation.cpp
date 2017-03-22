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
#include "IdChangeInvalidation.h"

#include "DocumentRuleSets.h"
#include "ElementChildIterator.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "StyleScope.h"

namespace WebCore {
namespace Style {

static bool mayBeAffectedByHostRules(const Element& element, const AtomicString& changedId)
{
    auto* shadowRoot = element.shadowRoot();
    if (!shadowRoot)
        return false;
    auto& shadowRuleSets = shadowRoot->styleScope().resolver().ruleSets();
    if (shadowRuleSets.authorStyle().hostPseudoClassRules().isEmpty())
        return false;
    return shadowRuleSets.features().idsInRules.contains(changedId);
}

static bool mayBeAffectedBySlottedRules(const Element& element, const AtomicString& changedId)
{
    for (auto* shadowRoot : assignedShadowRootsIfSlotted(element)) {
        auto& ruleSets = shadowRoot->styleScope().resolver().ruleSets();
        if (ruleSets.authorStyle().slottedPseudoElementRules().isEmpty())
            continue;
        if (ruleSets.features().idsInRules.contains(changedId))
            return true;
    }
    return false;
}

void IdChangeInvalidation::invalidateStyle(const AtomicString& changedId)
{
    if (changedId.isEmpty())
        return;

    auto& ruleSets = m_element.styleResolver().ruleSets();

    bool mayAffectStyle = ruleSets.features().idsInRules.contains(changedId)
        || mayBeAffectedByHostRules(m_element, changedId)
        || mayBeAffectedBySlottedRules(m_element, changedId);

    if (!mayAffectStyle)
        return;

    if (m_element.shadowRoot() && ruleSets.authorStyle().hasShadowPseudoElementRules()) {
        m_element.invalidateStyleForSubtree();
        return;
    }

    m_element.invalidateStyle();

    // This could be easily optimized for fine-grained descendant invalidation similar to ClassChangeInvalidation.
    // However using ids for dynamic styling is rare and this is probably not worth the memory cost of the required data structures.
    bool mayAffectDescendantStyle = ruleSets.features().idsMatchingAncestorsInRules.contains(changedId);
    if (mayAffectDescendantStyle)
        m_element.invalidateStyleForSubtree();
    else
        m_element.invalidateStyle();
}

}
}
