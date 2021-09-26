/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "PutByStatus.h"

#include "BytecodeStructs.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "GetterSetterAccessCase.h"
#include "ICStatusUtils.h"
#include "PolymorphicAccess.h"
#include "StructureInlines.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>

namespace JSC {

bool PutByStatus::appendVariant(const PutByVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

void PutByStatus::shrinkToFit()
{
    m_variants.shrinkToFit();
}

PutByStatus PutByStatus::computeFromLLInt(CodeBlock* profiledBlock, BytecodeIndex bytecodeIndex)
{
    VM& vm = profiledBlock->vm();
    
    auto instruction = profiledBlock->instructions().at(bytecodeIndex.offset());

    switch (instruction->opcodeID()) {
    case op_put_by_id:
        break;
    case op_put_by_val:
    case op_put_by_val_direct:
        return PutByStatus(NoInformation);
    case op_put_private_name:
        // We do no have a code retrieving LLInt information for `op_put_private_name`.
        // We can add support for it if this is required in future changes, since we have
        // IC implemented for this operation on LLInt.
        return PutByStatus(NoInformation);
    default: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    }

    auto bytecode = instruction->as<OpPutById>();
    auto& metadata = bytecode.metadata(profiledBlock);
    const Identifier* identifier = &(profiledBlock->identifier(bytecode.m_property));
    UniquedStringImpl* uid = identifier->impl();

    StructureID structureID = metadata.m_oldStructureID;
    if (!structureID)
        return PutByStatus(NoInformation);
    
    Structure* structure = vm.heap.structureIDTable().get(structureID);

    StructureID newStructureID = metadata.m_newStructureID;
    if (!newStructureID) {
        PropertyOffset offset = structure->getConcurrently(uid);
        if (!isValidOffset(offset))
            return PutByStatus(NoInformation);
        
        return PutByVariant::replace(nullptr, structure, offset);
    }

    Structure* newStructure = vm.heap.structureIDTable().get(newStructureID);
    
    ASSERT(structure->transitionWatchpointSetHasBeenInvalidated());
    
    PropertyOffset offset = newStructure->getConcurrently(uid);
    if (!isValidOffset(offset))
        return PutByStatus(NoInformation);
    
    ObjectPropertyConditionSet conditionSet;
    if (!(bytecode.m_flags.isDirect())) {
        conditionSet =
            generateConditionsForPropertySetterMissConcurrently(
                vm, profiledBlock->globalObject(), structure, uid);
        if (!conditionSet.isValid())
            return PutByStatus(NoInformation);
    }
    
    return PutByVariant::transition(nullptr, structure, newStructure, conditionSet, offset);
}

#if ENABLE(JIT)
PutByStatus::PutByStatus(StubInfoSummary summary, StructureStubInfo& stubInfo)
{
    switch (summary) {
    case StubInfoSummary::NoInformation:
        m_state = NoInformation;
        return;
    case StubInfoSummary::Simple:
    case StubInfoSummary::MakesCalls:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    case StubInfoSummary::TakesSlowPath:
        m_state = stubInfo.tookSlowPath ? ObservedTakesSlowPath : LikelyTakesSlowPath;
        return;
    case StubInfoSummary::TakesSlowPathAndMakesCalls:
        m_state = stubInfo.tookSlowPath ? ObservedSlowPathAndMakesCalls : MakesCalls;
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

PutByStatus PutByStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, ExitFlag didExit, CallLinkStatus::ExitSiteData callExitSiteData)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);
    
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#if ENABLE(DFG_JIT)
    if (didExit)
        return PutByStatus(LikelyTakesSlowPath);
    
    StructureStubInfo* stubInfo = map.get(CodeOrigin(bytecodeIndex)).stubInfo;
    PutByStatus result = computeForStubInfo(
        locker, profiledBlock, stubInfo, callExitSiteData);
    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex);
    
    return result;
#else // ENABLE(JIT)
    UNUSED_PARAM(map);
    UNUSED_PARAM(didExit);
    UNUSED_PARAM(callExitSiteData);
    return PutByStatus(NoInformation);
#endif // ENABLE(JIT)
}

