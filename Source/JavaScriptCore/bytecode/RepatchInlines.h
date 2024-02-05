/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "ErrorInstance.h"
#include "FrameTracers.h"
#include "LLIntEntrypoint.h"
#include "Repatch.h"

#include "VMTrapsInlines.h"

namespace JSC {

inline void* throwNotAFunctionErrorFromCallIC(JSGlobalObject* globalObject, JSCell* owner, JSValue callee, CallLinkInfo* callLinkInfo)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto errorMessage = constructErrorMessage(globalObject, callee, "is not a function"_s);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (UNLIKELY(!errorMessage)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    // Call IC will throw these errors after throwing away the caller's frame when it is a tail-call.
    // But we would like to have error information for them from the thrown frame.
    // This frame information can be reconstructed easily since we have CodeOrigin and owner CodeBlock for CallLinkInfo.
    auto [codeBlock, bytecodeIndex] = callLinkInfo->retrieveCaller(owner);
    if (codeBlock)
        errorMessage = appendSourceToErrorMessage(codeBlock, bytecodeIndex, errorMessage, runtimeTypeForValue(callee), notAFunctionSourceAppender);
    auto* error = ErrorInstance::create(vm, globalObject->errorStructure(ErrorType::TypeError), errorMessage, JSValue(), ErrorType::TypeError, owner, callLinkInfo);
    throwException(globalObject, scope, error);
    return nullptr;
}

inline void* throwNotAConstructorErrorFromCallIC(JSGlobalObject* globalObject, JSCell* owner, JSValue callee, CallLinkInfo* callLinkInfo)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto errorMessage = constructErrorMessage(globalObject, callee, "is not a constructor"_s);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (UNLIKELY(!errorMessage)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    // Call IC will throw these errors after throwing away the caller's frame when it is a tail-call.
    // But we would like to have error information for them from the thrown frame.
    // This frame information can be reconstructed easily since we have CodeOrigin and owner CodeBlock for CallLinkInfo.
    auto [codeBlock, bytecodeIndex] = callLinkInfo->retrieveCaller(owner);
    if (codeBlock)
        errorMessage = appendSourceToErrorMessage(codeBlock, bytecodeIndex, errorMessage, runtimeTypeForValue(callee), defaultSourceAppender);
    auto* error = ErrorInstance::create(vm, globalObject->errorStructure(ErrorType::TypeError), errorMessage, JSValue(), ErrorType::TypeError, owner, callLinkInfo);
    throwException(globalObject, scope, error);
    return nullptr;
}

inline void* handleHostCall(VM& vm, JSCell* owner, CallFrame* calleeFrame, JSValue callee, CallLinkInfo* callLinkInfo)
{
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
            if (UNLIKELY(scope.exception()))
                return nullptr;
            return LLInt::getHostCallReturnValueEntrypoint().code().taggedPtr();
        }

        auto* globalObject = callLinkInfo->globalObjectForSlowPath(owner);
        calleeFrame->setCallee(globalObject->partiallyInitializedFrameCallee());
        ASSERT(callData.type == CallData::Type::None);
        RELEASE_AND_RETURN(scope, throwNotAFunctionErrorFromCallIC(globalObject, owner, callee, callLinkInfo));
    }

    ASSERT(callLinkInfo->specializationKind() == CodeForConstruct);

    auto constructData = JSC::getConstructData(callee);
    ASSERT(constructData.type != CallData::Type::JS);

    if (constructData.type == CallData::Type::Native) {
        NativeCallFrameTracer tracer(vm, calleeFrame);
        calleeFrame->setCallee(asObject(callee));
        vm.encodedHostCallReturnValue = constructData.native.function(asObject(callee)->globalObject(), calleeFrame);
        DisallowGC disallowGC;
        if (UNLIKELY(scope.exception()))
            return nullptr;
        return LLInt::getHostCallReturnValueEntrypoint().code().taggedPtr();
    }

    auto* globalObject = callLinkInfo->globalObjectForSlowPath(owner);
    calleeFrame->setCallee(globalObject->partiallyInitializedFrameCallee());
    ASSERT(constructData.type == CallData::Type::None);
    RELEASE_AND_RETURN(scope, throwNotAConstructorErrorFromCallIC(globalObject, owner, callee, callLinkInfo));
}

