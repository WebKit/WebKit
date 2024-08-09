/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#include "InByStatus.h"

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "ICStatusUtils.h"
#include "InlineCacheCompiler.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InByStatus);

bool InByStatus::appendVariant(const InByVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

void InByStatus::shrinkToFit()
{
    m_variants.shrinkToFit();
}

#if ENABLE(JIT)
InByStatus InByStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, ExitFlag didExit, CallLinkStatus::ExitSiteData callExitSiteData, CodeOrigin codeOrigin)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);

    InByStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfoWithoutExitSiteFeedback(locker, profiledBlock, map.get(CodeOrigin(bytecodeIndex)).stubInfo, callExitSiteData, codeOrigin);

    if (!result.takesSlowPath() && didExit)
        return InByStatus(TakesSlowPath);
#else
    UNUSED_PARAM(map);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(didExit);
    UNUSED_PARAM(callExitSiteData);
#endif

    return result;
}

InByStatus InByStatus::computeFor(
    CodeBlock* profiledBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& contextStack, CodeOrigin codeOrigin)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    CallLinkStatus::ExitSiteData callExitSiteData = CallLinkStatus::computeExitSiteData(profiledBlock, bytecodeIndex);
    ExitFlag didExit = hasBadCacheExitSite(profiledBlock, bytecodeIndex);
    
    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);
        
        auto bless = [&] (const InByStatus& result) -> InByStatus {
            if (!context->isInlined(codeOrigin)) {
                InByStatus baselineResult = computeFor(profiledBlock, baselineMap, bytecodeIndex, didExit, callExitSiteData, codeOrigin);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return InByStatus(TakesSlowPath);
            return result;
        };
        
#if ENABLE(DFG_JIT)
        if (status.stubInfo) {
            InByStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfoWithoutExitSiteFeedback(locker, profiledBlock, status.stubInfo, callExitSiteData, codeOrigin);
            }
            if (result.isSet())
                return bless(result);
        }
#endif
        
        if (status.inStatus)
            return bless(*status.inStatus);
    }
    
    return computeFor(profiledBlock, baselineMap, bytecodeIndex, didExit, callExitSiteData, codeOrigin);
}
#endif // ENABLE(JIT)

