/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "PutByIdStatus.h"

#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "GetterSetterAccessCase.h"
#include "ICStatusUtils.h"
#include "LLIntData.h"
#include "LowLevelInterpreter.h"
#include "JSCInlines.h"
#include "PolymorphicAccess.h"
#include "Structure.h"
#include "StructureChain.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>

namespace JSC {

bool PutByIdStatus::appendVariant(const PutByIdVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

PutByIdStatus PutByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, UniquedStringImpl* uid)
{
    VM& vm = *profiledBlock->vm();
    
    auto instruction = profiledBlock->instructions().at(bytecodeIndex);
    auto bytecode = instruction->as<OpPutById>();
    auto& metadata = bytecode.metadata(profiledBlock);

    StructureID structureID = metadata.m_oldStructure;
    if (!structureID)
        return PutByIdStatus(NoInformation);
    
    Structure* structure = vm.heap.structureIDTable().get(structureID);

    StructureID newStructureID = metadata.m_newStructure;
    if (!newStructureID) {
        PropertyOffset offset = structure->getConcurrently(uid);
        if (!isValidOffset(offset))
            return PutByIdStatus(NoInformation);
        
        return PutByIdVariant::replace(structure, offset);
    }

    Structure* newStructure = vm.heap.structureIDTable().get(newStructureID);
    
    ASSERT(structure->transitionWatchpointSetHasBeenInvalidated());
    
    PropertyOffset offset = newStructure->getConcurrently(uid);
    if (!isValidOffset(offset))
        return PutByIdStatus(NoInformation);
    
    ObjectPropertyConditionSet conditionSet;
    if (!(bytecode.m_flags & PutByIdIsDirect)) {
        conditionSet =
            generateConditionsForPropertySetterMissConcurrently(
                vm, profiledBlock->globalObject(), structure, uid);
        if (!conditionSet.isValid())
            return PutByIdStatus(NoInformation);
    }
    
    return PutByIdVariant::transition(
        structure, newStructure, conditionSet, offset);
}

#if ENABLE(JIT)
PutByIdStatus PutByIdStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, unsigned bytecodeIndex, UniquedStringImpl* uid, ExitFlag didExit, CallLinkStatus::ExitSiteData callExitSiteData)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);
    
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
#if ENABLE(DFG_JIT)
    if (didExit)
        return PutByIdStatus(TakesSlowPath);
    
    StructureStubInfo* stubInfo = map.get(CodeOrigin(bytecodeIndex)).stubInfo;
    PutByIdStatus result = computeForStubInfo(
        locker, profiledBlock, stubInfo, uid, callExitSiteData);
    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex, uid);
    
    return result;
#else // ENABLE(JIT)
    UNUSED_PARAM(map);
    UNUSED_PARAM(didExit);
    UNUSED_PARAM(callExitSiteData);
    return PutByIdStatus(NoInformation);
#endif // ENABLE(JIT)
}

PutByIdStatus PutByIdStatus::computeForStubInfo(const ConcurrentJSLocker& locker, CodeBlock* baselineBlock, StructureStubInfo* stubInfo, CodeOrigin codeOrigin, UniquedStringImpl* uid)
{
    return computeForStubInfo(
        locker, baselineBlock, stubInfo, uid,
        CallLinkStatus::computeExitSiteData(baselineBlock, codeOrigin.bytecodeIndex));
}

