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
#include "GetByIdStatus.h"

#include "CodeBlock.h"
#include "JSCInlines.h"
#include "JSScope.h"
#include "LLIntData.h"
#include "LowLevelInterpreter.h"
#include "PolymorphicGetByIdList.h"
#include <wtf/ListDump.h>

namespace JSC {

bool GetByIdStatus::appendVariant(const GetByIdVariant& variant)
{
    for (unsigned i = 0; i < m_variants.size(); ++i) {
        if (m_variants[i].structureSet().overlaps(variant.structureSet()))
            return false;
    }
    m_variants.append(variant);
    return true;
}

#if ENABLE(DFG_JIT)
bool GetByIdStatus::hasExitSite(const ConcurrentJITLocker& locker, CodeBlock* profiledBlock, unsigned bytecodeIndex, ExitingJITType jitType)
{
    return profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCache, jitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCacheWatchpoint, jitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadWeakConstantCache, jitType))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadWeakConstantCacheWatchpoint, jitType));
}
#endif

GetByIdStatus GetByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, StringImpl* uid)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
#if ENABLE(LLINT)
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;
    
    if (instruction[0].u.opcode == LLInt::getOpcode(op_get_array_length))
        return GetByIdStatus(NoInformation, false);

    Structure* structure = instruction[4].u.structure.get();
    if (!structure)
        return GetByIdStatus(NoInformation, false);

    if (structure->takesSlowPathInDFGForImpureProperty())
        return GetByIdStatus(NoInformation, false);

    unsigned attributesIgnored;
    JSCell* specificValue;
    PropertyOffset offset = structure->getConcurrently(
        *profiledBlock->vm(), uid, attributesIgnored, specificValue);
    if (structure->isDictionary())
        specificValue = 0;
    if (!isValidOffset(offset))
        return GetByIdStatus(NoInformation, false);
    
    return GetByIdStatus(Simple, false, GetByIdVariant(StructureSet(structure), offset, specificValue));
#else
    return GetByIdStatus(NoInformation, false);
#endif
}

bool GetByIdStatus::computeForChain(CodeBlock* profiledBlock, StringImpl* uid, PassRefPtr<IntendedStructureChain> passedChain)
{
#if ENABLE(JIT)
    RefPtr<IntendedStructureChain> chain = passedChain;
    
    // Validate the chain. If the chain is invalid, then currently the best thing
    // we can do is to assume that TakesSlow is true. In the future, it might be
    // worth exploring reifying the structure chain from the structure we've got
    // instead of using the one from the cache, since that will do the right things
    // if the structure chain has changed. But that may be harder, because we may
    // then end up having a different type of access altogether. And it currently
    // does not appear to be worth it to do so -- effectively, the heuristic we
    // have now is that if the structure chain has changed between when it was
    // cached on in the baseline JIT and when the DFG tried to inline the access,
    // then we fall back on a polymorphic access.
    if (!chain->isStillValid())
        return false;

    if (chain->head()->takesSlowPathInDFGForImpureProperty())
        return false;
    size_t chainSize = chain->size();
    for (size_t i = 0; i < chainSize; i++) {
        if (chain->at(i)->takesSlowPathInDFGForImpureProperty())
            return false;
    }

    JSObject* currentObject = chain->terminalPrototype();
    Structure* currentStructure = chain->last();
    
    ASSERT_UNUSED(currentObject, currentObject);
    
    unsigned attributesIgnored;
    JSCell* specificValue;
    
    PropertyOffset offset = currentStructure->getConcurrently(
        *profiledBlock->vm(), uid, attributesIgnored, specificValue);
    if (currentStructure->isDictionary())
        specificValue = 0;
    if (!isValidOffset(offset))
        return false;
    
    return appendVariant(GetByIdVariant(StructureSet(chain->head()), offset, specificValue, chain));
#else // ENABLE(JIT)
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(uid);
    UNUSED_PARAM(passedChain);
    UNREACHABLE_FOR_PLATFORM();
    return false;
#endif // ENABLE(JIT)
}