ALWAYS_INLINE void* linkFor(VM& vm, JSCell* owner, CallFrame* calleeFrame, CallLinkInfo* callLinkInfo)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();

    JSValue calleeAsValue = calleeFrame->guaranteedJSValueCallee();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell) {
        if (auto* internalFunction = jsDynamicCast<InternalFunction*>(calleeAsValue)) {
            CodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            RELEASE_ASSERT(!!codePtr);

            switch (callLinkInfo->mode()) {
            case CallLinkInfo::Mode::Init: {
                if (!callLinkInfo->seenOnce())
                    callLinkInfo->setSeen();
                else
                    linkMonomorphicCall(vm, owner, *callLinkInfo, nullptr, internalFunction, codePtr);
                break;
            }
            case CallLinkInfo::Mode::Monomorphic:
            case CallLinkInfo::Mode::Polymorphic: {
#if ENABLE(JIT)
                if (kind == CodeForCall && callLinkInfo->allowStubs()) {
                    linkPolymorphicCall(vm, owner, calleeFrame, *callLinkInfo, CallVariant(internalFunction));
                    break;
                }
#endif
                callLinkInfo->setVirtualCall(vm);
                break;
            }
            case CallLinkInfo::Mode::Virtual:
                break;
            }

            return codePtr.taggedPtr();
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(vm, owner, calleeFrame, calleeAsValue, callLinkInfo));
    }

    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    CodePtr<JSEntryPtrTag> codePtr;
    CodeBlock* codeBlock = nullptr;

    DeferTraps deferTraps(vm); // We can't jettison any code until after we link the call.

    if (executable->isHostFunction()) {
        codePtr = jsToWasmICCodePtr(kind, callee);
        if (!codePtr)
            codePtr = executable->entrypointFor(kind, MustCheckArity);
    } else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            auto* globalObject = callLinkInfo->globalObjectForSlowPath(owner);
            calleeFrame->setCallee(globalObject->partiallyInitializedFrameCallee());
            RELEASE_AND_RETURN(throwScope, throwNotAConstructorErrorFromCallIC(globalObject, owner, callee, callLinkInfo));
        }

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, scope, kind, *codeBlockSlot);
        RETURN_IF_EXCEPTION(throwScope, nullptr);

        codeBlock = *codeBlockSlot;
        ASSERT(codeBlock);

        ArityCheckMode arity;
        if (calleeFrame->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo->isVarargs())
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = functionExecutable->entrypointFor(kind, arity);
    }

    switch (callLinkInfo->mode()) {
    case CallLinkInfo::Mode::Init: {
        if (!callLinkInfo->seenOnce())
            callLinkInfo->setSeen();
        else
            linkMonomorphicCall(vm, owner, *callLinkInfo, codeBlock, callee, codePtr);
        break;
    }
    case CallLinkInfo::Mode::Monomorphic:
    case CallLinkInfo::Mode::Polymorphic: {
#if ENABLE(JIT)
        if (kind == CodeForCall && callLinkInfo->allowStubs()) {
            linkPolymorphicCall(vm, owner, calleeFrame, *callLinkInfo, CallVariant(callee));
            break;
        }
#endif
        callLinkInfo->setVirtualCall(vm);
        break;
    }
    case CallLinkInfo::Mode::Virtual:
        break;
    }

    return codePtr.taggedPtr();
}

ALWAYS_INLINE void* virtualForWithFunction(VM& vm, JSCell* owner, CallFrame* calleeFrame, CallLinkInfo* callLinkInfo, JSCell*& calleeAsFunctionCell)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();

    JSValue calleeAsValue = calleeFrame->guaranteedJSValueCallee();
    calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell)) {
        if (jsDynamicCast<InternalFunction*>(calleeAsValue)) {
            CodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            ASSERT(!!codePtr);
            return codePtr.taggedPtr();
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(vm, owner, calleeFrame, calleeAsValue, callLinkInfo));
    }

    JSFunction* function = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = function->scopeUnchecked();
    ExecutableBase* executable = function->executable();

    DeferTraps deferTraps(vm); // We can't jettison if we're going to call this CodeBlock.

    if (!executable->isHostFunction()) {
        FunctionExecutable* functionExecutable = jsCast<FunctionExecutable*>(executable);

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            auto* globalObject = callLinkInfo->globalObjectForSlowPath(owner);
            calleeFrame->setCallee(globalObject->partiallyInitializedFrameCallee());
            RELEASE_AND_RETURN(throwScope, throwNotAConstructorErrorFromCallIC(globalObject, owner, function, callLinkInfo));
        }

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        functionExecutable->prepareForExecution<FunctionExecutable>(vm, function, scope, kind, *codeBlockSlot);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }

    // FIXME: Support wasm IC.
    // https://bugs.webkit.org/show_bug.cgi?id=220339
    return executable->entrypointFor(kind, MustCheckArity).taggedPtr();
}

} // namespace JSC
