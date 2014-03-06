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

#include "CodeBlock.h"
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
        if (m_variants[i].oldStructure() == variant.oldStructure())
            return false;
    }
    m_variants.append(variant);
    return true;
}

#if ENABLE(DFG_JIT)
bool PutByIdStatus::hasExitSite(const ConcurrentJITLocker& locker, CodeBlock* profiledBlock, unsigned bytecodeIndex, ExitingJITType exitType)
{
    return profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCache, exitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCacheWatchpoint, exitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadWeakConstantCache, exitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadWeakConstantCacheWatchpoint, exitType));
    
}
#endif

PutByIdStatus PutByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, StringImpl* uid)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
#if ENABLE(LLINT)
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;

    Structure* structure = instruction[4].u.structure.get();
    if (!structure)
        return PutByIdStatus(NoInformation);
    
    if (instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id)
        || instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id_out_of_line)) {
        PropertyOffset offset = structure->getConcurrently(*profiledBlock->vm(), uid);
        if (!isValidOffset(offset))
            return PutByIdStatus(NoInformation);
        
        return PutByIdVariant::replace(structure, offset);
    }
    
    ASSERT(structure->transitionWatchpointSetHasBeenInvalidated());
    
    ASSERT(instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id_transition_direct)
           || instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id_transition_normal)
           || instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id_transition_direct_out_of_line)
           || instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id_transition_normal_out_of_line));
    
    Structure* newStructure = instruction[6].u.structure.get();
    StructureChain* chain = instruction[7].u.structureChain.get();
    ASSERT(newStructure);
    ASSERT(chain);
    
    PropertyOffset offset = newStructure->getConcurrently(*profiledBlock->vm(), uid);
    if (!isValidOffset(offset))
        return PutByIdStatus(NoInformation);
    
    return PutByIdVariant::transition(
        structure, newStructure,
        chain ? adoptRef(new IntendedStructureChain(profiledBlock, structure, chain)) : 0,
        offset);
#else
    return PutByIdStatus(NoInformation);
#endif
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
    PutByIdStatus result = computeForStubInfo(locker, profiledBlock, stubInfo, uid);
    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex, uid);
    
    return result;
#else // ENABLE(JIT)
    UNUSED_PARAM(map);
    return PutByIdStatus(NoInformation);
#endif // ENABLE(JIT)
}

