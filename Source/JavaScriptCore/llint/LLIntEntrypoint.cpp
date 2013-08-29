/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "LLIntEntrypoint.h"

#if ENABLE(LLINT)

#include "CodeBlock.h"
#include "JITCode.h"
#include "JSObject.h"
#include "LLIntThunks.h"
#include "LowLevelInterpreter.h"
#include "VM.h"


namespace JSC { namespace LLInt {

static void setFunctionEntrypoint(VM& vm, CodeBlock* codeBlock)
{
    CodeSpecializationKind kind = codeBlock->specializationKind();
    
    if (!vm.canUseJIT()) {
        if (kind == CodeForCall) {
            codeBlock->setJITCode(
                adoptRef(new DirectJITCode(MacroAssemblerCodeRef::createLLIntCodeRef(llint_function_for_call_prologue), JITCode::InterpreterThunk)),
                MacroAssemblerCodePtr::createLLIntCodePtr(llint_function_for_call_arity_check));
            return;
        }

        ASSERT(kind == CodeForConstruct);
        codeBlock->setJITCode(
            adoptRef(new DirectJITCode(MacroAssemblerCodeRef::createLLIntCodeRef(llint_function_for_construct_prologue), JITCode::InterpreterThunk)),
            MacroAssemblerCodePtr::createLLIntCodePtr(llint_function_for_construct_arity_check));
        return;
    }
    
#if ENABLE(JIT)
    if (kind == CodeForCall) {
        codeBlock->setJITCode(
            adoptRef(new DirectJITCode(vm.getCTIStub(functionForCallEntryThunkGenerator), JITCode::InterpreterThunk)),
            vm.getCTIStub(functionForCallArityCheckThunkGenerator).code());
        return;
    }

    ASSERT(kind == CodeForConstruct);
    codeBlock->setJITCode(
        adoptRef(new DirectJITCode(vm.getCTIStub(functionForConstructEntryThunkGenerator), JITCode::InterpreterThunk)),
        vm.getCTIStub(functionForConstructArityCheckThunkGenerator).code());
#endif // ENABLE(JIT)
}

static void setEvalEntrypoint(VM& vm, CodeBlock* codeBlock)
{
    if (!vm.canUseJIT()) {
        codeBlock->setJITCode(
            adoptRef(new DirectJITCode(MacroAssemblerCodeRef::createLLIntCodeRef(llint_eval_prologue), JITCode::InterpreterThunk)),
            MacroAssemblerCodePtr());
        return;
    }
#if ENABLE(JIT)
    codeBlock->setJITCode(
        adoptRef(new DirectJITCode(vm.getCTIStub(evalEntryThunkGenerator), JITCode::InterpreterThunk)),
        MacroAssemblerCodePtr());
#endif
}

static void setProgramEntrypoint(VM& vm, CodeBlock* codeBlock)
{
    if (!vm.canUseJIT()) {
        codeBlock->setJITCode(
            adoptRef(new DirectJITCode(MacroAssemblerCodeRef::createLLIntCodeRef(llint_program_prologue), JITCode::InterpreterThunk)),
            MacroAssemblerCodePtr());
        return;
    }
#if ENABLE(JIT)
    codeBlock->setJITCode(
        adoptRef(new DirectJITCode(vm.getCTIStub(programEntryThunkGenerator), JITCode::InterpreterThunk)),
        MacroAssemblerCodePtr());
#endif
}

void setEntrypoint(VM& vm, CodeBlock* codeBlock)
{
    switch (codeBlock->codeType()) {
    case GlobalCode:
        setProgramEntrypoint(vm, codeBlock);
        return;
    case EvalCode:
        setEvalEntrypoint(vm, codeBlock);
        return;
    case FunctionCode:
        setFunctionEntrypoint(vm, codeBlock);
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} } // namespace JSC::LLInt

#endif // ENABLE(LLINT)
