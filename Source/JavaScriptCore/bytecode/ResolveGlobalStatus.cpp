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
#include "ResolveGlobalStatus.h"

#include "CodeBlock.h"
#include "JSValue.h"
#include "Structure.h"

namespace JSC {

#if ENABLE(LLINT) || (ENABLE(JIT) && ENABLE(VALUE_PROFILER))
static ResolveGlobalStatus computeForStructure(CodeBlock* codeBlock, Structure* structure, Identifier& identifier)
{
    unsigned attributesIgnored;
    JSCell* specificValue;
    PropertyOffset offset = structure->get(
        *codeBlock->globalData(), identifier, attributesIgnored, specificValue);
    if (structure->isDictionary())
        specificValue = 0;
    if (!isValidOffset(offset))
        return ResolveGlobalStatus();
    
    return ResolveGlobalStatus(ResolveGlobalStatus::Simple, structure, offset, specificValue);
}
#endif // ENABLE(LLINT) || ENABLE(JIT)

static ResolveGlobalStatus computeForLLInt(CodeBlock* codeBlock, unsigned bytecodeIndex, Identifier& identifier)
{
#if ENABLE(LLINT)
    Instruction* instruction = codeBlock->instructions().begin() + bytecodeIndex;
    
    ASSERT(instruction[0].u.opcode == llint_op_resolve_global);
    
    Structure* structure = instruction[3].u.structure.get();
    if (!structure)
        return ResolveGlobalStatus();
    
    return computeForStructure(codeBlock, structure, identifier);
#else
    UNUSED_PARAM(codeBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(identifier);
    return ResolveGlobalStatus();
#endif
}

ResolveGlobalStatus ResolveGlobalStatus::computeFor(CodeBlock* codeBlock, unsigned bytecodeIndex, Identifier& identifier)
{
#if ENABLE(JIT) && ENABLE(VALUE_PROFILER)
    if (!codeBlock->numberOfGlobalResolveInfos())
        return computeForLLInt(codeBlock, bytecodeIndex, identifier);
    
    if (codeBlock->likelyToTakeSlowCase(bytecodeIndex))
        return ResolveGlobalStatus(TakesSlowPath);
    
    GlobalResolveInfo& globalResolveInfo = codeBlock->globalResolveInfoForBytecodeOffset(bytecodeIndex);
    
    if (!globalResolveInfo.structure)
        return computeForLLInt(codeBlock, bytecodeIndex, identifier);
    
    return computeForStructure(codeBlock, globalResolveInfo.structure.get(), identifier);
#else
    return computeForLLInt(codeBlock, bytecodeIndex, identifier);
#endif
}

} // namespace JSC

