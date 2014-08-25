/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "AccessorCallJITStubRoutine.h"
#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "LLIntData.h"
#include "LowLevelInterpreter.h"
#include "JSCInlines.h"
#include "PolymorphicPutByIdList.h"
#include "Structure.h"
#include "StructureChain.h"
#include <wtf/ListDump.h>

namespace JSC {

bool PutByIdStatus::appendVariant(const PutByIdVariant& variant)
{
    for (unsigned i = 0; i < m_variants.size(); ++i) {
        if (m_variants[i].attemptToMerge(variant))
            return true;
    }
    for (unsigned i = 0; i < m_variants.size(); ++i) {
        if (m_variants[i].oldStructure().overlaps(variant.oldStructure()))
            return false;
    }
    m_variants.append(variant);
    return true;
}

#if ENABLE(DFG_JIT)
bool PutByIdStatus::hasExitSite(const ConcurrentJITLocker& locker, CodeBlock* profiledBlock, unsigned bytecodeIndex, ExitingJITType exitType)
{
    return profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCache, exitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadConstantCache, exitType));
    
}
#endif

PutByIdStatus PutByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, StringImpl* uid)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;

    Structure* structure = instruction[4].u.structure.get();
    if (!structure)
        return PutByIdStatus(NoInformation);
    
    if (instruction[0].u.opcode == LLInt::getOpcode(op_put_by_id)
        || instruction[0].u.opcode == LLInt::getOpcode(op_put_by_id_out_of_line)) {
        PropertyOffset offset = structure->getConcurrently(*profiledBlock->vm(), uid);
        if (!isValidOffset(offset))
            return PutByIdStatus(NoInformation);
        
        return PutByIdVariant::replace(structure, offset);
    }
    
    ASSERT(structure->transitionWatchpointSetHasBeenInvalidated());
    
    ASSERT(instruction[0].u.opcode == LLInt::getOpcode(op_put_by_id_transition_direct)
        || instruction[0].u.opcode == LLInt::getOpcode(op_put_by_id_transition_normal)
        || instruction[0].u.opcode == LLInt::getOpcode(op_put_by_id_transition_direct_out_of_line)
        || instruction[0].u.opcode == LLInt::getOpcode(op_put_by_id_transition_normal_out_of_line));
    
    Structure* newStructure = instruction[6].u.structure.get();
    StructureChain* chain = instruction[7].u.structureChain.get();
    ASSERT(newStructure);
    ASSERT(chain);
    
    PropertyOffset offset = newStructure->getConcurrently(*profiledBlock->vm(), uid);
    if (!isValidOffset(offset))
        return PutByIdStatus(NoInformation);
    
    RefPtr<IntendedStructureChain> intendedChain;
    if (chain)
        intendedChain = adoptRef(new IntendedStructureChain(profiledBlock, structure, chain));
    
    return PutByIdVariant::transition(structure, newStructure, intendedChain.get(), offset);
}

PutByIdStatus PutByIdStatus::computeFor(CodeBlock* profiledBlock, StubInfoMap& map, unsigned bytecodeIndex, StringImpl* uid)
{
    ConcurrentJITLocker locker(profiledBlock->m_lock);
    
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
#if ENABLE(DFG_JIT)
    if (profiledBlock->likelyToTakeSlowCase(bytecodeIndex)
        || hasExitSite(locker, profiledBlock, bytecodeIndex))
        return PutByIdStatus(TakesSlowPath);
    
    StructureStubInfo* stubInfo = map.get(CodeOrigin(bytecodeIndex));
    PutByIdStatus result = computeForStubInfo(
        locker, profiledBlock, stubInfo, uid,
        CallLinkStatus::computeExitSiteData(locker, profiledBlock, bytecodeIndex));
    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex, uid);
    
    return result;
#else // ENABLE(JIT)
    UNUSED_PARAM(map);
    return PutByIdStatus(NoInformation);
#endif // ENABLE(JIT)
}

