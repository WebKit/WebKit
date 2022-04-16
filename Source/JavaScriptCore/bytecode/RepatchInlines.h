/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Repatch.h"

#include "VMTrapsInlines.h"

namespace JSC {

inline SlowPathReturnType handleHostCall(JSGlobalObject* globalObject, CallFrame* calleeFrame, JSValue callee, CallLinkInfo* callLinkInfo)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calleeFrame->setCodeBlock(nullptr);

    if (callLinkInfo->specializationKind() == CodeForCall) {
        auto callData = JSC::getCallData(callee);
        ASSERT(callData.type != CallData::Type::JS);

        if (callData.type == CallData::Type::Native) {
            NativeCallFrameTracer tracer(vm, calleeFrame);
            calleeFrame->setCallee(asObject(callee));
            vm.encodedHostCallReturnValue = callData.native.function(asObject(callee)->globalObject(), calleeFrame);
            DisallowGC disallowGC;
            if (UNLIKELY(scope.exception())) {
                return encodeResult(
                    vm.getCTIThrowExceptionFromCallSlowPath().code().executableAddress(),
                    reinterpret_cast<void*>(KeepTheFrame));
            }

            return encodeResult(
                LLInt::getHostCallReturnValueEntrypoint().code().executableAddress(),
                reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
        }

        ASSERT(callData.type == CallData::Type::None);
        throwException(globalObject, scope, createNotAFunctionError(globalObject, callee));
        return encodeResult(
            vm.getCTIThrowExceptionFromCallSlowPath().code().executableAddress(),
            reinterpret_cast<void*>(KeepTheFrame));
    }

    ASSERT(callLinkInfo->specializationKind() == CodeForConstruct);

    auto constructData = JSC::getConstructData(callee);
    ASSERT(constructData.type != CallData::Type::JS);

    if (constructData.type == CallData::Type::Native) {
        NativeCallFrameTracer tracer(vm, calleeFrame);
        calleeFrame->setCallee(asObject(callee));
        vm.encodedHostCallReturnValue = constructData.native.function(asObject(callee)->globalObject(), calleeFrame);
        DisallowGC disallowGC;
        if (UNLIKELY(scope.exception())) {
            return encodeResult(
                vm.getCTIThrowExceptionFromCallSlowPath().code().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }

        return encodeResult(LLInt::getHostCallReturnValueEntrypoint().code().executableAddress(), reinterpret_cast<void*>(KeepTheFrame));
    }

    ASSERT(constructData.type == CallData::Type::None);
    throwException(globalObject, scope, createNotAConstructorError(globalObject, callee));
    return encodeResult(
        vm.getCTIThrowExceptionFromCallSlowPath().code().executableAddress(),
        reinterpret_cast<void*>(KeepTheFrame));
}

ALWAYS_INLINE SlowPathReturnType linkFor(CallFrame* calleeFrame, JSGlobalObject* globalObject, CallLinkInfo* callLinkInfo)
{
    CallFrame* callFrame = calleeFrame->callerFrame();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();
    NativeCallFrameTracer tracer(vm, callFrame);

    RELEASE_ASSERT(!callLinkInfo->isDirect());

    JSValue calleeAsValue = calleeFrame->guaranteedJSValueCallee();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell) {
        if (auto* internalFunction = jsDynamicCast<InternalFunction*>(calleeAsValue)) {
            MacroAssemblerCodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            RELEASE_ASSERT(!!codePtr);

            if (!callLinkInfo->seenOnce())
                callLinkInfo->setSeen();
            else
                linkMonomorphicCall(vm, calleeFrame, *callLinkInfo, nullptr, internalFunction, codePtr);

            void* linkedTarget = codePtr.executableAddress();
            return encodeResult(linkedTarget, reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(globalObject, calleeFrame, calleeAsValue, callLinkInfo));
    }

    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
    CodeBlock* codeBlock = nullptr;

    DeferTraps deferTraps(vm); // We can't jettison any code until after we link the call.

    if (executable->isHostFunction()) {
        codePtr = jsToWasmICCodePtr(kind, callee);
        if (!codePtr)
            codePtr = executable->entrypointFor(kind, MustCheckArity);
    } else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        auto handleThrowException = [&] () {
            void* throwTarget = vm.getCTIThrowExceptionFromCallSlowPath().code().executableAddress();
            return encodeResult(throwTarget, reinterpret_cast<void*>(KeepTheFrame));
        };

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            throwException(globalObject, throwScope, createNotAConstructorError(globalObject, callee));
            return handleThrowException();
        }

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, scope, kind, *codeBlockSlot);
        RETURN_IF_EXCEPTION(throwScope, handleThrowException());

        codeBlock = *codeBlockSlot;
        ASSERT(codeBlock);

        ArityCheckMode arity;
        if (calleeFrame->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo->isVarargs())
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = functionExecutable->entrypointFor(kind, arity);
    }

    if (!callLinkInfo->seenOnce())
        callLinkInfo->setSeen();
    else
        linkMonomorphicCall(vm, calleeFrame, *callLinkInfo, codeBlock, callee, codePtr);

    return encodeResult(codePtr.executableAddress(), reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
}

ALWAYS_INLINE SlowPathReturnType virtualForWithFunction(JSGlobalObject* globalObject, CallFrame* calleeFrame, CallLinkInfo* callLinkInfo, JSCell*& calleeAsFunctionCell)
{
    CallFrame* callFrame = calleeFrame->callerFrame();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();
    NativeCallFrameTracer tracer(vm, callFrame);

    JSValue calleeAsValue = calleeFrame->guaranteedJSValueCallee();
    calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell)) {
        if (jsDynamicCast<InternalFunction*>(calleeAsValue)) {
            MacroAssemblerCodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            ASSERT(!!codePtr);
            return encodeResult(codePtr.executableAddress(), reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(globalObject, calleeFrame, calleeAsValue, callLinkInfo));
    }

    JSFunction* function = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = function->scopeUnchecked();
    ExecutableBase* executable = function->executable();

    DeferTraps deferTraps(vm); // We can't jettison if we're going to call this CodeBlock.

    if (!executable->isHostFunction()) {
        FunctionExecutable* functionExecutable = jsCast<FunctionExecutable*>(executable);

        auto handleThrowException = [&] () {
            void* throwTarget = vm.getCTIThrowExceptionFromCallSlowPath().code().executableAddress();
            return encodeResult(throwTarget, reinterpret_cast<void*>(KeepTheFrame));
        };

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            throwException(globalObject, throwScope, createNotAConstructorError(globalObject, function));
            return handleThrowException();
        }

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        functionExecutable->prepareForExecution<FunctionExecutable>(vm, function, scope, kind, *codeBlockSlot);
        RETURN_IF_EXCEPTION(throwScope, handleThrowException());
    }

    // FIXME: Support wasm IC.
    // https://bugs.webkit.org/show_bug.cgi?id=220339
    return encodeResult(executable->entrypointFor(
        kind, MustCheckArity).executableAddress(),
        reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
}

} // namespace JSC
