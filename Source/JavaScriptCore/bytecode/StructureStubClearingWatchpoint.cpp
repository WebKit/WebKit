/*
 * Copyright (C) 2012, 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "StructureStubClearingWatchpoint.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "JSCellInlines.h"
#include "StructureStubInfo.h"

namespace JSC {

void StructureTransitionStructureStubClearingWatchpoint::fireInternal(VM& vm, const FireDetail&)
{
    if (!m_holder->isValid())
        return;

    if (!m_key || !m_key.isWatchable(PropertyCondition::EnsureWatchability)) {
        // This will implicitly cause my own demise: stub reset removes all watchpoints.
        // That works, because deleting a watchpoint removes it from the set's list, and
        // the set's list traversal for firing is robust against the set changing.
        ConcurrentJSLocker locker(m_holder->codeBlock()->m_lock);
        m_holder->stubInfo()->reset(locker, m_holder->codeBlock());
        return;
    }

    if (m_key.kind() == PropertyCondition::Presence) {
        // If this was a presence condition, let's watch the property for replacements. This is profitable
        // for the DFG, which will want the replacement set to be valid in order to do constant folding.
        m_key.object()->structure(vm)->startWatchingPropertyForReplacements(vm, m_key.offset());
    }

    m_key.object()->structure(vm)->addTransitionWatchpoint(this);
}

inline bool WatchpointsOnStructureStubInfo::isValid() const
{
    return m_codeBlock->isLive();
}

WatchpointsOnStructureStubInfo::Node& WatchpointsOnStructureStubInfo::addWatchpoint(const ObjectPropertyCondition& key)
{
    if (!key || key.condition().kind() != PropertyCondition::Equivalence)
        return *m_watchpoints.add(WTF::in_place<StructureTransitionStructureStubClearingWatchpoint>, key, *this);
    ASSERT(key.condition().kind() == PropertyCondition::Equivalence);
    return *m_watchpoints.add(WTF::in_place<AdaptiveValueStructureStubClearingWatchpoint>, key, *this);
}

void WatchpointsOnStructureStubInfo::ensureReferenceAndInstallWatchpoint(
    std::unique_ptr<WatchpointsOnStructureStubInfo>& holderRef, CodeBlock* codeBlock,
    StructureStubInfo* stubInfo, const ObjectPropertyCondition& key)
{
    if (!holderRef)
        holderRef = makeUnique<WatchpointsOnStructureStubInfo>(codeBlock, stubInfo);
    else {
        ASSERT(holderRef->m_codeBlock == codeBlock);
        ASSERT(holderRef->m_stubInfo == stubInfo);
    }
    
    ASSERT(!!key);
    auto& watchpointVariant = holderRef->addWatchpoint(key);
    if (key.kind() == PropertyCondition::Equivalence) {
        auto& adaptiveWatchpoint = WTF::get<AdaptiveValueStructureStubClearingWatchpoint>(watchpointVariant);
        adaptiveWatchpoint.install(codeBlock->vm());
    } else {
        auto* structureTransitionWatchpoint = &WTF::get<StructureTransitionStructureStubClearingWatchpoint>(watchpointVariant);
        key.object()->structure()->addTransitionWatchpoint(structureTransitionWatchpoint);
    }
}

Watchpoint* WatchpointsOnStructureStubInfo::ensureReferenceAndAddWatchpoint(
    std::unique_ptr<WatchpointsOnStructureStubInfo>& holderRef, CodeBlock* codeBlock,
    StructureStubInfo* stubInfo)
{
    if (!holderRef)
        holderRef = makeUnique<WatchpointsOnStructureStubInfo>(codeBlock, stubInfo);
    else {
        ASSERT(holderRef->m_codeBlock == codeBlock);
        ASSERT(holderRef->m_stubInfo == stubInfo);
    }
    
    return &WTF::get<StructureTransitionStructureStubClearingWatchpoint>(holderRef->addWatchpoint(ObjectPropertyCondition()));
}

void AdaptiveValueStructureStubClearingWatchpoint::handleFire(VM&, const FireDetail&)
{
    if (!m_holder->isValid())
        return;

    // This will implicitly cause my own demise: stub reset removes all watchpoints.
    // That works, because deleting a watchpoint removes it from the set's list, and
    // the set's list traversal for firing is robust against the set changing.
    ConcurrentJSLocker locker(m_holder->codeBlock()->m_lock);
    m_holder->stubInfo()->reset(locker, m_holder->codeBlock());
}

} // namespace JSC

#endif // ENABLE(JIT)

