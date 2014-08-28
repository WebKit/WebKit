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

#include "AccessorCallJITStubRoutine.h"
#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "JSCInlines.h"
#include "JSScope.h"
#include "LLIntData.h"
#include "LowLevelInterpreter.h"
#include "PolymorphicGetByIdList.h"
#include <wtf/ListDump.h>

namespace JSC {

bool GetByIdStatus::appendVariant(const GetByIdVariant& variant)
{
    // Attempt to merge this variant with an already existing variant.
    for (unsigned i = 0; i < m_variants.size(); ++i) {
        if (m_variants[i].attemptToMerge(variant))
            return true;
    }
    
    // Make sure there is no overlap. We should have pruned out opportunities for
    // overlap but it's possible that an inline cache got into a weird state. We are
    // defensive and bail if we detect crazy.
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
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadConstantCache, jitType));
}
#endif

GetByIdStatus GetByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, StringImpl* uid)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(uid);
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;
    
    if (instruction[0].u.opcode == LLInt::getOpcode(op_get_array_length))
        return GetByIdStatus(NoInformation, false);

    Structure* structure = instruction[4].u.structure.get();
    if (!structure)
        return GetByIdStatus(NoInformation, false);

    if (structure->takesSlowPathInDFGForImpureProperty())
        return GetByIdStatus(NoInformation, false);

    unsigned attributesIgnored;
    PropertyOffset offset = structure->getConcurrently(
        *profiledBlock->vm(), uid, attributesIgnored);
    if (!isValidOffset(offset))
        return GetByIdStatus(NoInformation, false);
    
    return GetByIdStatus(Simple, false, GetByIdVariant(StructureSet(structure), offset));
}

GetByIdStatus GetByIdStatus::computeFor(CodeBlock* profiledBlock, StubInfoMap& map, unsigned bytecodeIndex, StringImpl* uid)
{
    ConcurrentJITLocker locker(profiledBlock->m_lock);

    GetByIdStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfo(
        locker, profiledBlock, map.get(CodeOrigin(bytecodeIndex)), uid,
        CallLinkStatus::computeExitSiteData(locker, profiledBlock, bytecodeIndex));
    
    if (!result.takesSlowPath()
        && (hasExitSite(locker, profiledBlock, bytecodeIndex)
            || profiledBlock->likelyToTakeSlowCase(bytecodeIndex)))
        return GetByIdStatus(result.makesCalls() ? MakesCalls : TakesSlowPath, true);
#else
    UNUSED_PARAM(map);
#endif

    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex, uid);
    
    return result;
}

