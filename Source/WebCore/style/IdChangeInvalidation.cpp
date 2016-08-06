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

namespace WebCore {
namespace Style {

static bool mayBeAffectedByHostStyle(ShadowRoot& shadowRoot, const AtomicString& changedId)
{
    auto& shadowRuleSets = shadowRoot.styleResolver().ruleSets();
    if (shadowRuleSets.authorStyle().hostPseudoClassRules().isEmpty())
        return false;

    return shadowRuleSets.features().idsInRules.contains(changedId.impl());
}

void IdChangeInvalidation::invalidateStyle(const AtomicString& changedId)
{
    if (changedId.isEmpty())
        return;

    auto& ruleSets = m_element.styleResolver().ruleSets();

    bool mayAffectStyle = ruleSets.features().idsInRules.contains(changedId.impl());

    auto* shadowRoot = m_element.shadowRoot();
    if (!mayAffectStyle && shadowRoot && mayBeAffectedByHostStyle(*shadowRoot, changedId))
        mayAffectStyle = true;

    if (!mayAffectStyle)
        return;

    if (m_element.shadowRoot() && ruleSets.authorStyle().hasShadowPseudoElementRules()) {
        m_element.setNeedsStyleRecalc(FullStyleChange);
        return;
    }

    m_element.setNeedsStyleRecalc(InlineStyleChange);

    // This could be easily optimized for fine-grained descendant invalidation similar to ClassChangeInvalidation.
    // However using ids for dynamic styling is rare and this is probably not worth the memory cost of the required data structures.
    bool mayAffectDescendantStyle = ruleSets.features().idsMatchingAncestorsInRules.contains(changedId.impl());
    if (mayAffectDescendantStyle)
        m_element.setNeedsStyleRecalc(FullStyleChange);
    else
        m_element.setNeedsStyleRecalc(InlineStyleChange);
}

}
}