#if ENABLE(JIT)
PutByIdStatus PutByIdStatus::computeForStubInfo(const ConcurrentJITLocker&, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, StringImpl* uid)
{
    if (!stubInfo || !stubInfo->seen)
        return PutByIdStatus();
    
    if (stubInfo->resetByGC)
        return PutByIdStatus(TakesSlowPath);

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
            return PutByIdVariant::transition(
                stubInfo->u.putByIdTransition.previousStructure.get(),
                stubInfo->u.putByIdTransition.structure.get(),
                stubInfo->u.putByIdTransition.chain ? adoptRef(new IntendedStructureChain(
                    profiledBlock, stubInfo->u.putByIdTransition.previousStructure.get(),
                    stubInfo->u.putByIdTransition.chain.get())) : 0,
                offset);
        }
        return PutByIdStatus(TakesSlowPath);
    }
        
    case access_put_by_id_list: {
        PolymorphicPutByIdList* list = stubInfo->u.putByIdList.list;
        
        PutByIdStatus result;
        result.m_state = Simple;
        
        for (unsigned i = 0; i < list->size(); ++i) {
            const PutByIdAccess& access = list->at(i);
            
            switch (access.type()) {
            case PutByIdAccess::Replace: {
                Structure* structure = access.structure();
                PropertyOffset offset = structure->getConcurrently(*profiledBlock->vm(), uid);
                if (!isValidOffset(offset))
                    return PutByIdStatus(TakesSlowPath);
                if (!result.appendVariant(PutByIdVariant::replace(structure, offset)))
                    return PutByIdStatus(TakesSlowPath);
                break;
            }
                
            case PutByIdAccess::Transition: {
                PropertyOffset offset =
                    access.newStructure()->getConcurrently(*profiledBlock->vm(), uid);
                if (!isValidOffset(offset))
                    return PutByIdStatus(TakesSlowPath);
                bool ok = result.appendVariant(PutByIdVariant::transition(
                    access.oldStructure(), access.newStructure(),
                    access.chain() ? adoptRef(new IntendedStructureChain(
                        profiledBlock, access.oldStructure(), access.chain())) : 0,
                    offset));
                if (!ok)
                    return PutByIdStatus(TakesSlowPath);
                break;
            }
            case PutByIdAccess::CustomSetter:
                return PutByIdStatus(MakesCalls);

            default:
                return PutByIdStatus(TakesSlowPath);
            }
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
        {
            ConcurrentJITLocker locker(baselineBlock->m_lock);
            if (hasExitSite(locker, baselineBlock, codeOrigin.bytecodeIndex, ExitFromFTL))
                return PutByIdStatus(TakesSlowPath);
        }
            
        PutByIdStatus result;
        {
            ConcurrentJITLocker locker(dfgBlock->m_lock);
            result = computeForStubInfo(locker, dfgBlock, dfgMap.get(codeOrigin), uid);
        }
        
        if (result.isSet())
            return result;
    }
#else
    UNUSED_PARAM(dfgBlock);
    UNUSED_PARAM(dfgMap);
#endif

    return computeFor(baselineBlock, baselineMap, codeOrigin.bytecodeIndex, uid);
}

PutByIdStatus PutByIdStatus::computeFor(VM& vm, JSGlobalObject* globalObject, Structure* structure, StringImpl* uid, bool isDirect)
{
    if (toUInt32FromStringImpl(uid) != PropertyName::NotAnIndex)
        return PutByIdStatus(TakesSlowPath);

    if (!structure)
        return PutByIdStatus(TakesSlowPath);
    
    if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
        return PutByIdStatus(TakesSlowPath);

    if (!structure->propertyAccessesAreCacheable())
        return PutByIdStatus(TakesSlowPath);
    
    unsigned attributes;
    JSCell* specificValue;
    PropertyOffset offset = structure->getConcurrently(vm, uid, attributes, specificValue);
    if (isValidOffset(offset)) {
        if (attributes & CustomAccessor)
            return PutByIdStatus(MakesCalls);

        if (attributes & (Accessor | ReadOnly))
            return PutByIdStatus(TakesSlowPath);
        if (specificValue) {
            // We need the PutById slow path to verify that we're storing the right value into
            // the specialized slot.
            return PutByIdStatus(TakesSlowPath);
        }
        return PutByIdVariant::replace(structure, offset);
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
        if (!chain->isNormalized())
            return PutByIdStatus(TakesSlowPath);
    }
    
    // We only optimize if there is already a structure that the transition is cached to.
    // Among other things, this allows us to guard against a transition with a specific
    // value.
    //
    // - If we're storing a value that could be specific: this would only be a problem if
    //   the existing transition did have a specific value already, since if it didn't,
    //   then we would behave "as if" we were not storing a specific value. If it did
    //   have a specific value, then we'll know - the fact that we pass 0 for
    //   specificValue will tell us.
    //
    // - If we're not storing a value that could be specific: again, this would only be a
    //   problem if the existing transition did have a specific value, which we check for
    //   by passing 0 for the specificValue.
    Structure* transition = Structure::addPropertyTransitionToExistingStructureConcurrently(structure, uid, 0, 0, offset);
    if (!transition)
        return PutByIdStatus(TakesSlowPath); // This occurs in bizarre cases only. See above.
    ASSERT(!transition->transitionDidInvolveSpecificValue());
    ASSERT(isValidOffset(offset));
    
    return PutByIdVariant::transition(structure, transition, chain.release(), offset);
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