#if ENABLE(JIT)
PutByIdStatus PutByIdStatus::computeForStubInfo(
    const ConcurrentJITLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo,
    StringImpl* uid, CallLinkStatus::ExitSiteData callExitSiteData)
{
    if (!stubInfo || !stubInfo->seen)
        return PutByIdStatus();
    
    switch (stubInfo->accessType) {
    case access_unset:
        // If the JIT saw it but didn't optimize it, then assume that this takes slow path.
        return PutByIdStatus(TakesSlowPath);
        
    case access_put_by_id_replace: {
        PropertyOffset offset =
            stubInfo->u.putByIdReplace.baseObjectStructure->getConcurrently(
                *profiledBlock->vm(), uid);
        if (isValidOffset(offset)) {
            return PutByIdVariant::replace(
                stubInfo->u.putByIdReplace.baseObjectStructure.get(), offset);
        }
        return PutByIdStatus(TakesSlowPath);
    }
        
    case access_put_by_id_transition_normal:
    case access_put_by_id_transition_direct: {
        ASSERT(stubInfo->u.putByIdTransition.previousStructure->transitionWatchpointSetHasBeenInvalidated());
        PropertyOffset offset = 
            stubInfo->u.putByIdTransition.structure->getConcurrently(
                *profiledBlock->vm(), uid);
        if (isValidOffset(offset)) {
            RefPtr<IntendedStructureChain> chain;
            if (stubInfo->u.putByIdTransition.chain) {
                chain = adoptRef(new IntendedStructureChain(
                    profiledBlock, stubInfo->u.putByIdTransition.previousStructure.get(),
                    stubInfo->u.putByIdTransition.chain.get()));
            }
            return PutByIdVariant::transition(
                stubInfo->u.putByIdTransition.previousStructure.get(),
                stubInfo->u.putByIdTransition.structure.get(),
                chain.get(), offset);
        }
        return PutByIdStatus(TakesSlowPath);
    }
        
    case access_put_by_id_list: {
        PolymorphicPutByIdList* list = stubInfo->u.putByIdList.list;
        
        PutByIdStatus result;
        result.m_state = Simple;
        
        State slowPathState = TakesSlowPath;
        for (unsigned i = 0; i < list->size(); ++i) {
            const PutByIdAccess& access = list->at(i);
            
            switch (access.type()) {
            case PutByIdAccess::Setter:
            case PutByIdAccess::CustomSetter:
                slowPathState = MakesCalls;
                break;
            default:
                break;
            }
        }
        
        for (unsigned i = 0; i < list->size(); ++i) {
            const PutByIdAccess& access = list->at(i);
            
            PutByIdVariant variant;
            
            switch (access.type()) {
            case PutByIdAccess::Replace: {
                Structure* structure = access.structure();
                PropertyOffset offset = structure->getConcurrently(*profiledBlock->vm(), uid);
                if (!isValidOffset(offset))
                    return PutByIdStatus(slowPathState);
                variant = PutByIdVariant::replace(structure, offset);
                break;
            }
                
            case PutByIdAccess::Transition: {
                PropertyOffset offset =
                    access.newStructure()->getConcurrently(*profiledBlock->vm(), uid);
                if (!isValidOffset(offset))
                    return PutByIdStatus(slowPathState);
                RefPtr<IntendedStructureChain> chain;
                if (access.chain()) {
                    chain = adoptRef(new IntendedStructureChain(
                        profiledBlock, access.oldStructure(), access.chain()));
                    if (!chain->isStillValid())
                        continue;
                }
                variant = PutByIdVariant::transition(
                    access.oldStructure(), access.newStructure(), chain.get(), offset);
                break;
            }
                
            case PutByIdAccess::Setter: {
                Structure* structure = access.structure();
                
                ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(
                    profiledBlock, structure, access.chain(), access.chainCount(), uid);
                
                switch (complexGetStatus.kind()) {
                case ComplexGetStatus::ShouldSkip:
                    continue;
                    
                case ComplexGetStatus::TakesSlowPath:
                    return PutByIdStatus(slowPathState);
                    
                case ComplexGetStatus::Inlineable: {
                    AccessorCallJITStubRoutine* stub = static_cast<AccessorCallJITStubRoutine*>(
                        access.stubRoutine());
                    std::unique_ptr<CallLinkStatus> callLinkStatus =
                        std::make_unique<CallLinkStatus>(
                            CallLinkStatus::computeFor(
                                locker, profiledBlock, *stub->m_callLinkInfo, callExitSiteData));
                    
                    variant = PutByIdVariant::setter(
                        structure, complexGetStatus.offset(), complexGetStatus.chain(),
                        std::move(callLinkStatus));
                } }
                break;
            }
                
            case PutByIdAccess::CustomSetter:
                return PutByIdStatus(MakesCalls);

            default:
                return PutByIdStatus(slowPathState);
            }
            
            if (!result.appendVariant(variant))
                return PutByIdStatus(slowPathState);
        }
        
        return result;
    }
        
    default:
        return PutByIdStatus(TakesSlowPath);
    }
}
#endif

