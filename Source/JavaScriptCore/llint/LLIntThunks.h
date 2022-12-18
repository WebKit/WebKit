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

#pragma once

#include "MacroAssemblerCodeRef.h"
#include "OpcodeSize.h"
#include "VM.h"
#include <wtf/Scope.h>

namespace JSC {

struct ProtoCallFrame;
typedef int64_t EncodedJSValue;

extern "C" {
    EncodedJSValue vmEntryToJavaScript(void*, VM*, ProtoCallFrame*);
    EncodedJSValue vmEntryToNative(void*, VM*, ProtoCallFrame*);
    EncodedJSValue vmEntryCustomGetter(CPURegister, CPURegister, CPURegister, CPURegister);
    EncodedJSValue vmEntryCustomSetter(CPURegister, CPURegister, CPURegister, CPURegister, CPURegister);
    EncodedJSValue vmEntryHostFunction(JSGlobalObject*, CallFrame*, void*);
}

#if CPU(ARM64E)
extern "C" {
    void jitCagePtrGateAfter(void);
    void vmEntryToJavaScriptGateAfter(void);

    void llint_function_for_call_arity_checkUntagGateAfter(void);
    void llint_function_for_call_arity_checkTagGateAfter(void);
    void llint_function_for_construct_arity_checkUntagGateAfter(void);
    void llint_function_for_construct_arity_checkTagGateAfter(void);
}
#endif

inline EncodedJSValue vmEntryToWasm(void* code, VM* vm, ProtoCallFrame* frame)
{
    auto clobberizeValidator = makeScopeExit([&] {
        vm->didEnterVM = true;
    });
    code = retagCodePtr<WasmEntryPtrTag, JSEntryPtrTag>(code);
    return vmEntryToJavaScript(code, vm, frame);
}

namespace LLInt {

MacroAssemblerCodeRef<JSEntryPtrTag> functionForCallEntryThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> functionForConstructEntryThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> functionForCallArityCheckThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> functionForConstructArityCheckThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> evalEntryThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> programEntryThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> moduleProgramEntryThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> getHostCallReturnValueThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> genericReturnPointThunk(OpcodeSize);
MacroAssemblerCodeRef<JSEntryPtrTag> fuzzerReturnEarlyFromLoopHintThunk();

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> callToThrowThunk();
MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleUncaughtExceptionThunk();
MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleCatchThunk(OpcodeSize);

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmCatchThunk(OpcodeSize);
MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmCatchAllThunk(OpcodeSize);
#endif

#if ENABLE(JIT_CAGE)
MacroAssemblerCodeRef<NativeToJITGatePtrTag> jitCagePtrThunk();
#endif

#if CPU(ARM64E)
MacroAssemblerCodeRef<NativeToJITGatePtrTag> createJSGateThunk(void*, PtrTag, const char*);
MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmGateThunk(void*, PtrTag, const char*);
MacroAssemblerCodeRef<NativeToJITGatePtrTag> createTailCallGate(PtrTag, bool);
MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmTailCallGate(PtrTag);
MacroAssemblerCodeRef<NativeToJITGatePtrTag> loopOSREntryGateThunk();
MacroAssemblerCodeRef<NativeToJITGatePtrTag> entryOSREntryGateThunk();
MacroAssemblerCodeRef<NativeToJITGatePtrTag> wasmOSREntryGateThunk();
MacroAssemblerCodeRef<NativeToJITGatePtrTag> exceptionHandlerGateThunk();
MacroAssemblerCodeRef<NativeToJITGatePtrTag> returnFromLLIntGateThunk();
MacroAssemblerCodeRef<NativeToJITGatePtrTag> untagGateThunk(void*);
MacroAssemblerCodeRef<NativeToJITGatePtrTag> tagGateThunk(void*);
#endif

MacroAssemblerCodeRef<JSEntryPtrTag> normalOSRExitTrampolineThunk();
#if ENABLE(DFG_JIT)
MacroAssemblerCodeRef<JSEntryPtrTag> checkpointOSRExitTrampolineThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> checkpointOSRExitFromInlinedCallTrampolineThunk();
MacroAssemblerCodeRef<JSEntryPtrTag> returnLocationThunk(OpcodeID, OpcodeSize);
#endif

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunk();
MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunkSIMD();
#endif // ENABLE(WEBASSEMBLY)

} } // namespace JSC::LLInt
