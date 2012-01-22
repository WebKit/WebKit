/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

namespace JSC {

GetByIdStatus GetByIdStatus::computeFor(CodeBlock* profiledBlock, unsigned bytecodeIndex, Identifier& ident)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(ident);
#if ENABLE(JIT) && ENABLE(VALUE_PROFILER)
    // First check if it makes either calls, in which case we want to be super careful, or
    // if it's not set at all, in which case we punt.
    StructureStubInfo& stubInfo = profiledBlock->getStubInfo(bytecodeIndex);
    if (!stubInfo.seen)
        return GetByIdStatus(NoInformation, StructureSet(), notFound);
    
    PolymorphicAccessStructureList* list;
    int listSize;
    switch (stubInfo.accessType) {
    case access_get_by_id_self_list:
        list = stubInfo.u.getByIdSelfList.structureList;
        listSize = stubInfo.u.getByIdSelfList.listSize;
        break;
    case access_get_by_id_proto_list:
        list = stubInfo.u.getByIdProtoList.structureList;
        listSize = stubInfo.u.getByIdProtoList.listSize;
        break;
    default:
        list = 0;
        listSize = 0;
        break;
    }
    for (int i = 0; i < listSize; ++i) {
        if (!list->list[i].isDirect)
            return GetByIdStatus(MakesCalls, StructureSet(), notFound);
    }
    
    // Next check if it takes slow case, in which case we want to be kind of careful.
    if (profiledBlock->likelyToTakeSlowCase(bytecodeIndex))
        return GetByIdStatus(TakesSlowPath, StructureSet(), notFound);
    
    // Finally figure out if we can derive an access strategy.
    GetByIdStatus result;
    switch (stubInfo.accessType) {
    case access_unset:
        return GetByIdStatus(NoInformation, StructureSet(), notFound);
        
    case access_get_by_id_self: {
        Structure* structure = stubInfo.u.getByIdSelf.baseObjectStructure.get();
        result.m_offset = structure->get(*profiledBlock->globalData(), ident);
        
        if (result.m_offset != notFound)
            result.m_structureSet.add(structure);
        
        if (result.m_offset != notFound)
            ASSERT(result.m_structureSet.size());
        break;
    }
        
    case access_get_by_id_self_list: {
        PolymorphicAccessStructureList* list = stubInfo.u.getByIdProtoList.structureList;
        unsigned size = stubInfo.u.getByIdProtoList.listSize;
        for (unsigned i = 0; i < size; ++i) {
            ASSERT(list->list[i].isDirect);
            
            Structure* structure = list->list[i].base.get();
            if (result.m_structureSet.contains(structure))
                continue;
            
            size_t myOffset = structure->get(*profiledBlock->globalData(), ident);
            
            if (myOffset == notFound) {
                result.m_offset = notFound;
                break;
            }
                    
            if (!i)
                result.m_offset = myOffset;
            else if (result.m_offset != myOffset) {
                result.m_offset = notFound;
                break;
            }
                    
            result.m_structureSet.add(structure);
        }
                    
        if (result.m_offset != notFound)
            ASSERT(result.m_structureSet.size());
        break;
    }
        
    default:
        ASSERT(result.m_offset == notFound);
        break;
    }
    
    if (result.m_offset == notFound) {
        result.m_state = TakesSlowPath;
        result.m_structureSet.clear();
    } else
        result.m_state = SimpleDirect;
    
    return result;
#else // ENABLE(JIT)
    return GetByIdStatus(NoInformation, StructureSet(), notFound);
#endif // ENABLE(JIT)
}

} // namespace JSC

