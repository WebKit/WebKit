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
#include "LLIntThunks.h"

#include "CallData.h"
#include "ExceptionHelpers.h"
#include "Interpreter.h"
#include "JSInterfaceJIT.h"
#include "JSObject.h"
#include "JSStackInlines.h"
#include "LLIntCLoop.h"
#include "LinkBuffer.h"
#include "LowLevelInterpreter.h"
#include "ProtoCallFrame.h"
#include "VM.h"

namespace JSC {

#if ENABLE(JIT)
#if ENABLE(LLINT)

namespace LLInt {

static MacroAssemblerCodeRef generateThunkWithJumpTo(VM* vm, void (*target)(), const char *thunkKind)
{
    JSInterfaceJIT jit(vm);
    
    // FIXME: there's probably a better way to do it on X86, but I'm not sure I care.
    jit.move(JSInterfaceJIT::TrustedImmPtr(bitwise_cast<void*>(target)), JSInterfaceJIT::regT0);
    jit.jump(JSInterfaceJIT::regT0);
    
    LinkBuffer patchBuffer(*vm, &jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, ("LLInt %s prologue thunk", thunkKind));
}

MacroAssemblerCodeRef functionForCallEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_call_prologue, "function for call");
}

MacroAssemblerCodeRef functionForConstructEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_construct_prologue, "function for construct");
}

MacroAssemblerCodeRef functionForCallArityCheckThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_call_arity_check, "function for call with arity check");
}

MacroAssemblerCodeRef functionForConstructArityCheckThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_function_for_construct_arity_check, "function for construct with arity check");
}

MacroAssemblerCodeRef evalEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_eval_prologue, "eval");
}

MacroAssemblerCodeRef programEntryThunkGenerator(VM* vm)
{
    return generateThunkWithJumpTo(vm, llint_program_prologue, "program");
}

} // namespace LLInt

#endif // ENABLE(LLINT)
#else // ENABLE(JIT)

// Non-JIT (i.e. C Loop LLINT) case:

typedef JSValue (*ExecuteCode) (CallFrame*, void* executableAddress);

template<ExecuteCode execute>
EncodedJSValue doCallToJavaScript(void* executableAddress, ProtoCallFrame* protoCallFrame)
{
    CodeBlock* codeBlock = protoCallFrame->codeBlock();
    JSScope* scope = protoCallFrame->scope();
    JSObject* callee = protoCallFrame->callee();
    int argCountIncludingThis = protoCallFrame->argumentCountIncludingThis();
    int argCount = protoCallFrame->argumentCount();
    JSValue thisValue = protoCallFrame->thisValue();
    JSStack& stack = scope->vm()->interpreter->stack();

    CallFrame* newCallFrame = stack.pushFrame(codeBlock, scope, argCountIncludingThis, callee);
    if (UNLIKELY(!newCallFrame)) {
        JSGlobalObject* globalObject = scope->globalObject();
        ExecState* exec = globalObject->globalExec();
        return JSValue::encode(throwStackOverflowError(exec));
    }

    // Set the arguments for the callee:
    newCallFrame->setThisValue(thisValue);
    for (int i = 0; i < argCount; ++i)
        newCallFrame->setArgument(i, protoCallFrame->argument(i));

    JSValue result = execute(newCallFrame, executableAddress);

    stack.popFrame(newCallFrame);

    return JSValue::encode(result);
}

static inline JSValue executeJS(CallFrame* newCallFrame, void* executableAddress)
{
    Opcode entryOpcode = *reinterpret_cast<Opcode*>(&executableAddress);
    return CLoop::execute(newCallFrame, entryOpcode);
}

EncodedJSValue callToJavaScript(void* executableAddress, ExecState**, ProtoCallFrame* protoCallFrame, Register*)
{
    return doCallToJavaScript<executeJS>(executableAddress, protoCallFrame);
}

static inline JSValue executeNative(CallFrame* newCallFrame, void* executableAddress)
{
    NativeFunction function = reinterpret_cast<NativeFunction>(executableAddress);
    return JSValue::decode(function(newCallFrame));
}

EncodedJSValue callToNativeFunction(void* executableAddress, ExecState**, ProtoCallFrame* protoCallFrame, Register*)
{
    return doCallToJavaScript<executeNative>(executableAddress, protoCallFrame);
}

#endif // ENABLE(JIT)

} // namespace JSC