#if ENABLE(DFG_JIT)
InByStatus InByStatus::computeForStubInfoWithoutExitSiteFeedback(const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, CallLinkStatus::ExitSiteData callExitSiteData, CodeOrigin)
{
    StubInfoSummary summary = StructureStubInfo::summary(profiledBlock->vm(), stubInfo);
    if (!isInlineable(summary))
        return InByStatus(summary);
    
    // Finally figure out if we can derive an access strategy.
    InByStatus result;
    result.m_state = Simple;
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        return InByStatus(NoInformation);

    case CacheType::InByIdSelf: {
        Structure* structure = stubInfo->inlineAccessBaseStructure();
        if (structure->takesSlowPathInDFGForImpureProperty())
            return InByStatus(TakesSlowPath);
        CacheableIdentifier identifier = stubInfo->identifier();
        UniquedStringImpl* uid = identifier.uid();
        RELEASE_ASSERT(uid);
        InByVariant variant(WTFMove(identifier));
        unsigned attributes;
        variant.m_offset = structure->getConcurrently(uid, attributes);
        if (!isValidOffset(variant.m_offset))
            return InByStatus(TakesSlowPath);
        if (attributes & PropertyAttribute::CustomAccessorOrValue)
            return InByStatus(TakesSlowPath);

        variant.m_structureSet.add(structure);
        bool didAppend = result.appendVariant(variant);
        ASSERT_UNUSED(didAppend, didAppend);
        return result;
    }

    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->m_stub.get();
        if (list->size() == 1) {
            const AccessCase& access = list->at(0);
            switch (access.type()) {
            case AccessCase::InMegamorphic:
            case AccessCase::IndexedMegamorphicIn: {
                if (!stubInfo->tookSlowPath)
                    return InByStatus(Megamorphic);
                break;
            }
            case AccessCase::ProxyObjectIn:
            case AccessCase::IndexedProxyObjectIn: {
                auto status = InByStatus(InByStatus::ProxyObject);
                auto callLinkStatus = makeUnique<CallLinkStatus>();
                if (CallLinkInfo* callLinkInfo = stubInfo->callLinkInfoAt(locker, 0, access))
                    *callLinkStatus = CallLinkStatus::computeFor(locker, profiledBlock, *callLinkInfo, callExitSiteData);
                status.appendVariant(InByVariant(access.identifier(), { }, invalidOffset, { }, WTFMove(callLinkStatus)));
                return status;
            }
            default:
                break;
            }
        }

        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);
            if (access.viaGlobalProxy())
                return InByStatus(TakesSlowPath);

            if (access.usesPolyProto())
                return InByStatus(TakesSlowPath);

            if (!access.requiresIdentifierNameMatch()) {
                // FIXME: We could use this for indexed loads in the future. This is pretty solid profiling
                // information, and probably better than ArrayProfile when it's available.
                // https://bugs.webkit.org/show_bug.cgi?id=204215
                return InByStatus(TakesSlowPath);
            }

            Structure* structure = access.structure();
            if (!structure) {
                // The null structure cases arise due to array.length. We have no way of creating a
                // InByVariant for those, and we don't really have to since the DFG handles those
                // cases in FixupPhase using value profiling. That's a bit awkward - we shouldn't
                // have to use value profiling to discover something that the AccessCase could have
                // told us. But, it works well enough. So, our only concern here is to not
                // crash on null structure.
                return InByStatus(TakesSlowPath);
            }

            ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(structure, access.conditionSet(), access.uid());
            switch (complexGetStatus.kind()) {
            case ComplexGetStatus::ShouldSkip:
                continue;

            case ComplexGetStatus::TakesSlowPath:
                return InByStatus(TakesSlowPath);

            case ComplexGetStatus::Inlineable: {
                switch (access.type()) {
                case AccessCase::InHit:
                case AccessCase::InMiss:
                    break;
                default:
                    return InByStatus(TakesSlowPath);
                }

                InByVariant variant(
                    access.identifier(), StructureSet(structure), complexGetStatus.offset(),
                    complexGetStatus.conditionSet());

                if (!result.appendVariant(variant))
                    return InByStatus(TakesSlowPath);
                break;
            }
            }
        }

        result.shrinkToFit();
        return result;
    }

    default:
        return InByStatus(TakesSlowPath);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return InByStatus();
}
#endif

void InByStatus::merge(const InByStatus& other)
{
    if (other.m_state == NoInformation)
        return;
    
    switch (m_state) {
    case NoInformation:
        *this = other;
        return;

    case Megamorphic:
        if (m_state != other.m_state) {
            if (other.m_state == Simple) {
                *this = other;
                return;
            }
            *this = InByStatus(TakesSlowPath);
            return;
        }
        return;

    case Simple:
    case ProxyObject:
        if (m_state != other.m_state) {
            *this = InByStatus(TakesSlowPath);
            return;
        }
        for (const InByVariant& otherVariant : other.m_variants) {
            if (!appendVariant(otherVariant)) {
                *this = InByStatus(TakesSlowPath);
                return;
            }
        }
        shrinkToFit();
        return;

    case TakesSlowPath:
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void InByStatus::filter(const StructureSet& structureSet)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, structureSet);
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

template<typename Visitor>
void InByStatus::visitAggregateImpl(Visitor& visitor)
{
    for (InByVariant& variant : m_variants)
        variant.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(InByStatus);

template<typename Visitor>
void InByStatus::markIfCheap(Visitor& visitor)
{
    for (InByVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

template void InByStatus::markIfCheap(AbstractSlotVisitor&);
template void InByStatus::markIfCheap(SlotVisitor&);

bool InByStatus::finalize(VM& vm)
{
    for (InByVariant& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

CacheableIdentifier InByStatus::singleIdentifier() const
{
    return singleIdentifierForICStatus(m_variants);
}

void InByStatus::dump(PrintStream& out) const
{
    out.print("(");
    switch (m_state) {
    case NoInformation:
        out.print("NoInformation");
        break;
    case Simple:
        out.print("Simple");
        break;
    case ProxyObject:
        out.print("ProxyObject");
        break;
    case Megamorphic:
        out.print("Megamorphic");
        break;
    case TakesSlowPath:
        out.print("TakesSlowPath");
        break;
    }
    out.print(", ", listDump(m_variants), ")");
}

} // namespace JSC

