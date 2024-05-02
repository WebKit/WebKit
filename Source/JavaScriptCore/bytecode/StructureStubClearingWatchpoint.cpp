/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(StructureStubInfoClearingWatchpoint);
WTF_MAKE_TZONE_ALLOCATED_IMPL(AdaptiveValueStructureStubClearingWatchpoint);
WTF_MAKE_TZONE_ALLOCATED_IMPL(StructureTransitionStructureStubClearingWatchpoint);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WatchpointsOnStructureStubInfo);

void StructureStubInfoClearingWatchpoint::fireInternal(VM&, const FireDetail&)
{
    if (!m_owner->isLive())
        return;

    // This will implicitly cause my own demise: stub reset removes all watchpoints.
    // That works, because deleting a watchpoint removes it from the set's list, and
    // the set's list traversal for firing is robust against the set changing.
    ConcurrentJSLocker locker(m_owner->m_lock);
    m_stubInfo->reset(locker, m_owner.get());
}

void StructureTransitionStructureStubClearingWatchpoint::fireInternal(VM& vm, const FireDetail&)
{
    if (!m_key || !m_key.isWatchable(PropertyCondition::EnsureWatchability)) {
        m_holder->stub()->invalidate();
        return;
    }

    if (m_key.kind() == PropertyCondition::Presence) {
        // If this was a presence condition, let's watch the property for replacements. This is profitable
        // for the DFG, which will want the replacement set to be valid in order to do constant folding.
        m_key.object()->structure()->startWatchingPropertyForReplacements(vm, m_key.offset());
    }

    m_key.object()->structure()->addTransitionWatchpoint(this);
}

WatchpointsOnStructureStubInfo::Node& WatchpointsOnStructureStubInfo::addWatchpoint(const ObjectPropertyCondition& key)
{
    if (!key || key.condition().kind() != PropertyCondition::Equivalence)
        return *m_watchpoints.add(std::in_place_type<StructureTransitionStructureStubClearingWatchpoint>, key, *this);
    ASSERT(key.condition().kind() == PropertyCondition::Equivalence);
    return *m_watchpoints.add(std::in_place_type<AdaptiveValueStructureStubClearingWatchpoint>, key, *this);
}

void WatchpointsOnStructureStubInfo::ensureReferenceAndInstallWatchpoint(VM& vm, std::unique_ptr<WatchpointsOnStructureStubInfo>& holderRef, PolymorphicAccessJITStubRoutine* stub, const ObjectPropertyCondition& key)
{
    if (!holderRef)
        holderRef = makeUnique<WatchpointsOnStructureStubInfo>(stub);
    else
        ASSERT(holderRef->m_stub == stub);

    ASSERT(!!key);
    auto& watchpointVariant = holderRef->addWatchpoint(key);
    if (key.kind() == PropertyCondition::Equivalence) {
        auto& adaptiveWatchpoint = std::get<AdaptiveValueStructureStubClearingWatchpoint>(watchpointVariant);
        adaptiveWatchpoint.install(vm);
    } else {
        auto* structureTransitionWatchpoint = &std::get<StructureTransitionStructureStubClearingWatchpoint>(watchpointVariant);
        key.object()->structure()->addTransitionWatchpoint(structureTransitionWatchpoint);
    }
}

Watchpoint* WatchpointsOnStructureStubInfo::ensureReferenceAndAddWatchpoint(VM&, std::unique_ptr<WatchpointsOnStructureStubInfo>& holderRef, PolymorphicAccessJITStubRoutine* stub)
{
    if (!holderRef)
        holderRef = makeUnique<WatchpointsOnStructureStubInfo>(stub);
    else {
        ASSERT(holderRef->m_stub == stub);
    }

    return &std::get<StructureTransitionStructureStubClearingWatchpoint>(holderRef->addWatchpoint(ObjectPropertyCondition()));
}

void AdaptiveValueStructureStubClearingWatchpoint::handleFire(VM&, const FireDetail&)
{
    m_holder->stub()->invalidate();
}

} // namespace JSC

#endif // ENABLE(JIT)