PutByIdStatus PutByIdStatus::computeForStubInfo(
    const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo,
    UniquedStringImpl* uid, CallLinkStatus::ExitSiteData callExitSiteData)
{
    StubInfoSummary summary = StructureStubInfo::summary(stubInfo);
    if (!isInlineable(summary))
        return PutByIdStatus(summary);
    
    switch (stubInfo->cacheType) {
    case CacheType::Unset:
        // This means that we attempted to cache but failed for some reason.
        return PutByIdStatus(JSC::slowVersion(summary));
        
    case CacheType::PutByIdReplace: {
        PropertyOffset offset =
            stubInfo->u.byIdSelf.baseObjectStructure->getConcurrently(uid);
        if (isValidOffset(offset)) {
            return PutByIdVariant::replace(
                stubInfo->u.byIdSelf.baseObjectStructure.get(), offset);
        }
        return PutByIdStatus(JSC::slowVersion(summary));
    }
        
    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->u.stub;
        
        PutByIdStatus result;
        result.m_state = Simple;
        
        for (unsigned i = 0; i < list->size(); ++i) {
            const AccessCase& access = list->at(i);
            if (access.viaProxy())
                return PutByIdStatus(JSC::slowVersion(summary));
            if (access.usesPolyProto())
                return PutByIdStatus(JSC::slowVersion(summary));
            
            PutByIdVariant variant;
            
            switch (access.type()) {
            case AccessCase::Replace: {
                Structure* structure = access.structure();
                PropertyOffset offset = structure->getConcurrently(uid);
                if (!isValidOffset(offset))
                    return PutByIdStatus(JSC::slowVersion(summary));
                variant = PutByIdVariant::replace(
                    structure, offset);
                break;
            }
                
            case AccessCase::Transition: {
                PropertyOffset offset =
                    access.newStructure()->getConcurrently(uid);
                if (!isValidOffset(offset))
                    return PutByIdStatus(JSC::slowVersion(summary));
                ObjectPropertyConditionSet conditionSet = access.conditionSet();
                if (!conditionSet.structuresEnsureValidity())
                    return PutByIdStatus(JSC::slowVersion(summary));
                variant = PutByIdVariant::transition(
                    access.structure(), access.newStructure(), conditionSet, offset);
                break;
            }
                
            case AccessCase::Setter: {
                Structure* structure = access.structure();
                
                ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(
                    structure, access.conditionSet(), uid);
                
                switch (complexGetStatus.kind()) {
                case ComplexGetStatus::ShouldSkip:
                    continue;
                    
                case ComplexGetStatus::TakesSlowPath:
                    return PutByIdStatus(JSC::slowVersion(summary));
                    
                case ComplexGetStatus::Inlineable: {
                    std::unique_ptr<CallLinkStatus> callLinkStatus =
                        std::make_unique<CallLinkStatus>();
                    if (CallLinkInfo* callLinkInfo = access.as<GetterSetterAccessCase>().callLinkInfo()) {
                        *callLinkStatus = CallLinkStatus::computeFor(
                            locker, profiledBlock, *callLinkInfo, callExitSiteData);
                    }
                    
                    variant = PutByIdVariant::setter(
                        structure, complexGetStatus.offset(), complexGetStatus.conditionSet(),
                        WTFMove(callLinkStatus));
                } }
                break;
            }
                
            case AccessCase::CustomValueSetter:
            case AccessCase::CustomAccessorSetter:
                return PutByIdStatus(MakesCalls);

            default:
                return PutByIdStatus(JSC::slowVersion(summary));
            }
            
            if (!result.appendVariant(variant))
                return PutByIdStatus(JSC::slowVersion(summary));
        }
        
        return result;
    }
        
    default:
        return PutByIdStatus(JSC::slowVersion(summary));
    }
}

PutByIdStatus PutByIdStatus::computeFor(CodeBlock* baselineBlock, ICStatusMap& baselineMap, ICStatusContextStack& contextStack, CodeOrigin codeOrigin, UniquedStringImpl* uid)
{
    CallLinkStatus::ExitSiteData callExitSiteData =
        CallLinkStatus::computeExitSiteData(baselineBlock, codeOrigin.bytecodeIndex);
    ExitFlag didExit = hasBadCacheExitSite(baselineBlock, codeOrigin.bytecodeIndex);

    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);
        
        auto bless = [&] (const PutByIdStatus& result) -> PutByIdStatus {
            if (!context->isInlined(codeOrigin)) {
                PutByIdStatus baselineResult = computeFor(
                    baselineBlock, baselineMap, codeOrigin.bytecodeIndex, uid, didExit,
                    callExitSiteData);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };
        
        if (status.stubInfo) {
            PutByIdStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfo(
                    locker, context->optimizedCodeBlock, status.stubInfo, uid, callExitSiteData);
            }
            if (result.isSet())
                return bless(result);
        }
        
        if (status.putStatus)
            return bless(*status.putStatus);
    }
    
    return computeFor(baselineBlock, baselineMap, codeOrigin.bytecodeIndex, uid, didExit, callExitSiteData);
}