GetByIdStatus GetByIdStatus::computeFor(CodeBlock* profiledBlock, StubInfoMap& map, unsigned bytecodeIndex, StringImpl* uid)
{
    ConcurrentJITLocker locker(profiledBlock->m_lock);

    GetByIdStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfo(
        locker, profiledBlock, map.get(CodeOrigin(bytecodeIndex)), uid);
    
    if (!result.takesSlowPath()
        && (hasExitSite(locker, profiledBlock, bytecodeIndex)
            || profiledBlock->likelyToTakeSlowCase(bytecodeIndex)))
        return GetByIdStatus(TakesSlowPath, true);
#else
    UNUSED_PARAM(map);
#endif

    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex, uid);
    
    return result;
}

#if ENABLE(JIT)
GetByIdStatus GetByIdStatus::computeForStubInfo(
    const ConcurrentJITLocker&, CodeBlock* profiledBlock, StructureStubInfo* stubInfo,
    StringImpl* uid)
{
    if (!stubInfo || !stubInfo->seen)
        return GetByIdStatus(NoInformation);
    
    if (stubInfo->resetByGC)
        return GetByIdStatus(TakesSlowPath, true);

    PolymorphicGetByIdList* list = 0;
    if (stubInfo->accessType == access_get_by_id_list) {
        list = stubInfo->u.getByIdList.list;
        for (unsigned i = 0; i < list->size(); ++i) {
            if (list->at(i).doesCalls())
                return GetByIdStatus(MakesCalls, true);
        }
    }
    
    // Finally figure out if we can derive an access strategy.
    GetByIdStatus result;
    result.m_state = Simple;
    result.m_wasSeenInJIT = true; // This is interesting for bytecode dumping only.
    switch (stubInfo->accessType) {
    case access_unset:
        return GetByIdStatus(NoInformation);
        
    case access_get_by_id_self: {
        Structure* structure = stubInfo->u.getByIdSelf.baseObjectStructure.get();
        if (structure->takesSlowPathInDFGForImpureProperty())
            return GetByIdStatus(TakesSlowPath, true);
        unsigned attributesIgnored;
        JSCell* specificValue;
        GetByIdVariant variant;
        variant.m_offset = structure->getConcurrently(
            *profiledBlock->vm(), uid, attributesIgnored, specificValue);
        if (!isValidOffset(variant.m_offset))
            return GetByIdStatus(TakesSlowPath, true);
        
        if (structure->isDictionary())
            specificValue = 0;
        
        variant.m_structureSet.add(structure);
        variant.m_specificValue = JSValue(specificValue);
        result.appendVariant(variant);
        return result;
    }
        
    case access_get_by_id_list: {
        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            ASSERT(!list->at(listIndex).doesCalls());
            
            Structure* structure = list->at(listIndex).structure();
            if (structure->takesSlowPathInDFGForImpureProperty())
                return GetByIdStatus(TakesSlowPath, true);
            
            if (list->at(listIndex).chain()) {
                RefPtr<IntendedStructureChain> chain = adoptRef(new IntendedStructureChain(
                    profiledBlock, structure, list->at(listIndex).chain(),
                    list->at(listIndex).chainCount()));
                if (!result.computeForChain(profiledBlock, uid, chain))
                    return GetByIdStatus(TakesSlowPath, true);
                continue;
            }
            
            unsigned attributesIgnored;
            JSCell* specificValue;
            PropertyOffset myOffset = structure->getConcurrently(
                *profiledBlock->vm(), uid, attributesIgnored, specificValue);
            if (structure->isDictionary())
                specificValue = 0;
            
            if (!isValidOffset(myOffset))
                return GetByIdStatus(TakesSlowPath, true);

            bool found = false;
            for (unsigned variantIndex = 0; variantIndex < result.m_variants.size(); ++variantIndex) {
                GetByIdVariant& variant = result.m_variants[variantIndex];
                if (variant.m_chain)
                    continue;
                
                if (variant.m_offset != myOffset)
                    continue;

                found = true;
                if (variant.m_structureSet.contains(structure))
                    break;
                
                if (variant.m_specificValue != JSValue(specificValue))
                    variant.m_specificValue = JSValue();
                
                variant.m_structureSet.add(structure);
                break;
            }
            
            if (found)
                continue;
            
            if (!result.appendVariant(GetByIdVariant(StructureSet(structure), myOffset, specificValue)))
                return GetByIdStatus(TakesSlowPath, true);
        }
        
        return result;
    }
        
    case access_get_by_id_chain: {
        if (!stubInfo->u.getByIdChain.isDirect)
            return GetByIdStatus(MakesCalls, true);
        RefPtr<IntendedStructureChain> chain = adoptRef(new IntendedStructureChain(
            profiledBlock,
            stubInfo->u.getByIdChain.baseObjectStructure.get(),
            stubInfo->u.getByIdChain.chain.get(),
            stubInfo->u.getByIdChain.count));
        if (result.computeForChain(profiledBlock, uid, chain))
            return result;
        return GetByIdStatus(TakesSlowPath, true);
    }
        
    default:
        return GetByIdStatus(TakesSlowPath, true);
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return GetByIdStatus();
}
#endif // ENABLE(JIT)