PutByIdStatus PutByIdStatus::computeFor(CodeBlock* baselineBlock, CodeBlock* dfgBlock, StubInfoMap& baselineMap, StubInfoMap& dfgMap, CodeOrigin codeOrigin, StringImpl* uid)
{
#if ENABLE(DFG_JIT)
    if (dfgBlock) {
        CallLinkStatus::ExitSiteData exitSiteData;
        {
            ConcurrentJITLocker locker(baselineBlock->m_lock);
            if (hasExitSite(locker, baselineBlock, codeOrigin.bytecodeIndex, ExitFromFTL))
                return PutByIdStatus(TakesSlowPath);
            exitSiteData = CallLinkStatus::computeExitSiteData(
                locker, baselineBlock, codeOrigin.bytecodeIndex, ExitFromFTL);
        }
            
        PutByIdStatus result;
        {
            ConcurrentJITLocker locker(dfgBlock->m_lock);
            result = computeForStubInfo(
                locker, dfgBlock, dfgMap.get(codeOrigin), uid, exitSiteData);
        }
        
        // We use TakesSlowPath in some cases where the stub was unset. That's weird and
        // it would be better not to do that. But it means that we have to defend
        // ourselves here.
        if (result.isSimple())
            return result;
    }
#else
    UNUSED_PARAM(dfgBlock);
    UNUSED_PARAM(dfgMap);
#endif

    return computeFor(baselineBlock, baselineMap, codeOrigin.bytecodeIndex, uid);
}

PutByIdStatus PutByIdStatus::computeFor(VM& vm, JSGlobalObject* globalObject, const StructureSet& set, StringImpl* uid, bool isDirect)
{
    if (toUInt32FromStringImpl(uid) != PropertyName::NotAnIndex)
        return PutByIdStatus(TakesSlowPath);

    if (set.isEmpty())
        return PutByIdStatus();
    
    PutByIdStatus result;
    result.m_state = Simple;
    for (unsigned i = 0; i < set.size(); ++i) {
        Structure* structure = set[i];
        
        if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
            return PutByIdStatus(TakesSlowPath);

        if (!structure->propertyAccessesAreCacheable())
            return PutByIdStatus(TakesSlowPath);
    
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(vm, uid, attributes);
        if (isValidOffset(offset)) {
            if (attributes & CustomAccessor)
                return PutByIdStatus(MakesCalls);

            if (attributes & (Accessor | ReadOnly))
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
            
            if (!result.appendVariant(PutByIdVariant::replace(structure, offset)))
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
        if (structure->typeInfo().type() == StringType)
            return PutByIdStatus(TakesSlowPath);
    
        RefPtr<IntendedStructureChain> chain;
        if (!isDirect) {
            chain = adoptRef(new IntendedStructureChain(globalObject, structure));
        
            // If the prototype chain has setters or read-only properties, then give up.
            if (chain->mayInterceptStoreTo(vm, uid))
                return PutByIdStatus(TakesSlowPath);
        
            // If the prototype chain hasn't been normalized (i.e. there are proxies or dictionaries)
            // then give up. The dictionary case would only happen if this structure has not been
            // used in an optimized put_by_id transition. And really the only reason why we would
            // bail here is that I don't really feel like having the optimizing JIT go and flatten
            // dictionaries if we have evidence to suggest that those objects were never used as
            // prototypes in a cacheable prototype access - i.e. there's a good chance that some of
            // the other checks below will fail.
            if (structure->isProxy() || !chain->isNormalized())
                return PutByIdStatus(TakesSlowPath);
        }
    
        // We only optimize if there is already a structure that the transition is cached to.
        Structure* transition = Structure::addPropertyTransitionToExistingStructureConcurrently(structure, uid, 0, offset);
        if (!transition)
            return PutByIdStatus(TakesSlowPath);
        ASSERT(isValidOffset(offset));
    
        bool didAppend = result.appendVariant(
            PutByIdVariant::transition(structure, transition, chain.get(), offset));
        if (!didAppend)
            return PutByIdStatus(TakesSlowPath);
    }
    
    return result;
}

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

