/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSSelectorList.h"
#include "SelectorFilter.h"

namespace WebCore {

inline bool SelectorFilter::fastRejectSelector(const Hashes& hashes) const
{
    for (auto& hash : hashes) {
        if (!hash)
            return false;
        if (!m_ancestorIdentifierFilter.mayContain(hash))
            return true;
    }
    return false;
}

inline void SelectorFilter::collectSelectorHashes(CollectedSelectorHashes& collectedHashes, const CSSSelector& rightmostSelector, IncludeRightmost includeRightmost)
{
    auto [selector, relation, skipOverSubselectors] = [&] {
        if (includeRightmost == IncludeRightmost::No)
            return std::tuple { rightmostSelector.tagHistory(), rightmostSelector.relation(), true };

        return std::tuple { &rightmostSelector, CSSSelector::Relation::Subselector, false };
    }();

    for (; selector; selector = selector->tagHistory()) {
        // Only collect identifiers that match ancestors.
        switch (relation) {
        case CSSSelector::Relation::Subselector:
            if (!skipOverSubselectors)
                collectSimpleSelectorHash(collectedHashes, *selector);
            break;
        case CSSSelector::Relation::DirectAdjacent:
        case CSSSelector::Relation::IndirectAdjacent:
        case CSSSelector::Relation::ShadowDescendant:
        case CSSSelector::Relation::ShadowPartDescendant:
        case CSSSelector::Relation::ShadowSlotted:
            skipOverSubselectors = true;
            break;
        case CSSSelector::Relation::DescendantSpace:
        case CSSSelector::Relation::Child:
            skipOverSubselectors = false;
            collectSimpleSelectorHash(collectedHashes, *selector);
            break;
        }
        relation = selector->relation();
    }
}

inline auto SelectorFilter::chooseSelectorHashesForFilter(const CollectedSelectorHashes& collectedSelectorHashes) -> Hashes
{
    Hashes resultHashes;
    unsigned index = 0;

    auto addIfNew = [&] (unsigned hash) {
        for (unsigned i = 0; i < index; ++i) {
            if (resultHashes[i] == hash)
                return;
        }
        resultHashes[index++] = hash;
    };

    auto copyHashes = [&] (auto& hashes) {
        for (auto& hash : hashes) {
            addIfNew(hash);
            if (index == resultHashes.size())
                return true;
        }
        return false;
    };

    // There is a limited number of slots. Prefer more specific selector types.
    if (copyHashes(collectedSelectorHashes.ids))
        return resultHashes;
    if (copyHashes(collectedSelectorHashes.attributes))
        return resultHashes;
    if (copyHashes(collectedSelectorHashes.classes))
        return resultHashes;
    if (copyHashes(collectedSelectorHashes.tags))
        return resultHashes;

    // Null-terminate if not full.
    resultHashes[index] = 0;
    return resultHashes;
}

SelectorFilter::Hashes SelectorFilter::collectHashes(const CSSSelector& selector)
{
    CollectedSelectorHashes collectedHashes;
    collectSelectorHashes(collectedHashes, selector, IncludeRightmost::No);
    return chooseSelectorHashesForFilter(collectedHashes);
}

}
