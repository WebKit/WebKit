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

#if CPU(ARM64E)
extern "C" void jsTrampolineProgramPrologue(void);
extern "C" void jsTrampolineModuleProgramPrologue(void);
extern "C" void jsTrampolineEvalPrologue(void);
extern "C" void jsTrampolineFunctionForCallPrologue(void);
extern "C" void jsTrampolineFunctionForConstructPrologue(void);
extern "C" void jsTrampolineFunctionForCallArityCheckPrologue(void);
extern "C" void jsTrampolineFunctionForConstructArityCheckPrologue(void);

template<typename PtrType>
static MacroAssemblerCodeRef<JSEntryPtrTag> entrypointTrampoline(PtrType address)
{
    return MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(MacroAssemblerCodePtr<JSEntryPtrTag>::createFromExecutableAddress(retagCodePtr<void*, CFunctionPtrTag, JSEntryPtrTag>(address)));
}
#endif

static void setFunctionEntrypoint(CodeBlock* codeBlock)
{
    CodeSpecializationKind kind = codeBlock->specializationKind();
    
#if ENABLE(JIT)
    if (Options::useJIT()) {
        if (kind == CodeForCall) {
            static DirectJITCode* jitCode;
            static std::once_flag onceKey;
            std::call_once(onceKey, [&] {
                auto callRef = functionForCallEntryThunk();
                auto callArityCheckRef = functionForCallArityCheckThunk();
                jitCode = new DirectJITCode(callRef, callArityCheckRef.code(), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
            });

            codeBlock->setJITCode(makeRef(*jitCode));
            return;
        }
        ASSERT(kind == CodeForConstruct);

        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            auto constructRef = functionForConstructEntryThunk();
            auto constructArityCheckRef = functionForConstructArityCheckThunk();
            jitCode = new DirectJITCode(constructRef, constructArityCheckRef.code(), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
        });

        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    if (kind == CodeForCall) {
        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
#if CPU(ARM64E)
            jitCode = new DirectJITCode(entrypointTrampoline(jsTrampolineFunctionForCallPrologue), entrypointTrampoline(jsTrampolineFunctionForCallArityCheckPrologue).code(), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
#else
            jitCode = new DirectJITCode(getCodeRef<JSEntryPtrTag>(llint_function_for_call_prologue), getCodePtr<JSEntryPtrTag>(llint_function_for_call_arity_check), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
#endif
        });
        codeBlock->setJITCode(makeRef(*jitCode));
    } else {
        static DirectJITCode* jitCode;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
#if CPU(ARM64E)
            jitCode = new DirectJITCode(entrypointTrampoline(jsTrampolineFunctionForConstructPrologue), entrypointTrampoline(jsTrampolineFunctionForConstructArityCheckPrologue).code(), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
#else
            jitCode = new DirectJITCode(getCodeRef<JSEntryPtrTag>(llint_function_for_construct_prologue), getCodePtr<JSEntryPtrTag>(llint_function_for_construct_arity_check), JITType::InterpreterThunk, JITCode::ShareAttribute::Shared);
#endif
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
            MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = evalEntryThunk();
            jitCode = new NativeJITCode(codeRef, JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
#if CPU(ARM64E)
        jitCode = new NativeJITCode(entrypointTrampoline(jsTrampolineEvalPrologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
#else
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_eval_prologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
#endif
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
            MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = programEntryThunk();
            jitCode = new NativeJITCode(codeRef, JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
#if CPU(ARM64E)
        jitCode = new NativeJITCode(entrypointTrampoline(jsTrampolineProgramPrologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
#else
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_program_prologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
#endif
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
            MacroAssemblerCodeRef<JSEntryPtrTag> codeRef = moduleProgramEntryThunk();
            jitCode = new NativeJITCode(codeRef, JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
        });
        codeBlock->setJITCode(makeRef(*jitCode));
        return;
    }
#endif // ENABLE(JIT)

    static NativeJITCode* jitCode;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
#if CPU(ARM64E)
        jitCode = new NativeJITCode(entrypointTrampoline(jsTrampolineModuleProgramPrologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
#else
        jitCode = new NativeJITCode(getCodeRef<JSEntryPtrTag>(llint_module_program_prologue), JITType::InterpreterThunk, Intrinsic::NoIntrinsic, JITCode::ShareAttribute::Shared);
#endif
    });
    codeBlock->setJITCode(makeRef(*jitCode));
}

MacroAssemblerCodeRef<JSEntryPtrTag> getHostCallReturnValueEntrypoint()
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return getHostCallReturnValueThunk();
#endif // ENABLE(JIT)
    return LLInt::getCodeRef<JSEntryPtrTag>(llint_get_host_call_return_value);
}

MacroAssemblerCodeRef<JSEntryPtrTag> fuzzerReturnEarlyFromLoopHintEntrypoint()
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return fuzzerReturnEarlyFromLoopHintThunk();
#endif // ENABLE(JIT)
    return LLInt::getCodeRef<JSEntryPtrTag>(fuzzer_return_early_from_loop_hint);
}

MacroAssemblerCodeRef<JSEntryPtrTag> genericReturnPointEntrypoint(OpcodeSize size)
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return genericReturnPointThunk(size);
#endif // ENABLE(JIT)
    switch (size) {
    case OpcodeSize::Narrow:
        return LLInt::getCodeRef<JSEntryPtrTag>(llint_generic_return_point);
    case OpcodeSize::Wide16:
        return LLInt::getWide16CodeRef<JSEntryPtrTag>(llint_generic_return_point);
    case OpcodeSize::Wide32:
        return LLInt::getWide32CodeRef<JSEntryPtrTag>(llint_generic_return_point);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
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
