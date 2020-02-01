/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InByIdStatus.h"

#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "ICStatusUtils.h"
#include "JSCInlines.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>

namespace JSC {

bool InByIdStatus::appendVariant(const InByIdVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

#if ENABLE(JIT)
InByIdStatus InByIdStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, UniquedStringImpl* uid, ExitFlag didExit)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);

    InByIdStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfoWithoutExitSiteFeedback(locker, profiledBlock->vm(), map.get(CodeOrigin(bytecodeIndex)).stubInfo, uid);

    if (!result.takesSlowPath() && didExit)
        return InByIdStatus(TakesSlowPath);
#else
    UNUSED_PARAM(map);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
    UNUSED_PARAM(didExit);
#endif

    return result;
}

InByIdStatus InByIdStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, UniquedStringImpl* uid)
{
    return computeFor(profiledBlock, map, bytecodeIndex, uid, hasBadCacheExitSite(profiledBlock, bytecodeIndex));
}

InByIdStatus InByIdStatus::computeFor(
    CodeBlock* profiledBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& contextStack, CodeOrigin codeOrigin, UniquedStringImpl* uid)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    ExitFlag didExit = hasBadCacheExitSite(profiledBlock, bytecodeIndex);
    
    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);
        
        auto bless = [&] (const InByIdStatus& result) -> InByIdStatus {
            if (!context->isInlined(codeOrigin)) {
                InByIdStatus baselineResult = computeFor(
                    profiledBlock, baselineMap, bytecodeIndex, uid, didExit);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return InByIdStatus(TakesSlowPath);
            return result;
        };
        
#if ENABLE(DFG_JIT)
        if (status.stubInfo) {
            InByIdStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfoWithoutExitSiteFeedback(locker, profiledBlock->vm(), status.stubInfo, uid);
            }
            if (result.isSet())
                return bless(result);
        }
#endif
        
        if (status.inStatus)
            return bless(*status.inStatus);
    }
    
    return computeFor(profiledBlock, baselineMap, bytecodeIndex, uid, didExit);
}
#endif // ENABLE(JIT)

#if ENABLE(DFG_JIT)
InByIdStatus InByIdStatus::computeForStubInfo(const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, CodeOrigin codeOrigin, UniquedStringImpl* uid)
{
    InByIdStatus result = InByIdStatus::computeForStubInfoWithoutExitSiteFeedback(locker, profiledBlock->vm(), stubInfo, uid);

    if (!result.takesSlowPath() && hasBadCacheExitSite(profiledBlock, codeOrigin.bytecodeIndex()))
        return InByIdStatus(TakesSlowPath);
    return result;
}

InByIdStatus InByIdStatus::computeForStubInfoWithoutExitSiteFeedback(const ConcurrentJSLocker&, VM& vm, StructureStubInfo* stubInfo, UniquedStringImpl* uid)
{
    StubInfoSummary summary = StructureStubInfo::summary(vm, stubInfo);
    if (!isInlineable(summary))
        return InByIdStatus(summary);
    
    // Finally figure out if we can derive an access strategy.
    InByIdStatus result;
    result.m_state = Simple;
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        return InByIdStatus(NoInformation);

    case CacheType::InByIdSelf: {
        Structure* structure = stubInfo->u.byIdSelf.baseObjectStructure.get();
        if (structure->takesSlowPathInDFGForImpureProperty())
            return InByIdStatus(TakesSlowPath);
        unsigned attributes;
        InByIdVariant variant;
        variant.m_offset = structure->getConcurrently(uid, attributes);
        if (!isValidOffset(variant.m_offset))
            return InByIdStatus(TakesSlowPath);
        if (attributes & PropertyAttribute::CustomAccessorOrValue)
            return InByIdStatus(TakesSlowPath);

        variant.m_structureSet.add(structure);
        bool didAppend = result.appendVariant(variant);
        ASSERT_UNUSED(didAppend, didAppend);
        return result;
    }

    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->u.stub;
        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);
            if (access.viaProxy())
                return InByIdStatus(TakesSlowPath);

            if (access.usesPolyProto())
                return InByIdStatus(TakesSlowPath);

            Structure* structure = access.structure();
            if (!structure) {
                // The null structure cases arise due to array.length. We have no way of creating a
                // InByIdVariant for those, and we don't really have to since the DFG handles those
                // cases in FixupPhase using value profiling. That's a bit awkward - we shouldn't
                // have to use value profiling to discover something that the AccessCase could have
                // told us. But, it works well enough. So, our only concern here is to not
                // crash on null structure.
                return InByIdStatus(TakesSlowPath);
            }

            ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(structure, access.conditionSet(), uid);
            switch (complexGetStatus.kind()) {
            case ComplexGetStatus::ShouldSkip:
                continue;

            case ComplexGetStatus::TakesSlowPath:
                return InByIdStatus(TakesSlowPath);

            case ComplexGetStatus::Inlineable: {
                switch (access.type()) {
                case AccessCase::InHit:
                case AccessCase::InMiss:
                    break;
                default:
                    return InByIdStatus(TakesSlowPath);
                }

                InByIdVariant variant(
                    StructureSet(structure), complexGetStatus.offset(),
                    complexGetStatus.conditionSet());

                if (!result.appendVariant(variant))
                    return InByIdStatus(TakesSlowPath);
                break;
            }
            }
        }

        return result;
    }

    default:
        return InByIdStatus(TakesSlowPath);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return InByIdStatus();
}
#endif

void InByIdStatus::merge(const InByIdStatus& other)
{
    if (other.m_state == NoInformation)
        return;
    
    switch (m_state) {
    case NoInformation:
        *this = other;
        return;
        
    case Simple:
        if (other.m_state != Simple) {
            *this = InByIdStatus(TakesSlowPath);
            return;
        }
        for (const InByIdVariant& otherVariant : other.m_variants) {
            if (!appendVariant(otherVariant)) {
                *this = InByIdStatus(TakesSlowPath);
                return;
            }
        }
        return;
        
    case TakesSlowPath:
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void InByIdStatus::filter(const StructureSet& structureSet)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, structureSet);
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

void InByIdStatus::markIfCheap(SlotVisitor& visitor)
{
    for (InByIdVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

bool InByIdStatus::finalize(VM& vm)
{
    for (InByIdVariant& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

void InByIdStatus::dump(PrintStream& out) const
{
    out.print("(");
    switch (m_state) {
    case NoInformation:
        out.print("NoInformation");
        break;
    case Simple:
        out.print("Simple");
        break;
    case TakesSlowPath:
        out.print("TakesSlowPath");
        break;
    }
    out.print(", ", listDump(m_variants), ")");
}

} // namespace JSC

