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
#include "LLIntThunks.h"

#if ENABLE(LLINT)

#include "JSInterfaceJIT.h"
#include "JSObject.h"
#include "LinkBuffer.h"
#include "LowLevelInterpreter.h"
#include "ScopeChain.h"

namespace JSC { namespace LLInt {

static MacroAssemblerCodeRef generateThunkWithJumpTo(JSGlobalData* globalData, void (*target)())
{
    JSInterfaceJIT jit;
    
    // FIXME: there's probably a better way to do it on X86, but I'm not sure I care.
    jit.move(JSInterfaceJIT::TrustedImmPtr(bitwise_cast<void*>(target)), JSInterfaceJIT::regT0);
    jit.jump(JSInterfaceJIT::regT0);
    
    LinkBuffer patchBuffer(*globalData, &jit, GLOBAL_THUNK_ID);
    return patchBuffer.finalizeCode();
}

MacroAssemblerCodeRef functionForCallEntryThunkGenerator(JSGlobalData* globalData)
{
    return generateThunkWithJumpTo(globalData, llint_function_for_call_prologue);
}

MacroAssemblerCodeRef functionForConstructEntryThunkGenerator(JSGlobalData* globalData)
{
    return generateThunkWithJumpTo(globalData, llint_function_for_construct_prologue);
}

MacroAssemblerCodeRef functionForCallArityCheckThunkGenerator(JSGlobalData* globalData)
{
    return generateThunkWithJumpTo(globalData, llint_function_for_call_arity_check);
}

MacroAssemblerCodeRef functionForConstructArityCheckThunkGenerator(JSGlobalData* globalData)
{
    return generateThunkWithJumpTo(globalData, llint_function_for_construct_arity_check);
}

MacroAssemblerCodeRef evalEntryThunkGenerator(JSGlobalData* globalData)
{
    return generateThunkWithJumpTo(globalData, llint_eval_prologue);
}

MacroAssemblerCodeRef programEntryThunkGenerator(JSGlobalData* globalData)
{
    return generateThunkWithJumpTo(globalData, llint_program_prologue);
}

} } // namespace JSC::LLInt

#endif // ENABLE(LLINT)