GetByIdStatus GetByIdStatus::computeFor(
    CodeBlock* profiledBlock, CodeBlock* dfgBlock, StubInfoMap& baselineMap,
    StubInfoMap& dfgMap, CodeOrigin codeOrigin, StringImpl* uid)
{
#if ENABLE(DFG_JIT)
    if (dfgBlock) {
        GetByIdStatus result;
        {
            ConcurrentJITLocker locker(dfgBlock->m_lock);
            result = computeForStubInfo(locker, dfgBlock, dfgMap.get(codeOrigin), uid);
        }
        
        if (result.takesSlowPath())
            return result;
    
        {
            ConcurrentJITLocker locker(profiledBlock->m_lock);
            if (hasExitSite(locker, profiledBlock, codeOrigin.bytecodeIndex, ExitFromFTL))
                return GetByIdStatus(TakesSlowPath, true);
        }
        
        if (result.isSet())
            return result;
    }
#else
    UNUSED_PARAM(dfgBlock);
    UNUSED_PARAM(dfgMap);
#endif

    return computeFor(profiledBlock, baselineMap, codeOrigin.bytecodeIndex, uid);
}

GetByIdStatus GetByIdStatus::computeFor(VM& vm, Structure* structure, StringImpl* uid)
{
    // For now we only handle the super simple self access case. We could handle the
    // prototype case in the future.
    
    if (!structure)
        return GetByIdStatus(TakesSlowPath);

    if (toUInt32FromStringImpl(uid) != PropertyName::NotAnIndex)
        return GetByIdStatus(TakesSlowPath);
    
    if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
        return GetByIdStatus(TakesSlowPath);
    
    if (!structure->propertyAccessesAreCacheable())
        return GetByIdStatus(TakesSlowPath);

    unsigned attributes;
    JSCell* specificValue;
    PropertyOffset offset = structure->getConcurrently(vm, uid, attributes, specificValue);
    if (!isValidOffset(offset))
        return GetByIdStatus(TakesSlowPath); // It's probably a prototype lookup. Give up on life for now, even though we could totally be way smarter about it.
    if (attributes & Accessor)
        return GetByIdStatus(MakesCalls);
    if (structure->isDictionary())
        specificValue = 0;
    return GetByIdStatus(
        Simple, false, GetByIdVariant(StructureSet(structure), offset, specificValue));
}

void GetByIdStatus::dump(PrintStream& out) const
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
    case MakesCalls:
        out.print("MakesCalls");
        break;
    }
    out.print(", ", listDump(m_variants), ", seenInJIT = ", m_wasSeenInJIT, ")");
}

} // namespace JSC

