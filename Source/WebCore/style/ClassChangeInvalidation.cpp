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

#include "DocumentRuleSets.h"
#include "ElementChildIterator.h"
#include "ShadowRoot.h"
#include "SpaceSplitString.h"
#include "StyleInvalidationAnalysis.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/BitVector.h>

namespace WebCore {
namespace Style {

using ClassChangeVector = Vector<AtomicStringImpl*, 4>;

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
        // If the bit is not set the the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        changedClasses.append(oldClasses[i].impl());
    }

    return changedClasses;
}

static bool mayBeAffectedByHostRules(ShadowRoot* shadowRoot, AtomicStringImpl* changedClass)
{
    if (!shadowRoot)
        return false;
    auto& shadowRuleSets = shadowRoot->styleScope().resolver().ruleSets();
    if (shadowRuleSets.authorStyle().hostPseudoClassRules().isEmpty())
        return false;
    return shadowRuleSets.features().classesInRules.contains(changedClass);
}

static bool mayBeAffectedBySlottedRules(const Vector<ShadowRoot*>& assignedShadowRoots, AtomicStringImpl* changedClass)
{
    for (auto& assignedShadowRoot : assignedShadowRoots) {
        auto& ruleSets = assignedShadowRoot->styleScope().resolver().ruleSets();
        if (ruleSets.authorStyle().slottedPseudoElementRules().isEmpty())
            continue;
        if (ruleSets.features().classesInRules.contains(changedClass))
            return true;
    }
    return false;
}

void ClassChangeInvalidation::invalidateStyle(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    auto changedClasses = computeClassChange(oldClasses, newClasses);

    auto& ruleSets = m_element.styleResolver().ruleSets();
    auto* shadowRoot = m_element.shadowRoot();
    auto assignedShadowRoots = assignedShadowRootsIfSlotted(m_element);

    ClassChangeVector changedClassesAffectingStyle;
    for (auto* changedClass : changedClasses) {
        bool mayAffectStyle = ruleSets.features().classesInRules.contains(changedClass)
            || mayBeAffectedByHostRules(shadowRoot, changedClass)
            || mayBeAffectedBySlottedRules(assignedShadowRoots, changedClass);
        if (mayAffectStyle)
            changedClassesAffectingStyle.append(changedClass);
    };

    if (changedClassesAffectingStyle.isEmpty())
        return;

    if (shadowRoot && ruleSets.authorStyle().hasShadowPseudoElementRules()) {
        m_element.invalidateStyleForSubtree();
        return;
    }

    m_element.invalidateStyle();

    if (!childrenOfType<Element>(m_element).first())
        return;

    for (auto* changedClass : changedClassesAffectingStyle) {
        auto* ancestorClassRules = ruleSets.ancestorClassRules(changedClass);
        if (!ancestorClassRules)
            continue;
        m_descendantInvalidationRuleSets.append(ancestorClassRules);
    }
}

void ClassChangeInvalidation::invalidateDescendantStyle()
{
    for (auto* ancestorClassRules : m_descendantInvalidationRuleSets) {
        StyleInvalidationAnalysis invalidationAnalysis(*ancestorClassRules);
        invalidationAnalysis.invalidateStyle(m_element);
    }
}

}
}
