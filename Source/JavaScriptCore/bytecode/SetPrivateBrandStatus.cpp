/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Igalia S.A. All rights reserved.
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
#include "SetPrivateBrandStatus.h"

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "ICStatusUtils.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>

namespace JSC {

bool SetPrivateBrandStatus::appendVariant(const SetPrivateBrandVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

SetPrivateBrandStatus SetPrivateBrandStatus::computeForBaseline(CodeBlock* baselineBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, ExitFlag didExit)
{
    ConcurrentJSLocker locker(baselineBlock->m_lock);

    SetPrivateBrandStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfoWithoutExitSiteFeedback(
        locker, baselineBlock, map.get(CodeOrigin(bytecodeIndex)).stubInfo);

    if (didExit)
        return result.slowVersion();
#else
    UNUSED_PARAM(map);
    UNUSED_PARAM(didExit);
    UNUSED_PARAM(bytecodeIndex);
#endif

    return result;
}

#if ENABLE(JIT)
SetPrivateBrandStatus::SetPrivateBrandStatus(StubInfoSummary summary, StructureStubInfo& stubInfo)
{
    switch (summary) {
    case StubInfoSummary::NoInformation:
        m_state = NoInformation;
        return;
    case StubInfoSummary::Simple:
    case StubInfoSummary::MakesCalls:
    case StubInfoSummary::TakesSlowPathAndMakesCalls:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    case StubInfoSummary::TakesSlowPath:
        m_state = stubInfo.tookSlowPath ? ObservedTakesSlowPath : LikelyTakesSlowPath;
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

SetPrivateBrandStatus SetPrivateBrandStatus::computeForStubInfoWithoutExitSiteFeedback(
    const ConcurrentJSLocker&, CodeBlock* block, StructureStubInfo* stubInfo)
{
    StubInfoSummary summary = StructureStubInfo::summary(block->vm(), stubInfo);
    if (!isInlineable(summary))
        return SetPrivateBrandStatus(summary, *stubInfo);

    SetPrivateBrandStatus result;
    result.m_state = Simple;
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        return SetPrivateBrandStatus(NoInformation);

    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->u.stub;

        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);

            Structure* structure = access.structure();
            ASSERT(structure);
            ASSERT(access.type() == AccessCase::SetPrivateBrand);

            Structure* newStructure = Structure::setBrandTransitionFromExistingStructureConcurrently(structure, access.identifier().uid());
            if (!newStructure)
                return SetPrivateBrandStatus(JSC::slowVersion(summary), *stubInfo);

            SetPrivateBrandVariant variant(access.identifier(), structure, newStructure);
            if (!result.appendVariant(variant))
                return SetPrivateBrandStatus(JSC::slowVersion(summary), *stubInfo);
        }

        return result;
    }

    default:
        return SetPrivateBrandStatus(JSC::slowVersion(summary), *stubInfo);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return SetPrivateBrandStatus();
}

SetPrivateBrandStatus SetPrivateBrandStatus::computeFor(
    CodeBlock* baselineBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& contextStack, CodeOrigin codeOrigin)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    ExitFlag didExit = hasBadCacheExitSite(baselineBlock, bytecodeIndex);

    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);

        auto bless = [&] (const SetPrivateBrandStatus& result) -> SetPrivateBrandStatus {
            if (!context->isInlined(codeOrigin)) {
                SetPrivateBrandStatus baselineResult = computeForBaseline(
                    baselineBlock, baselineMap, bytecodeIndex, didExit);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };

        if (status.stubInfo) {
            SetPrivateBrandStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfoWithoutExitSiteFeedback(
                    locker, context->optimizedCodeBlock, status.stubInfo);
            }
            if (result.isSet())
                return bless(result);
        }
    }

    return computeForBaseline(baselineBlock, baselineMap, bytecodeIndex, didExit);
}

#endif // ENABLE(JIT)

SetPrivateBrandStatus SetPrivateBrandStatus::slowVersion() const
{
    if (observedSlowPath())
        return SetPrivateBrandStatus(ObservedTakesSlowPath);
    return SetPrivateBrandStatus(LikelyTakesSlowPath);
}

void SetPrivateBrandStatus::merge(const SetPrivateBrandStatus& other)
{
    if (other.m_state == NoInformation)
        return;

    auto mergeSlow = [&] () {
        if (observedSlowPath() || other.observedSlowPath())
            *this = SetPrivateBrandStatus(ObservedTakesSlowPath);
        else
            *this = SetPrivateBrandStatus(LikelyTakesSlowPath);
    };

    switch (m_state) {
    case NoInformation:
        *this = other;
        return;

    case Simple:
        if (m_state != other.m_state)
            return mergeSlow();

        for (auto& otherVariant : other.m_variants) {
            if (!appendVariant(otherVariant))
                return mergeSlow();
        }
        return;

    case LikelyTakesSlowPath:
    case ObservedTakesSlowPath:
        return mergeSlow();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void SetPrivateBrandStatus::filter(const StructureSet& structureSet)
{
    if (m_state != Simple)
        return;
    m_variants.removeAllMatching(
        [&] (auto& variant) -> bool {
            return !structureSet.contains(variant.oldStructure());
        });
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

CacheableIdentifier SetPrivateBrandStatus::singleIdentifier() const
{
    if (m_variants.isEmpty())
        return nullptr;

    CacheableIdentifier result = m_variants.first().identifier();
    if (!result)
        return nullptr;
    for (size_t i = 1; i < m_variants.size(); ++i) {
        CacheableIdentifier identifier = m_variants[i].identifier();
        if (!identifier)
            return nullptr;
        if (identifier != result)
            return nullptr;
    }
    return result;
}

template<typename Visitor>
void SetPrivateBrandStatus::visitAggregateImpl(Visitor& visitor)
{
    for (SetPrivateBrandVariant& variant : m_variants)
        variant.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(SetPrivateBrandStatus);

template<typename Visitor>
void SetPrivateBrandStatus::markIfCheap(Visitor& visitor)
{
    for (SetPrivateBrandVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

template void SetPrivateBrandStatus::markIfCheap(AbstractSlotVisitor&);
template void SetPrivateBrandStatus::markIfCheap(SlotVisitor&);

bool SetPrivateBrandStatus::finalize(VM& vm)
{
    for (auto& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

void SetPrivateBrandStatus::dump(PrintStream& out) const
{
    out.print("(");
    switch (m_state) {
    case NoInformation:
        out.print("NoInformation");
        break;
    case Simple:
        out.print("Simple");
        break;
    case LikelyTakesSlowPath:
        out.print("LikelyTakesSlowPath");
        break;
    case ObservedTakesSlowPath:
        out.print("ObservedTakesSlowPath");
        break;
    }
    out.print(", ", listDump(m_variants), ")");
}

} // namespace JSC
