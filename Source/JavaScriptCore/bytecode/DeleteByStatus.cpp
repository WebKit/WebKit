/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "DeleteByStatus.h"

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "ICStatusUtils.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>

namespace JSC {

bool DeleteByStatus::appendVariant(const DeleteByVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

void DeleteByStatus::shrinkToFit()
{
    m_variants.shrinkToFit();
}

DeleteByStatus DeleteByStatus::computeForBaseline(CodeBlock* baselineBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, ExitFlag didExit)
{
    ConcurrentJSLocker locker(baselineBlock->m_lock);

    DeleteByStatus result;

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
DeleteByStatus::DeleteByStatus(StubInfoSummary summary, StructureStubInfo& stubInfo)
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

DeleteByStatus DeleteByStatus::computeForStubInfoWithoutExitSiteFeedback(
    const ConcurrentJSLocker&, CodeBlock* block, StructureStubInfo* stubInfo)
{
    StubInfoSummary summary = StructureStubInfo::summary(block->vm(), stubInfo);
    if (!isInlineable(summary))
        return DeleteByStatus(summary, *stubInfo);

    DeleteByStatus result;
    result.m_state = Simple;
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        return DeleteByStatus(NoInformation);

    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->u.stub;

        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);
            ASSERT(!access.viaProxy());

            Structure* structure = access.structure();
            ASSERT(structure);

            switch (access.type()) {
            case AccessCase::DeleteMiss:
            case AccessCase::DeleteNonConfigurable: {
                DeleteByVariant variant(access.identifier(), access.type() == AccessCase::DeleteMiss ? true : false, structure, nullptr, invalidOffset);
                if (!result.appendVariant(variant))
                    return DeleteByStatus(JSC::slowVersion(summary), *stubInfo);
                break;
            }
            case AccessCase::Delete: {
                PropertyOffset offset;
                Structure* newStructure = Structure::removePropertyTransitionFromExistingStructureConcurrently(structure, access.identifier().uid(), offset);
                if (!newStructure)
                    return DeleteByStatus(JSC::slowVersion(summary), *stubInfo);
                ASSERT_UNUSED(offset, offset == access.offset());                
                DeleteByVariant variant(access.identifier(), true, structure, newStructure, access.offset());

                if (!result.appendVariant(variant))
                    return DeleteByStatus(JSC::slowVersion(summary), *stubInfo);
                break;
            }
            default:
                ASSERT_NOT_REACHED();
                return DeleteByStatus(JSC::slowVersion(summary), *stubInfo);
            }
        }

        result.shrinkToFit();
        return result;
    }

    default:
        return DeleteByStatus(JSC::slowVersion(summary), *stubInfo);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return DeleteByStatus();
}

DeleteByStatus DeleteByStatus::computeFor(
    CodeBlock* baselineBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& contextStack, CodeOrigin codeOrigin)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    ExitFlag didExit = hasBadCacheExitSite(baselineBlock, bytecodeIndex);

    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);

        auto bless = [&] (const DeleteByStatus& result) -> DeleteByStatus {
            if (!context->isInlined(codeOrigin)) {
                DeleteByStatus baselineResult = computeForBaseline(
                    baselineBlock, baselineMap, bytecodeIndex, didExit);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };

        if (status.stubInfo) {
            DeleteByStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfoWithoutExitSiteFeedback(
                    locker, context->optimizedCodeBlock, status.stubInfo);
            }
            if (result.isSet())
                return bless(result);
        }

        if (status.deleteStatus)
            return bless(*status.deleteStatus);
    }

    return computeForBaseline(baselineBlock, baselineMap, bytecodeIndex, didExit);
}

#endif // ENABLE(JIT)

DeleteByStatus DeleteByStatus::slowVersion() const
{
    if (observedSlowPath())
        return DeleteByStatus(ObservedTakesSlowPath);
    return DeleteByStatus(LikelyTakesSlowPath);
}

void DeleteByStatus::merge(const DeleteByStatus& other)
{
    if (other.m_state == NoInformation)
        return;

    auto mergeSlow = [&] () {
        if (observedSlowPath() || other.observedSlowPath())
            *this = DeleteByStatus(ObservedTakesSlowPath);
        else
            *this = DeleteByStatus(LikelyTakesSlowPath);
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

void DeleteByStatus::filter(const StructureSet& set)
{
    if (m_state != Simple)
        return;
    m_variants.removeAllMatching(
        [&] (auto& variant) -> bool {
            return !set.contains(variant.oldStructure());
        });
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

CacheableIdentifier DeleteByStatus::singleIdentifier() const
{
    return singleIdentifierForICStatus(m_variants);
}

template<typename Visitor>
void DeleteByStatus::visitAggregateImpl(Visitor& visitor)
{
    for (DeleteByVariant& variant : m_variants)
        variant.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(DeleteByStatus);

template<typename Visitor>
void DeleteByStatus::markIfCheap(Visitor& visitor)
{
    for (DeleteByVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

template void DeleteByStatus::markIfCheap(AbstractSlotVisitor&);
template void DeleteByStatus::markIfCheap(SlotVisitor&);

bool DeleteByStatus::finalize(VM& vm)
{
    for (auto& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

void DeleteByStatus::dump(PrintStream& out) const
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
