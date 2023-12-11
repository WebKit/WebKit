/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "CheckPrivateBrandStatus.h"

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "ICStatusUtils.h"
#include "InlineCacheCompiler.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CheckPrivateBrandStatus);

bool CheckPrivateBrandStatus::appendVariant(const CheckPrivateBrandVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

void CheckPrivateBrandStatus::shrinkToFit()
{
    m_variants.shrinkToFit();
}

CheckPrivateBrandStatus CheckPrivateBrandStatus::computeForBaseline(CodeBlock* baselineBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, ExitFlag didExit)
{
    ConcurrentJSLocker locker(baselineBlock->m_lock);

    CheckPrivateBrandStatus result;

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
CheckPrivateBrandStatus::CheckPrivateBrandStatus(StubInfoSummary summary, StructureStubInfo& stubInfo)
{
    switch (summary) {
    case StubInfoSummary::NoInformation:
        m_state = NoInformation;
        return;
    case StubInfoSummary::Simple:
    case StubInfoSummary::Megamorphic:
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

CheckPrivateBrandStatus CheckPrivateBrandStatus::computeForStubInfoWithoutExitSiteFeedback(
    const ConcurrentJSLocker&, CodeBlock* block, StructureStubInfo* stubInfo)
{
    StubInfoSummary summary = StructureStubInfo::summary(block->vm(), stubInfo);
    if (!isInlineable(summary))
        return CheckPrivateBrandStatus(summary, *stubInfo);

    CheckPrivateBrandStatus result;
    result.m_state = Simple;
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        return CheckPrivateBrandStatus(NoInformation);

    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->m_stub.get();
        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);

            Structure* structure = access.structure();
            ASSERT(structure);
            ASSERT(access.type() == AccessCase::CheckPrivateBrand);

            CheckPrivateBrandVariant variant(access.identifier(), StructureSet(structure));
            if (!result.appendVariant(variant))
                return CheckPrivateBrandStatus(JSC::slowVersion(summary), *stubInfo);
        }

        result.shrinkToFit();
        return result;
    }

    default:
        return CheckPrivateBrandStatus(JSC::slowVersion(summary), *stubInfo);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return CheckPrivateBrandStatus();
}

CheckPrivateBrandStatus CheckPrivateBrandStatus::computeFor(
    CodeBlock* baselineBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& contextStack, CodeOrigin codeOrigin)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    ExitFlag didExit = hasBadCacheExitSite(baselineBlock, bytecodeIndex);

    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);

        auto bless = [&] (const CheckPrivateBrandStatus& result) -> CheckPrivateBrandStatus {
            if (!context->isInlined(codeOrigin)) {
                CheckPrivateBrandStatus baselineResult = computeForBaseline(
                    baselineBlock, baselineMap, bytecodeIndex, didExit);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };

        if (status.stubInfo) {
            CheckPrivateBrandStatus result;
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

CheckPrivateBrandStatus CheckPrivateBrandStatus::slowVersion() const
{
    if (observedSlowPath())
        return CheckPrivateBrandStatus(ObservedTakesSlowPath);
    return CheckPrivateBrandStatus(LikelyTakesSlowPath);
}

void CheckPrivateBrandStatus::merge(const CheckPrivateBrandStatus& other)
{
    if (other.m_state == NoInformation)
        return;

    auto mergeSlow = [&] () {
        if (observedSlowPath() || other.observedSlowPath())
            *this = CheckPrivateBrandStatus(ObservedTakesSlowPath);
        else
            *this = CheckPrivateBrandStatus(LikelyTakesSlowPath);
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
        shrinkToFit();
        return;

    case LikelyTakesSlowPath:
    case ObservedTakesSlowPath:
        return mergeSlow();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void CheckPrivateBrandStatus::filter(const StructureSet& structureSet)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, structureSet);
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

CacheableIdentifier CheckPrivateBrandStatus::singleIdentifier() const
{
    return singleIdentifierForICStatus(m_variants);
}

template<typename Visitor>
void CheckPrivateBrandStatus::visitAggregateImpl(Visitor& visitor)
{
    for (CheckPrivateBrandVariant& variant : m_variants)
        variant.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(CheckPrivateBrandStatus);

template<typename Visitor>
void CheckPrivateBrandStatus::markIfCheap(Visitor& visitor)
{
    for (CheckPrivateBrandVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

template void CheckPrivateBrandStatus::markIfCheap(AbstractSlotVisitor&);
template void CheckPrivateBrandStatus::markIfCheap(SlotVisitor&);

bool CheckPrivateBrandStatus::finalize(VM& vm)
{
    for (auto& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

void CheckPrivateBrandStatus::dump(PrintStream& out) const
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
