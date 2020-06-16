/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "LLIntData.h"
#include "LLIntThunks.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "StackAlignment.h"

namespace JSC { namespace LLInt {

static void setFunctionEntrypoint(CodeBlock* codeBlock)
{
    CodeSpecializationKind kind = codeBlock->specializationKind();
    
#if ENABLE(JIT)
    if (Options::useJIT()) {
        if (kind == CodeForCall) {
            static DirectJITCode* jitCode;
            static std::once_flag onceKey;
            std::call_once(onceKey, [&] {
                auto callRef = functionForCallEntryThunk().retagged<JSEntryPtrTag>();
                auto callArityCheckRef = functionForCallArityCheckThunk().retaggedCode<JSEntryPtrTag>();
                jitCode = new DirectJITCode(callRef, callArityCheckRef, JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
            });

            codeBlock->setJITCode(makeRef(*jitCode));
            return;
        }
        ASSERT(kind == CodeForConstruct);

        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            auto constructRef = functionForConstructEntryThunk().retagged<JSEntryPtrTag>();
            auto constructArityCheckRef = functionForConstructArityCheckThunk().retaggedCode<JSEntryPtrTag>();
            jitCode = new DirectJITCode(constructRef, constructArityCheckRef, JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
        });

        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    if (kind == CodeForCall) {
        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            jitCode = new DirectJITCode(getCodeRef<JSEntryPtrTag>(llint_function_for_call_prologue), getCodePtr<JSEntryPtrTag>(llint_function_for_call_arity_check), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
    } else {
        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            jitCode = new DirectJITCode(getCodeRef<JSEntryPtrTag>(llint_function_for_construct_prologue), getCodePtr<JSEntryPtrTag>(llint_function_for_construct_arity_check), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
    }
}

static void setEvalEntrypoint(CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        static NativeJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = evalEntryThunk().retagged<JSEntryPtrTag>();
            jitCode = new NativeJITCode(codeRef, JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_eval_prologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

static void setProgramEntrypoint(CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        static NativeJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = programEntryThunk().retagged<JSEntryPtrTag>();
            jitCode = new NativeJITCode(codeRef, JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_program_prologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

static void setModuleProgramEntrypoint(CodeBlock* codeBlock)
{
#if ENABLE(JIT)
    if (Options::useJIT()) {
        static NativeJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = moduleProgramEntryThunk().retagged<JSEntryPtrTag>();
            jitCode = new NativeJITCode(codeRef, JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_module_program_prologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

void setEntrypoint(CodeBlock* codeBlock)
{
    switch (codeBlock->codeType()) {
    case GlobalCode:
        setProgramEntrypoint(codeBlock);
        return;
    case ModuleCode:
        setModuleProgramEntrypoint(codeBlock);
        return;
    case EvalCode:
        setEvalEntrypoint(codeBlock);
        return;
    case FunctionCode:
        setFunctionEntrypoint(codeBlock);
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