#if ENABLE(JIT)
GetByIdStatus GetByIdStatus::computeForStubInfo(
    const ConcurrentJITLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, StringImpl* uid,
    CallLinkStatus::ExitSiteData callExitSiteData)
{
    if (!stubInfo || !stubInfo->seen)
        return GetByIdStatus(NoInformation);
    
    PolymorphicGetByIdList* list = 0;
    State slowPathState = TakesSlowPath;
    if (stubInfo->accessType == access_get_by_id_list) {
        list = stubInfo->u.getByIdList.list;
        for (unsigned i = 0; i < list->size(); ++i) {
            const GetByIdAccess& access = list->at(i);
            if (access.doesCalls())
                slowPathState = MakesCalls;
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
            return GetByIdStatus(slowPathState, true);
        unsigned attributesIgnored;
        GetByIdVariant variant;
        variant.m_offset = structure->getConcurrently(
            *profiledBlock->vm(), uid, attributesIgnored);
        if (!isValidOffset(variant.m_offset))
            return GetByIdStatus(slowPathState, true);
        
        variant.m_structureSet.add(structure);
        bool didAppend = result.appendVariant(variant);
        ASSERT_UNUSED(didAppend, didAppend);
        return result;
    }
        
    case access_get_by_id_list: {
        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            Structure* structure = list->at(listIndex).structure();
            
            ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(
                profiledBlock, structure, list->at(listIndex).chain(),
                list->at(listIndex).chainCount(), uid);
             
            switch (complexGetStatus.kind()) {
            case ComplexGetStatus::ShouldSkip:
                continue;
                 
            case ComplexGetStatus::TakesSlowPath:
                return GetByIdStatus(slowPathState, true);
                 
            case ComplexGetStatus::Inlineable: {
                std::unique_ptr<CallLinkStatus> callLinkStatus;
                switch (list->at(listIndex).type()) {
                case GetByIdAccess::SimpleInline:
                case GetByIdAccess::SimpleStub: {
                    break;
                }
                case GetByIdAccess::Getter: {
                    AccessorCallJITStubRoutine* stub = static_cast<AccessorCallJITStubRoutine*>(
                        list->at(listIndex).stubRoutine());
                    callLinkStatus = std::make_unique<CallLinkStatus>(
                        CallLinkStatus::computeFor(
                            locker, profiledBlock, *stub->m_callLinkInfo, callExitSiteData));
                    break;
                }
                case GetByIdAccess::CustomGetter:
                case GetByIdAccess::WatchedStub:{
                    // FIXME: It would be totally sweet to support this at some point in the future.
                    // https://bugs.webkit.org/show_bug.cgi?id=133052
                    return GetByIdStatus(slowPathState, true);
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                 
                GetByIdVariant variant(
                    StructureSet(structure), complexGetStatus.offset(), complexGetStatus.chain(),
                    std::move(callLinkStatus));
                 
                if (!result.appendVariant(variant))
                    return GetByIdStatus(slowPathState, true);
                break;
            } }
        }
        
        return result;
    }
        
    default:
        return GetByIdStatus(slowPathState, true);
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
        CallLinkStatus::ExitSiteData exitSiteData;
        {
            ConcurrentJITLocker locker(profiledBlock->m_lock);
            exitSiteData = CallLinkStatus::computeExitSiteData(
                locker, profiledBlock, codeOrigin.bytecodeIndex, ExitFromFTL);
        }
        
        GetByIdStatus result;
        {
            ConcurrentJITLocker locker(dfgBlock->m_lock);
            result = computeForStubInfo(
                locker, dfgBlock, dfgMap.get(codeOrigin), uid, exitSiteData);
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

GetByIdStatus GetByIdStatus::computeFor(VM& vm, const StructureSet& set, StringImpl* uid)
{
    // For now we only handle the super simple self access case. We could handle the
    // prototype case in the future.
    
    if (set.isEmpty())
        return GetByIdStatus();

    if (toUInt32FromStringImpl(uid) != PropertyName::NotAnIndex)
        return GetByIdStatus(TakesSlowPath);
    
    GetByIdStatus result;
    result.m_state = Simple;
    result.m_wasSeenInJIT = false;
    for (unsigned i = 0; i < set.size(); ++i) {
        Structure* structure = set[i];
        if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
            return GetByIdStatus(TakesSlowPath);
        
        if (!structure->propertyAccessesAreCacheable())
            return GetByIdStatus(TakesSlowPath);
        
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(vm, uid, attributes);
        if (!isValidOffset(offset))
            return GetByIdStatus(TakesSlowPath); // It's probably a prototype lookup. Give up on life for now, even though we could totally be way smarter about it.
        if (attributes & Accessor)
            return GetByIdStatus(MakesCalls); // We could be smarter here, like strenght-reducing this to a Call.
        
        if (!result.appendVariant(GetByIdVariant(structure, offset)))
            return GetByIdStatus(TakesSlowPath);
    }
    
    return result;
}

bool GetByIdStatus::makesCalls() const
{
    switch (m_state) {
    case NoInformation:
    case TakesSlowPath:
        return false;
    case Simple:
        for (unsigned i = m_variants.size(); i--;) {
            if (m_variants[i].callLinkStatus())
                return true;
        }
        return false;
    case MakesCalls:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();

    return false;
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