PutByStatus PutByStatus::computeForStubInfo(const ConcurrentJSLocker& locker, CodeBlock* baselineBlock, StructureStubInfo* stubInfo, CodeOrigin codeOrigin)
{
    return computeForStubInfo(
        locker, baselineBlock, stubInfo,
        CallLinkStatus::computeExitSiteData(baselineBlock, codeOrigin.bytecodeIndex()));
}

PutByStatus PutByStatus::computeForStubInfo(const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, CallLinkStatus::ExitSiteData callExitSiteData)
{
    StubInfoSummary summary = StructureStubInfo::summary(profiledBlock->vm(), stubInfo);
    if (!isInlineable(summary))
        return PutByStatus(summary, *stubInfo);
    
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        // This means that we attempted to cache but failed for some reason.
        return PutByStatus(JSC::slowVersion(summary), *stubInfo);
        
    case CacheType::PutByIdReplace: {
        CacheableIdentifier identifier = stubInfo->identifier();
        UniquedStringImpl* uid = identifier.uid();
        RELEASE_ASSERT(uid);
        PropertyOffset offset =
            stubInfo->m_inlineAccessBaseStructure->getConcurrently(uid);
        if (isValidOffset(offset))
            return PutByVariant::replace(WTFMove(identifier), stubInfo->m_inlineAccessBaseStructure.get(), offset);
        return PutByStatus(JSC::slowVersion(summary), *stubInfo);
    }
        
    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->u.stub;
        
        PutByStatus result;
        result.m_state = Simple;
        
        for (unsigned i = 0; i < list->size(); ++i) {
            const AccessCase& access = list->at(i);
            if (access.viaProxy())
                return PutByStatus(JSC::slowVersion(summary), *stubInfo);
            if (access.usesPolyProto())
                return PutByStatus(JSC::slowVersion(summary), *stubInfo);
            
            switch (access.type()) {
            case AccessCase::Replace: {
                Structure* structure = access.structure();
                PropertyOffset offset = structure->getConcurrently(access.uid());
                if (!isValidOffset(offset))
                    return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                auto variant = PutByVariant::replace(access.identifier(), structure, offset);
                if (!result.appendVariant(variant))
                    return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                break;
            }
                
            case AccessCase::Transition: {
                PropertyOffset offset = access.newStructure()->getConcurrently(access.uid());
                if (!isValidOffset(offset))
                    return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                ObjectPropertyConditionSet conditionSet = access.conditionSet();
                if (!conditionSet.structuresEnsureValidity())
                    return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                auto variant = PutByVariant::transition(access.identifier(), access.structure(), access.newStructure(), conditionSet, offset);
                if (!result.appendVariant(variant))
                    return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                break;
            }
                
            case AccessCase::Setter: {
                Structure* structure = access.structure();
                
                ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(structure, access.conditionSet(), access.uid());
                
                switch (complexGetStatus.kind()) {
                case ComplexGetStatus::ShouldSkip:
                    continue;
                    
                case ComplexGetStatus::TakesSlowPath:
                    return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                    
                case ComplexGetStatus::Inlineable: {
                    std::unique_ptr<CallLinkStatus> callLinkStatus =
                        makeUnique<CallLinkStatus>();
                    if (CallLinkInfo* callLinkInfo = access.as<GetterSetterAccessCase>().callLinkInfo()) {
                        *callLinkStatus = CallLinkStatus::computeFor(
                            locker, profiledBlock, *callLinkInfo, callExitSiteData);
                    }
                    
                    auto variant = PutByVariant::setter(access.identifier(), structure, complexGetStatus.offset(), complexGetStatus.conditionSet(), WTFMove(callLinkStatus));
                    if (!result.appendVariant(variant))
                        return PutByStatus(JSC::slowVersion(summary), *stubInfo);
                }
                }
                break;
            }
                
            case AccessCase::CustomValueSetter:
            case AccessCase::CustomAccessorSetter:
                return PutByStatus(MakesCalls);

            default:
                return PutByStatus(JSC::slowVersion(summary), *stubInfo);
            }
        }
        
        result.shrinkToFit();
        return result;
    }
        
    default:
        return PutByStatus(JSC::slowVersion(summary), *stubInfo);
    }
}

