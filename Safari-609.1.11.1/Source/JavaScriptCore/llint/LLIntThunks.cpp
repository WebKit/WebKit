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
#include "LLIntThunks.h"

#include "CallData.h"
#include "ExceptionHelpers.h"
#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "JSInterfaceJIT.h"
#include "JSObject.h"
#include "LLIntCLoop.h"
#include "LLIntData.h"
#include "LinkBuffer.h"
#include "LowLevelInterpreter.h"
#include "ProtoCallFrame.h"
#include "StackAlignment.h"
#include "VM.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include <wtf/NeverDestroyed.h>

namespace JSC {

#if ENABLE(JIT)

namespace LLInt {

// These thunks are necessary because of nearCall used on JITed code.
// It requires that the distance from nearCall address to the destination address
// fits on 32-bits, and that's not the case of getCodeRef(llint_function_for_call_prologue)
// and others LLIntEntrypoints.

static MacroAssemblerCodeRef<JITThunkPtrTag> generateThunkWithJumpTo(OpcodeID opcodeID, const char *thunkKind)
{
    JSInterfaceJIT jit;

    // FIXME: there's probably a better way to do it on X86, but I'm not sure I care.
    LLIntCode target = LLInt::getCodeFunctionPtr<JSEntryPtrTag>(opcodeID);
    assertIsTaggedWith(target, JSEntryPtrTag);

#if ENABLE(WEBASSEMBLY)
    CCallHelpers::RegisterID scratch = Wasm::wasmCallingConvention().prologueScratchGPRs[0];
#else
    CCallHelpers::RegisterID scratch = JSInterfaceJIT::regT0;
#endif
    jit.move(JSInterfaceJIT::TrustedImmPtr(target), scratch);
    jit.farJump(scratch, JSEntryPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "LLInt %s prologue thunk", thunkKind);
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForCallEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_function_for_call_prologue, "function for call"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForConstructEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_function_for_construct_prologue, "function for construct"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForCallArityCheckThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_function_for_call_arity_check, "function for call with arity check"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForConstructArityCheckThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_function_for_construct_arity_check, "function for construct with arity check"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> evalEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_eval_prologue, "eval"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> programEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_program_prologue, "program"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> moduleProgramEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo(llint_module_program_prologue, "module_program"));
    });
    return codeRef;
}

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        if (Wasm::Context::useFastTLS())
            codeRef.construct(generateThunkWithJumpTo(wasm_function_prologue, "function for call"));
        else
            codeRef.construct(generateThunkWithJumpTo(wasm_function_prologue_no_tls, "function for call"));
    });
    return codeRef;
}
#endif // ENABLE(WEBASSEMBLY)

} // namespace LLInt

#endif

#if ENABLE(C_LOOP)
// Non-JIT (i.e. C Loop LLINT) case:

EncodedJSValue vmEntryToJavaScript(void* executableAddress, VM* vm, ProtoCallFrame* protoCallFrame)
{
    JSValue result = CLoop::execute(llint_vm_entry_to_javascript, executableAddress, vm, protoCallFrame);
    return JSValue::encode(result);
}

EncodedJSValue vmEntryToNative(void* executableAddress, VM* vm, ProtoCallFrame* protoCallFrame)
{
    JSValue result = CLoop::execute(llint_vm_entry_to_native, executableAddress, vm, protoCallFrame);
    return JSValue::encode(result);
}

extern "C" VMEntryRecord* vmEntryRecord(EntryFrame* entryFrame)
{
    // The C Loop doesn't have any callee save registers, so the VMEntryRecord is allocated at the base of the frame.
    intptr_t stackAlignment = stackAlignmentBytes();
    intptr_t VMEntryTotalFrameSize = (sizeof(VMEntryRecord) + (stackAlignment - 1)) & ~(stackAlignment - 1);
    return reinterpret_cast<VMEntryRecord*>(reinterpret_cast<char*>(entryFrame) - VMEntryTotalFrameSize);
}

#endif // ENABLE(C_LOOP)

} // namespace JSC
