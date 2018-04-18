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

namespace JSC {

EncodedJSValue JS_EXPORT_PRIVATE vmEntryToWasm(void* code, VM* vm, ProtoCallFrame* frame)
{
    code = retagCodePtr<WasmEntryPtrTag, JSEntryPtrTag>(code);
    return vmEntryToJavaScript(code, vm, frame);
}
    
#if ENABLE(JIT)

namespace LLInt {

static MacroAssemblerCodeRef<JITThunkPtrTag> generateThunkWithJumpTo(VM* vm, OpcodeID opcodeID, const char *thunkKind)
{
    JSInterfaceJIT jit(vm);

    // FIXME: there's probably a better way to do it on X86, but I'm not sure I care.
    LLIntCode target = LLInt::getCodeFunctionPtr<JSEntryPtrTag>(opcodeID);
    assertIsTaggedWith(target, JSEntryPtrTag);

    jit.move(JSInterfaceJIT::TrustedImmPtr(target), JSInterfaceJIT::regT0);
    jit.jump(JSInterfaceJIT::regT0, JSEntryPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "LLInt %s prologue thunk", thunkKind);
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForCallEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_call_prologue, "function for call");
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForConstructEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_construct_prologue, "function for construct");
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForCallArityCheckThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_call_arity_check, "function for call with arity check");
}

MacroAssemblerCodeRef<JITThunkPtrTag> functionForConstructArityCheckThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_construct_arity_check, "function for construct with arity check");
}

MacroAssemblerCodeRef<JITThunkPtrTag> evalEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_eval_prologue, "eval");
}

MacroAssemblerCodeRef<JITThunkPtrTag> programEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_program_prologue, "program");
}

MacroAssemblerCodeRef<JITThunkPtrTag> moduleProgramEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_module_program_prologue, "module_program");
}

} // namespace LLInt

#else // ENABLE(JIT)

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


#endif // ENABLE(JIT)

} // namespace JSC
