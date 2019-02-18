/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "CodeBlock.h"
#include "HeapInlines.h"
#include "JITCode.h"
#include "JSCellInlines.h"
#include "JSObject.h"
#include "LLIntThunks.h"
#include "LowLevelInterpreter.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "StackAlignment.h"
#include "VM.h"

namespace JSC { namespace LLInt {

static void setFunctionEntrypoint(VM& vm, CodeBlock* codeBlock)
{
    CodeSpecializationKind kind = codeBlock->specializationKind();
    
#if ENABLE(JIT)
    if (VM::canUseJIT()) {
        if (kind == CodeForCall) {
            codeBlock->setJITCode(
                adoptRef(*new DirectJITCode(vm.getCTIStub(functionForCallEntryThunkGenerator).retagged<JSEntryPtrTag>(), vm.getCTIStub(functionForCallArityCheckThunkGenerator).retaggedCode<JSEntryPtrTag>(), JITCode::InterpreterThunk)));
            return;
        }
        ASSERT(kind == CodeForConstruct);
        codeBlock->setJITCode(
            adoptRef(*new DirectJITCode(vm.getCTIStub(functionForConstructEntryThunkGenerator).retagged<JSEntryPtrTag>(), vm.getCTIStub(functionForConstructArityCheckThunkGenerator).retaggedCode<JSEntryPtrTag>(), JITCode::InterpreterThunk)));
        return;
    }
#endif // ENABLE(JIT)

    UNUSED_PARAM(vm);
    if (kind == CodeForCall) {
        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            jitCode = new DirectJITCode(getCodeRef<JSEntryPtrTag>(llint_function_for_call_prologue), getCodePtr<JSEntryPtrTag>(llint_function_for_call_arity_check), JITCode::InterpreterThunk);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
    } else {
        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            jitCode = new DirectJITCode(getCodeRef<JSEntryPtrTag>(llint_function_for_construct_prologue), getCodePtr<JSEntryPtrTag>(llint_function_for_construct_arity_check), JITCode::InterpreterThunk);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
    }
}

static void setEvalEntrypoint(VM& vm, CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    if (VM::canUseJIT()) {
        MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = vm.getCTIStub(evalEntryThunkGenerator).retagged<JSEntryPtrTag>();
        codeBlock->setJITCode(
            adoptRef(*new DirectJITCode(codeRef, codeRef.code(), JITCode::InterpreterThunk)));
        return;
    }
#endif // ENABLE(JIT)

    UNUSED_PARAM(vm);
    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_eval_prologue), JITCode::InterpreterThunk, NoIntrinsic);
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

static void setProgramEntrypoint(VM& vm, CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    if (VM::canUseJIT()) {
        MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = vm.getCTIStub(programEntryThunkGenerator).retagged<JSEntryPtrTag>();
        codeBlock->setJITCode(
            adoptRef(*new DirectJITCode(codeRef, codeRef.code(), JITCode::InterpreterThunk)));
        return;
    }
#endif // ENABLE(JIT)

    UNUSED_PARAM(vm);
    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_program_prologue), JITCode::InterpreterThunk, NoIntrinsic);
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

static void setModuleProgramEntrypoint(VM& vm, CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    if (VM::canUseJIT()) {
        MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = vm.getCTIStub(moduleProgramEntryThunkGenerator).retagged<JSEntryPtrTag>();
        codeBlock->setJITCode(
            adoptRef(*new DirectJITCode(codeRef, codeRef.code(), JITCode::InterpreterThunk)));
        return;
    }
#endif // ENABLE(JIT)

    UNUSED_PARAM(vm);
    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_module_program_prologue), JITCode::InterpreterThunk, NoIntrinsic);
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

void setEntrypoint(VM& vm, CodeBlock* codeBlock)
{
    switch (codeBlock->codeType()) {
    case GlobalCode:
        setProgramEntrypoint(vm, codeBlock);
        return;
    case ModuleCode:
        setModuleProgramEntrypoint(vm, codeBlock);
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

unsigned frameRegisterCountFor(CodeBlock* codeBlock)
{
    ASSERT(static_cast<unsigned>(codeBlock->numCalleeLocals()) == WTF::roundUpToMultipleOf(stackAlignmentRegisters(), static_cast<unsigned>(codeBlock->numCalleeLocals())));

    return roundLocalRegisterCountForFramePointerOffset(codeBlock->numCalleeLocals() + maxFrameExtentForSlowPathCallInRegisters);
}

} } // namespace JSC::LLInt