PutByStatus PutByStatus::computeFor(CodeBlock* baselineBlock, ICStatusMap& baselineMap, ICStatusContextStack& contextStack, CodeOrigin codeOrigin)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    CallLinkStatus::ExitSiteData callExitSiteData = CallLinkStatus::computeExitSiteData(baselineBlock, bytecodeIndex);
    ExitFlag didExit = hasBadCacheExitSite(baselineBlock, bytecodeIndex);

    for (ICStatusContext* context : contextStack) {
        ICStatus status = context->get(codeOrigin);
        
        auto bless = [&] (const PutByStatus& result) -> PutByStatus {
            if (!context->isInlined(codeOrigin)) {
                PutByStatus baselineResult = computeFor(
                    baselineBlock, baselineMap, bytecodeIndex, didExit,
                    callExitSiteData);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };
        
        if (status.stubInfo) {
            PutByStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfo(
                    locker, context->optimizedCodeBlock, status.stubInfo, callExitSiteData);
            }
            if (result.isSet())
                return bless(result);
        }
        
        if (status.putStatus)
            return bless(*status.putStatus);
    }
    
    return computeFor(baselineBlock, baselineMap, bytecodeIndex, didExit, callExitSiteData);
}

PutByStatus PutByStatus::computeFor(JSGlobalObject* globalObject, const StructureSet& set, CacheableIdentifier identifier, bool isDirect, PrivateFieldPutKind privateFieldPutKind)
{
    UniquedStringImpl* uid = identifier.uid();
    if (parseIndex(*uid))
        return PutByStatus(LikelyTakesSlowPath);

    if (set.isEmpty())
        return PutByStatus();
    
    VM& vm = globalObject->vm();
    PutByStatus result;
    result.m_state = Simple;
    for (unsigned i = 0; i < set.size(); ++i) {
        Structure* structure = set[i];
        
        if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
            return PutByStatus(LikelyTakesSlowPath);

        if (!structure->propertyAccessesAreCacheable())
            return PutByStatus(LikelyTakesSlowPath);
    
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(uid, attributes);
        if (isValidOffset(offset)) {
            // We can't have a valid offset for structures on `PutPrivateNameById` define mode
            // since it means we are redefining a private field. In such case, we need to take 
            // slow path to throw exception.
            if (privateFieldPutKind.isDefine())
                return PutByStatus(LikelyTakesSlowPath);

            if (attributes & PropertyAttribute::CustomAccessorOrValue)
                return PutByStatus(MakesCalls);

            if (attributes & (PropertyAttribute::Accessor | PropertyAttribute::ReadOnly))
                return PutByStatus(LikelyTakesSlowPath);
            
            WatchpointSet* replaceSet = structure->propertyReplacementWatchpointSet(offset);
            if (!replaceSet || replaceSet->isStillValid()) {
                // When this executes, it'll create, and fire, this replacement watchpoint set.
                // That means that  this has probably never executed or that something fishy is
                // going on. Also, we cannot create or fire the watchpoint set from the concurrent
                // JIT thread, so even if we wanted to do this, we'd need to have a lazy thingy.
                // So, better leave this alone and take slow path.
                return PutByStatus(LikelyTakesSlowPath);
            }

            PutByVariant variant = PutByVariant::replace(identifier, structure, offset);
            if (!result.appendVariant(variant))
                return PutByStatus(LikelyTakesSlowPath);
            continue;
        }

        // We can have a case with PutPrivateNameById in set mode and it
        // should never cause a structure transition because it means we are
        // trying to store in a not installed private field. We need to take
        // slow path to throw excpetion if it ever gets executed.
        if (privateFieldPutKind.isSet())
            return PutByStatus(LikelyTakesSlowPath);

        // Our hypothesis is that we're doing a transition. Before we prove that this is really
        // true, we want to do some sanity checks.
    
        // Don't cache put transitions on dictionaries.
        if (structure->isDictionary())
            return PutByStatus(LikelyTakesSlowPath);

        // If the structure corresponds to something that isn't an object, then give up, since
        // we don't want to be adding properties to strings.
        if (!structure->typeInfo().isObject())
            return PutByStatus(LikelyTakesSlowPath);
    
        ObjectPropertyConditionSet conditionSet;
        if (!isDirect) {
            ASSERT(privateFieldPutKind.isNone());
            conditionSet = generateConditionsForPropertySetterMissConcurrently(
                vm, globalObject, structure, uid);
            if (!conditionSet.isValid())
                return PutByStatus(LikelyTakesSlowPath);
        }
    
        // We only optimize if there is already a structure that the transition is cached to.
        Structure* transition =
            Structure::addPropertyTransitionToExistingStructureConcurrently(structure, uid, 0, offset);
        if (!transition)
            return PutByStatus(LikelyTakesSlowPath);
        ASSERT(isValidOffset(offset));
    
        bool didAppend = result.appendVariant(PutByVariant::transition(identifier, structure, transition, conditionSet, offset));
        if (!didAppend)
            return PutByStatus(LikelyTakesSlowPath);
    }
    
    result.shrinkToFit();
    return result;
}
#endif