PutByIdStatus PutByIdStatus::computeFor(JSGlobalObject* globalObject, const StructureSet& set, UniquedStringImpl* uid, bool isDirect)
{
    if (parseIndex(*uid))
        return PutByIdStatus(TakesSlowPath);

    if (set.isEmpty())
        return PutByIdStatus();
    
    VM& vm = globalObject->vm();
    PutByIdStatus result;
    result.m_state = Simple;
    for (unsigned i = 0; i < set.size(); ++i) {
        Structure* structure = set[i];
        
        if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
            return PutByIdStatus(TakesSlowPath);

        if (!structure->propertyAccessesAreCacheable())
            return PutByIdStatus(TakesSlowPath);
    
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(uid, attributes);
        if (isValidOffset(offset)) {
            if (attributes & PropertyAttribute::CustomAccessorOrValue)
                return PutByIdStatus(MakesCalls);

            if (attributes & (PropertyAttribute::Accessor | PropertyAttribute::ReadOnly))
                return PutByIdStatus(TakesSlowPath);
            
            WatchpointSet* replaceSet = structure->propertyReplacementWatchpointSet(offset);
            if (!replaceSet || replaceSet->isStillValid()) {
                // When this executes, it'll create, and fire, this replacement watchpoint set.
                // That means that  this has probably never executed or that something fishy is
                // going on. Also, we cannot create or fire the watchpoint set from the concurrent
                // JIT thread, so even if we wanted to do this, we'd need to have a lazy thingy.
                // So, better leave this alone and take slow path.
                return PutByIdStatus(TakesSlowPath);
            }

            PutByIdVariant variant =
                PutByIdVariant::replace(structure, offset);
            if (!result.appendVariant(variant))
                return PutByIdStatus(TakesSlowPath);
            continue;
        }
    
        // Our hypothesis is that we're doing a transition. Before we prove that this is really
        // true, we want to do some sanity checks.
    
        // Don't cache put transitions on dictionaries.
        if (structure->isDictionary())
            return PutByIdStatus(TakesSlowPath);

        // If the structure corresponds to something that isn't an object, then give up, since
        // we don't want to be adding properties to strings.
        if (!structure->typeInfo().isObject())
            return PutByIdStatus(TakesSlowPath);
    
        ObjectPropertyConditionSet conditionSet;
        if (!isDirect) {
            conditionSet = generateConditionsForPropertySetterMissConcurrently(
                vm, globalObject, structure, uid);
            if (!conditionSet.isValid())
                return PutByIdStatus(TakesSlowPath);
        }
    
        // We only optimize if there is already a structure that the transition is cached to.
        Structure* transition =
            Structure::addPropertyTransitionToExistingStructureConcurrently(structure, uid, 0, offset);
        if (!transition)
            return PutByIdStatus(TakesSlowPath);
        ASSERT(isValidOffset(offset));
    
        bool didAppend = result.appendVariant(
            PutByIdVariant::transition(
                structure, transition, conditionSet, offset));
        if (!didAppend)
            return PutByIdStatus(TakesSlowPath);
    }
    
    return result;
}
#endif

bool PutByIdStatus::makesCalls() const
{
    if (m_state == MakesCalls)
        return true;
    
    if (m_state != Simple)
        return false;
    
    for (unsigned i = m_variants.size(); i--;) {
        if (m_variants[i].makesCalls())
            return true;
    }
    
    return false;
}

PutByIdStatus PutByIdStatus::slowVersion() const
{
    return PutByIdStatus(makesCalls() ? MakesCalls : TakesSlowPath);
}

void PutByIdStatus::markIfCheap(SlotVisitor& visitor)
{
    for (PutByIdVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

bool PutByIdStatus::finalize()
{
    for (PutByIdVariant& variant : m_variants) {
        if (!variant.finalize())
            return false;
    }
    return true;
}

void PutByIdStatus::merge(const PutByIdStatus& other)
{
    if (other.m_state == NoInformation)
        return;
    
    auto mergeSlow = [&] () {
        *this = PutByIdStatus((makesCalls() || other.makesCalls()) ? MakesCalls : TakesSlowPath);
    };
    
    switch (m_state) {
    case NoInformation:
        *this = other;
        return;
        
    case Simple:
        if (other.m_state != Simple)
            return mergeSlow();
        
        for (const PutByIdVariant& other : other.m_variants) {
            if (!appendVariant(other))
                return mergeSlow();
        }
        return;
        
    case TakesSlowPath:
    case MakesCalls:
        return mergeSlow();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void PutByIdStatus::filter(const StructureSet& set)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, set);
    for (PutByIdVariant& variant : m_variants)
        variant.fixTransitionToReplaceIfNecessary();
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

void PutByIdStatus::dump(PrintStream& out) const
{
    switch (m_state) {
    case NoInformation:
        out.print("(NoInformation)");
        return;
        
    case Simple:
        out.print("(", listDump(m_variants), ")");
        return;
        
    case TakesSlowPath:
        out.print("(TakesSlowPath)");
        return;
    case MakesCalls:
        out.print("(MakesCalls)");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

