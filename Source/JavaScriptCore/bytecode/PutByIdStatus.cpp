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
#include "PutByIdStatus.h"

#include "CodeBlock.h"
#include "LLIntData.h"
#include "LowLevelInterpreter.h"
#include "Structure.h"
#include "StructureChain.h"

namespace JSC {

PutByIdStatus PutByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, Identifier& ident)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(ident);
#if ENABLE(LLINT)
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;

    Structure* structure = instruction[4].u.structure.get();
    if (!structure)
        return PutByIdStatus(NoInformation, 0, 0, 0, invalidOffset);
    
    if (instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id)
        || instruction[0].u.opcode == LLInt::getOpcode(llint_op_put_by_id_out_of_line)) {
        PropertyOffset offset = structure->get(*profiledBlock->globalData(), ident);
        if (!isValidOffset(offset))
            return PutByIdStatus(NoInformation, 0, 0, 0, invalidOffset);
        
        return PutByIdStatus(SimpleReplace, structure, 0, 0, offset);
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
    
    PropertyOffset offset = newStructure->get(*profiledBlock->globalData(), ident);
    if (!isValidOffset(offset))
        return PutByIdStatus(NoInformation, 0, 0, 0, invalidOffset);
    
    return PutByIdStatus(SimpleTransition, structure, newStructure, chain, offset);
#else
    return PutByIdStatus(NoInformation, 0, 0, 0, invalidOffset);
#endif
}

PutByIdStatus PutByIdStatus::computeFor(CodeBlock* profiledBlock, unsigned bytecodeIndex, Identifier& ident)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(ident);
#if ENABLE(JIT) && ENABLE(VALUE_PROFILER)
    if (!profiledBlock->numberOfStructureStubInfos())
        return computeFromLLInt(profiledBlock, bytecodeIndex, ident);
    
    if (profiledBlock->likelyToTakeSlowCase(bytecodeIndex))
        return PutByIdStatus(TakesSlowPath, 0, 0, 0, invalidOffset);
    
    StructureStubInfo& stubInfo = profiledBlock->getStubInfo(bytecodeIndex);
    if (!stubInfo.seen)
        return computeFromLLInt(profiledBlock, bytecodeIndex, ident);
    
    switch (stubInfo.accessType) {
    case access_unset:
        return computeFromLLInt(profiledBlock, bytecodeIndex, ident);
        
    case access_put_by_id_replace: {
        PropertyOffset offset = stubInfo.u.putByIdReplace.baseObjectStructure->get(
            *profiledBlock->globalData(), ident);
        if (isValidOffset(offset)) {
            return PutByIdStatus(
                SimpleReplace,
                stubInfo.u.putByIdReplace.baseObjectStructure.get(),
                0, 0,
                offset);
        }
        return PutByIdStatus(TakesSlowPath, 0, 0, 0, invalidOffset);
    }
        
    case access_put_by_id_transition_normal:
    case access_put_by_id_transition_direct: {
        ASSERT(stubInfo.u.putByIdTransition.previousStructure->transitionWatchpointSetHasBeenInvalidated());
        PropertyOffset offset = stubInfo.u.putByIdTransition.structure->get(
            *profiledBlock->globalData(), ident);
        if (isValidOffset(offset)) {
            return PutByIdStatus(
                SimpleTransition,
                stubInfo.u.putByIdTransition.previousStructure.get(),
                stubInfo.u.putByIdTransition.structure.get(),
                stubInfo.u.putByIdTransition.chain.get(),
                offset);
        }
        return PutByIdStatus(TakesSlowPath, 0, 0, 0, invalidOffset);
    }
        
    default:
        return PutByIdStatus(TakesSlowPath, 0, 0, 0, invalidOffset);
    }
#else // ENABLE(JIT)
    return PutByIdStatus(NoInformation, 0, 0, 0, invalidOffset);
#endif // ENABLE(JIT)
}

} // namespace JSC