bool PutByStatus::makesCalls() const
{
    switch (m_state) {
    case NoInformation:
    case LikelyTakesSlowPath:
    case ObservedTakesSlowPath:
        return false;
    case MakesCalls:
    case ObservedSlowPathAndMakesCalls:
        return true;
    case Simple: {
        for (unsigned i = m_variants.size(); i--;) {
            if (m_variants[i].makesCalls())
                return true;
        }
        return false;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

PutByStatus PutByStatus::slowVersion() const
{
    if (observedStructureStubInfoSlowPath())
        return PutByStatus(makesCalls() ? ObservedSlowPathAndMakesCalls : ObservedTakesSlowPath);
    return PutByStatus(makesCalls() ? MakesCalls : LikelyTakesSlowPath);
}

CacheableIdentifier PutByStatus::singleIdentifier() const
{
    return singleIdentifierForICStatus(m_variants);
}

template<typename Visitor>
void PutByStatus::visitAggregateImpl(Visitor& visitor)
{
    for (PutByVariant& variant : m_variants)
        variant.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(PutByStatus);

template<typename Visitor>
void PutByStatus::markIfCheap(Visitor& visitor)
{
    for (PutByVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

template void PutByStatus::markIfCheap(AbstractSlotVisitor&);
template void PutByStatus::markIfCheap(SlotVisitor&);

bool PutByStatus::finalize(VM& vm)
{
    for (PutByVariant& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

void PutByStatus::merge(const PutByStatus& other)
{
    if (other.m_state == NoInformation)
        return;

    auto mergeSlow = [&] () {
        if (observedStructureStubInfoSlowPath() || other.observedStructureStubInfoSlowPath())
            *this = PutByStatus((makesCalls() || other.makesCalls()) ? ObservedSlowPathAndMakesCalls : ObservedTakesSlowPath);
        else
            *this = PutByStatus((makesCalls() || other.makesCalls()) ? MakesCalls : LikelyTakesSlowPath);
    };

    switch (m_state) {
    case NoInformation:
        *this = other;
        return;
        
    case Simple:
        if (other.m_state != Simple)
            return mergeSlow();
        
        for (const PutByVariant& other : other.m_variants) {
            if (!appendVariant(other))
                return mergeSlow();
        }
        shrinkToFit();
        return;
        
    case LikelyTakesSlowPath:
    case ObservedTakesSlowPath:
    case MakesCalls:
    case ObservedSlowPathAndMakesCalls:
        return mergeSlow();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void PutByStatus::filter(const StructureSet& set)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, set);
    for (PutByVariant& variant : m_variants)
        variant.fixTransitionToReplaceIfNecessary();
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

void PutByStatus::dump(PrintStream& out) const
{
    switch (m_state) {
    case NoInformation:
        out.print("(NoInformation)");
        return;
    case Simple:
        out.print("(", listDump(m_variants), ")");
        return;
    case LikelyTakesSlowPath:
        out.print("LikelyTakesSlowPath");
        return;
    case ObservedTakesSlowPath:
        out.print("ObservedTakesSlowPath");
        return;
    case MakesCalls:
        out.print("MakesCalls");
        return;
    case ObservedSlowPathAndMakesCalls:
        out.print("ObservedSlowPathAndMakesCalls");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

