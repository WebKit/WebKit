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
#include "SpaceSplitString.h"
#include "StyleInvalidationAnalysis.h"

namespace WebCore {
namespace Style {

auto ClassChangeInvalidation::collectClasses(const SpaceSplitString& classes) -> ClassChangeVector
{
    ClassChangeVector result;
    result.reserveCapacity(classes.size());
    for (unsigned i = 0; i < classes.size(); ++i)
        result.uncheckedAppend(classes[i].impl());
    return result;
}

void ClassChangeInvalidation::computeClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    unsigned oldSize = oldClasses.size();
    unsigned newSize = newClasses.size();

    if (!oldSize) {
        m_addedClasses = collectClasses(newClasses);
        return;
    }
    if (!newSize) {
        m_removedClasses = collectClasses(oldClasses);
        return;
    }

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
        m_addedClasses.append(newClasses[i].impl());
    }
    for (unsigned i = 0; i < oldSize; ++i) {
        // If the bit is not set the the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        m_removedClasses.append(oldClasses[i].impl());
    }
}

void ClassChangeInvalidation::invalidateStyle(const ClassChangeVector& changedClasses)
{
    auto& ruleSets = m_element.styleResolver().ruleSets();

    Vector<AtomicStringImpl*, 4> changedClassesAffectingStyle;
    for (auto* changedClass : changedClasses) {
        if (ruleSets.features().classesInRules.contains(changedClass))
            changedClassesAffectingStyle.append(changedClass);
    };

    if (changedClassesAffectingStyle.isEmpty())
        return;

    if (m_element.shadowRoot() && ruleSets.authorStyle()->hasShadowPseudoElementRules()) {
        m_element.setNeedsStyleRecalc(FullStyleChange);
        return;
    }

    m_element.setNeedsStyleRecalc(InlineStyleChange);

    if (!childrenOfType<Element>(m_element).first())
        return;

    for (auto* changedClass : changedClassesAffectingStyle) {
        auto* ancestorClassRules = ruleSets.ancestorClassRules(changedClass);
        if (!ancestorClassRules)
            continue;
        StyleInvalidationAnalysis invalidationAnalysis(*ancestorClassRules);
        invalidationAnalysis.invalidateStyle(m_element);
    }
}

}
}
