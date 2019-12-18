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
#include "ClassChangeInvalidation.h"

#include "ElementChildIterator.h"
#include "SpaceSplitString.h"
#include "StyleInvalidationFunctions.h"
#include <wtf/BitVector.h>

namespace WebCore {
namespace Style {

using ClassChangeVector = Vector<AtomStringImpl*, 4>;

static ClassChangeVector collectClasses(const SpaceSplitString& classes)
{
    ClassChangeVector result;
    result.reserveCapacity(classes.size());
    for (unsigned i = 0; i < classes.size(); ++i)
        result.uncheckedAppend(classes[i].impl());
    return result;
}

static ClassChangeVector computeClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    unsigned oldSize = oldClasses.size();
    unsigned newSize = newClasses.size();

    if (!oldSize)
        return collectClasses(newClasses);
    if (!newSize)
        return collectClasses(oldClasses);

    ClassChangeVector changedClasses;

    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldSize);
    // Class vectors tend to be very short. This is faster than using a hash table.
    for (unsigned i = 0; i < newSize; ++i) {
        bool foundFromBoth = false;
        for (unsigned j = 0; j < oldSize; ++j) {
            if (newClasses[i] == oldClasses[j]) {
                remainingClassBits.quickSet(j);
                foundFromBoth = true;
            }
        }
        if (foundFromBoth)
            continue;
        changedClasses.append(newClasses[i].impl());
    }
    for (unsigned i = 0; i < oldSize; ++i) {
        // If the bit is not set the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        changedClasses.append(oldClasses[i].impl());
    }

    return changedClasses;
}

void ClassChangeInvalidation::computeInvalidation(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    auto changedClasses = computeClassChange(oldClasses, newClasses);

    bool shouldInvalidateCurrent = false;
    bool mayAffectStyleInShadowTree = false;

    traverseRuleFeatures(m_element, [&] (const RuleFeatureSet& features, bool mayAffectShadowTree) {
        for (auto* changedClass : changedClasses) {
            if (mayAffectShadowTree && features.classRules.contains(changedClass))
                mayAffectStyleInShadowTree = true;
            if (features.classesAffectingHost.contains(changedClass))
                shouldInvalidateCurrent = true;
        }
    });

    if (mayAffectStyleInShadowTree) {
        // FIXME: We should do fine-grained invalidation for shadow tree.
        m_element.invalidateStyleForSubtree();
    }

    if (shouldInvalidateCurrent)
        m_element.invalidateStyle();

    auto& ruleSets = m_element.styleResolver().ruleSets();

    for (auto* changedClass : changedClasses) {
        if (auto* invalidationRuleSets = ruleSets.classInvalidationRuleSets(changedClass)) {
            for (auto& invalidationRuleSet : *invalidationRuleSets)
                Invalidator::addToMatchElementRuleSets(m_matchElementRuleSets, invalidationRuleSet);
        }
    }
}

void ClassChangeInvalidation::invalidateStyleWithRuleSets()
{
    Invalidator::invalidateWithMatchElementRuleSets(m_element, m_matchElementRuleSets);
}

}
}
