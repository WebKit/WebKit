/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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
#include "JITOperations.h"

#if ENABLE(JIT)

#include "ArithProfile.h"
#include "ArrayConstructor.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlockInlines.h"
#include "CommonSlowPathsInlines.h"
#include "DFGDriver.h"
#include "DFGOSREntry.h"
#include "DFGThunks.h"
#include "Debugger.h"
#include "EnsureStillAliveHere.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "GetterSetter.h"
#include "ICStats.h"
#include "InlineCacheCompiler.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JITThunks.h"
#include "JITToDFGDeferredCompilationCallback.h"
#include "JITWorklist.h"
#include "JSArrayIterator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGenerator.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSCPtrTag.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSLexicalEnvironment.h"
#include "JSRemoteFunction.h"
#include "JSWithScope.h"
#include "LLIntEntrypoint.h"
#include "MegamorphicCache.h"
#include "ObjectConstructor.h"
#include "PropertyName.h"
#include "RegExpObject.h"
#include "RepatchInlines.h"
#include "ShadowChicken.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "TypeProfilerLog.h"
#include "VMInlines.h"
#include "VMTrapsInlines.h"

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC {



ALWAYS_INLINE ICSlowPathCallFrameTracer::ICSlowPathCallFrameTracer(VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo)
 #if ASSERT_ENABLED
        : m_vm(vm)
#endif
{
    UNUSED_PARAM(vm);
    UNUSED_PARAM(callFrame);
    ASSERT(callFrame);
    ASSERT(reinterpret_cast<void*>(callFrame) < reinterpret_cast<void*>(vm.topEntryFrame));
    assertStackPointerIsAligned();
#if USE(BUILTIN_FRAME_ADDRESS)
    // If ASSERT_ENABLED and USE(BUILTIN_FRAME_ADDRESS), prepareCallOperation() will put the frame pointer into vm.topCallFrame.
    // We can ensure here that a call to prepareCallOperation() (or its equivalent) is not missing by comparing vm.topCallFrame to
    // the result of __builtin_frame_address which is passed in as callFrame.
    ASSERT(vm.topCallFrame == callFrame);
    vm.topCallFrame = callFrame;
#endif
    callFrame->setCallSiteIndex(stubInfo->callSiteIndex);
}

ALWAYS_INLINE JSValue profiledAdd(JSGlobalObject* globalObject, JSValue op1, JSValue op2, BinaryArithProfile& arithProfile)
{
    arithProfile.observeLHSAndRHS(op1, op2);
    JSValue result = jsAdd(globalObject, op1, op2);
    arithProfile.observeResult(result);
    return result;
}

// FIXME (see rdar://72897291): Work around a Clang bug where __builtin_return_address()
// sometimes gives us a signed pointer, and sometimes does not.
#define OUR_RETURN_ADDRESS removeCodePtrTag(__builtin_return_address(0))


JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationThrowStackOverflowError, void, (CodeBlock* codeBlock))
{
    // We pass in our own code block, because the callframe hasn't been populated.
    VM& vm = codeBlock->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    callFrame->convertToStackOverflowFrame(vm, codeBlock);
    throwStackOverflowError(codeBlock->globalObject(), scope);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationThrowStackOverflowErrorFromThunk, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwStackOverflowError(globalObject, scope);
    genericUnwind(vm, callFrame);
    ASSERT(vm.targetMachinePCForThrow);
}

static JSValue getWrappedValue(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject())
        RELEASE_AND_RETURN(scope, value);

    if (value.isCallable())
        RELEASE_AND_RETURN(scope, JSRemoteFunction::tryCreate(targetGlobalObject, vm, static_cast<JSObject*>(value.asCell())));

    throwTypeError(globalObject, scope, "value passing between realms must be callable or primitive"_s);
    return jsUndefined();
}

JSC_DEFINE_JIT_OPERATION(operationGetWrappedValueForTarget, EncodedJSValue, (JSRemoteFunction* callee, EncodedJSValue encodedValue))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();

    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(isRemoteFunction(callee));

    JSGlobalObject* targetGlobalObject = callee->targetFunction()->globalObject();
    OPERATION_RETURN(scope, JSValue::encode(getWrappedValue(globalObject, targetGlobalObject, JSValue::decode(encodedValue))));
}

JSC_DEFINE_JIT_OPERATION(operationGetWrappedValueForCaller, EncodedJSValue, (JSRemoteFunction* callee, EncodedJSValue encodedValue))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();

    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(isRemoteFunction(callee));

    OPERATION_RETURN(scope, JSValue::encode(getWrappedValue(globalObject, globalObject, JSValue::decode(encodedValue))));
}

static inline UGPRPair materializeTargetCode(VM& vm, JSFunction* targetFunction)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ExecutableBase* executable = targetFunction->executable();

    // Force the executable to cache its arity entrypoint.
    {
        DeferTraps deferTraps(vm); // We can't jettison any code until after we link the call.
        CodeBlock* codeBlockSlot = nullptr;
        if (!executable->isHostFunction()) {
            JSScope* scope = targetFunction->scopeUnchecked();
            FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
            functionExecutable->prepareForExecution<FunctionExecutable>(vm, targetFunction, scope, CodeForCall, codeBlockSlot);
            RETURN_IF_EXCEPTION(throwScope, encodeResult(nullptr, nullptr));
        }
        return encodeResult(executable->entrypointFor(CodeForCall, MustCheckArity).taggedPtr(), codeBlockSlot);
    }
}

JSC_DEFINE_JIT_OPERATION(operationMaterializeBoundFunctionTargetCode, UGPRPair, (JSBoundFunction* callee))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* targetFunction = jsCast<JSFunction*>(callee->targetFunction()); // We call this function only when JSBoundFunction's target is JSFunction.
    OPERATION_RETURN(scope, materializeTargetCode(vm, targetFunction));
}

JSC_DEFINE_JIT_OPERATION(operationMaterializeRemoteFunctionTargetCode, UGPRPair, (JSRemoteFunction* callee))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(isRemoteFunction(callee));
    auto* targetFunction = jsCast<JSFunction*>(callee->targetFunction()); // We call this function only when JSRemoteFunction's target is JSFunction.
    OPERATION_RETURN(scope, materializeTargetCode(vm, targetFunction));
}

JSC_DEFINE_JIT_OPERATION(operationThrowRemoteFunctionException, EncodedJSValue, (JSRemoteFunction* callee))
{
    JSGlobalObject* globalObject = callee->globalObject();
    VM& vm = globalObject->vm();

    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(isRemoteFunction(callee));

    Exception* exception = scope.exception();

    // We should only be here when "rethrowing" an exception
    RELEASE_ASSERT(exception);

    if (UNLIKELY(vm.isTerminationException(exception))) {
        scope.release();
        OPERATION_RETURN(scope, encodedJSValue());
    }

    JSValue exceptionValue = exception->value();
    scope.clearException();

    String exceptionString = exceptionValue.toWTFString(globalObject);
    Exception* toStringException = scope.exception();
    if (UNLIKELY(toStringException && vm.isTerminationException(toStringException))) {
        scope.release();
        OPERATION_RETURN(scope, encodedJSValue());
    }
    scope.clearException();

    if (exceptionString.length())
        OPERATION_RETURN(scope, throwVMTypeError(globalObject, scope, exceptionString));

    OPERATION_RETURN(scope, throwVMTypeError(globalObject, scope));
}

JSC_DEFINE_JIT_OPERATION(operationThrowIteratorResultIsNotObject, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationTryGetByIdGaveUp, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);


    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);
    baseValue.getPropertySlot(globalObject, identifier, slot);

    OPERATION_RETURN(scope, JSValue::encode(slot.getPureResult()));
}

JSC_DEFINE_JIT_OPERATION(operationTryGetByIdGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);
    baseValue.getPropertySlot(globalObject, identifier, slot);

    OPERATION_RETURN(scope, JSValue::encode(slot.getPureResult()));
}

JSC_DEFINE_JIT_OPERATION(operationTryGetByIdOptimize, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);

    baseValue.getPropertySlot(globalObject, identifier, slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier) && !slot.isTaintedByOpaqueObject() && (slot.isCacheableValue() || slot.isCacheableGetter() || slot.isUnset()))
        repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::TryById);

    OPERATION_RETURN(scope, JSValue::encode(slot.getPureResult()));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdDirectGaveUp, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, identifier, slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    OPERATION_RETURN(scope, JSValue::encode(found ? slot.getValue(globalObject, identifier) : jsUndefined()));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdDirectGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, identifier, slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    OPERATION_RETURN(scope, JSValue::encode(found ? slot.getValue(globalObject, identifier) : jsUndefined()));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdDirectOptimize, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, identifier, slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
        repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::ByIdDirect);

    OPERATION_RETURN(scope, JSValue::encode(found ? slot.getValue(globalObject, identifier) : jsUndefined()));
}

static ALWAYS_INLINE JSValue getByIdMegamorphic(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, JSValue baseValue, JSValue thisValue, CacheableIdentifier identifier, GetByKind kind)
{
    auto* uid = identifier.uid();
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);

    if (UNLIKELY(!baseValue.isObject())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        return baseValue.get(globalObject, uid, slot);
    }

    JSObject* baseObject = asObject(baseValue);
    JSObject* object = baseObject;
    bool cacheable = true;
    while (true) {
        if (UNLIKELY(TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()) && object->type() != ArrayType && object->type() != JSFunctionType && object != globalObject->arrayPrototype())) {
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
            if (object->getNonIndexPropertySlot(globalObject, uid, slot))
                return slot.getValue(globalObject, uid);
            return jsUndefined();
        }

        Structure* structure = object->structure();
        bool hasProperty = object->getOwnNonIndexPropertySlot(vm, structure, uid, slot);
        structure = object->structure(); // Reload it again since static-class-table can cause transition. But this transition only affects on this Structure.
        cacheable &= structure->propertyAccessesAreCacheable();
        if (hasProperty) {
            if (LIKELY(cacheable && slot.isCacheableValue() && slot.cachedOffset() <= MegamorphicCache::maxOffset)) {
                if (slot.slotBase() == baseObject || !baseObject->structure()->isDictionary())
                    vm.megamorphicCache()->initAsHit(baseObject->structureID(), uid, slot.slotBase(), slot.cachedOffset(), slot.slotBase() == baseObject);
                else {
                    if (UNLIKELY(baseObject->structure()->hasBeenFlattenedBefore())) {
                        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                            repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
                    }
                }
            } else {
                if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                    repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
            }
            return slot.getValue(globalObject, uid);
        }

        cacheable &= structure->propertyAccessesAreCacheableForAbsence();
        cacheable &= structure->hasMonoProto();

        JSValue prototype = object->getPrototypeDirect();
        if (!prototype.isObject()) {
            if (LIKELY(cacheable)) {
                if (LIKELY(!baseObject->structure()->isDictionary())) {
                    vm.megamorphicCache()->initAsMiss(baseObject->structureID(), uid);
                    return jsUndefined();
                }
                if (LIKELY(!baseObject->structure()->hasBeenFlattenedBefore()))
                    return jsUndefined();
            }
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
            return jsUndefined();
        }
        object = asObject(prototype);
    }
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdMegamorphic, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(base);
    CacheableIdentifier identifier = stubInfo->identifier();

    OPERATION_RETURN(scope, JSValue::encode(getByIdMegamorphic(globalObject, vm, callFrame, stubInfo, baseValue, baseValue, identifier, GetByKind::ById)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdMegamorphicGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(base);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    OPERATION_RETURN(scope, JSValue::encode(getByIdMegamorphic(globalObject, vm, callFrame, nullptr, baseValue, baseValue, identifier, GetByKind::ById)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdGaveUp, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    CacheableIdentifier identifier = stubInfo->identifier();
    JSValue result = baseValue.get(globalObject, identifier, slot);

    LOG_IC((vm, ICEvent::OperationGetById, baseValue.classInfoOrNull(), identifier, baseValue == slot.slotBase()));

    OPERATION_RETURN(scope, JSValue::encode(result));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    JSValue result = baseValue.get(globalObject, identifier, slot);
    
    LOG_IC((vm, ICEvent::OperationGetByIdGeneric, baseValue.classInfoOrNull(), identifier, baseValue == slot.slotBase()));
    
    OPERATION_RETURN(scope, JSValue::encode(result));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdOptimize, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);

    OPERATION_RETURN(scope, JSValue::encode(baseValue.getPropertySlot(globalObject, identifier, [&] (bool found, PropertySlot& slot) -> JSValue {
        
        LOG_IC((vm, ICEvent::OperationGetByIdOptimize, baseValue.classInfoOrNull(), identifier, baseValue == slot.slotBase()));
        
        CodeBlock* codeBlock = callFrame->codeBlock();
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::ById);
        return found ? slot.getValue(globalObject, identifier) : jsUndefined();
    })));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisGaveUp, EncodedJSValue, (EncodedJSValue base, EncodedJSValue thisEncoded, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    JSValue thisValue = JSValue::decode(thisEncoded);
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);

    OPERATION_RETURN(scope, JSValue::encode(baseValue.get(globalObject, identifier, slot)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue thisEncoded, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    JSValue baseValue = JSValue::decode(base);
    JSValue thisValue = JSValue::decode(thisEncoded);
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);

    OPERATION_RETURN(scope, JSValue::encode(baseValue.get(globalObject, identifier, slot)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisOptimize, EncodedJSValue, (EncodedJSValue base, EncodedJSValue thisEncoded, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    JSValue thisValue = JSValue::decode(thisEncoded);

    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);
    OPERATION_RETURN(scope, JSValue::encode(baseValue.getPropertySlot(globalObject, identifier, slot, [&] (bool found, PropertySlot& slot) -> JSValue {
        LOG_IC((vm, ICEvent::OperationGetByIdWithThisOptimize, baseValue.classInfoOrNull(), identifier, baseValue == slot.slotBase()));
        
        CodeBlock* codeBlock = callFrame->codeBlock();
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::ByIdWithThis);
        return found ? slot.getValue(globalObject, identifier) : jsUndefined();
    })));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisMegamorphic, EncodedJSValue, (EncodedJSValue base, EncodedJSValue encodedThis, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();

    OPERATION_RETURN(scope, JSValue::encode(getByIdMegamorphic(globalObject, vm, callFrame, stubInfo, JSValue::decode(base), JSValue::decode(encodedThis), identifier, GetByKind::ByIdWithThis)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisMegamorphicGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue encodedThis, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    OPERATION_RETURN(scope, JSValue::encode(getByIdMegamorphic(globalObject, vm, callFrame, nullptr, JSValue::decode(base), JSValue::decode(encodedThis), identifier, GetByKind::ByIdWithThis)));
}

JSC_DEFINE_JIT_OPERATION(operationInByIdGaveUp, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, JSValue::encode(jsUndefined()));
    }
    JSObject* baseObject = asObject(baseValue);

    LOG_IC((vm, ICEvent::OperationInByIdGeneric, baseObject->classInfo(), identifier));

    scope.release();
    PropertySlot slot(baseObject, PropertySlot::InternalMethodType::HasProperty);
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(baseObject->getPropertySlot(globalObject, identifier, slot))));
}

JSC_DEFINE_JIT_OPERATION(operationInByIdOptimize, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue baseValue = JSValue::decode(base);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, JSValue::encode(jsUndefined()));
    }
    JSObject* baseObject = asObject(baseValue);

    LOG_IC((vm, ICEvent::OperationInByIdOptimize, baseObject->classInfo(), identifier));

    PropertySlot slot(baseObject, PropertySlot::InternalMethodType::HasProperty);
    bool found = baseObject->getPropertySlot(globalObject, identifier, slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseObject->structure(), identifier))
        repatchInBy(globalObject, codeBlock, baseObject, identifier, found, slot, *stubInfo, InByKind::ById);
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(found)));
}

static ALWAYS_INLINE JSValue inByIdMegamorphic(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, JSValue baseValue, CacheableIdentifier identifier)
{
    constexpr bool verbose = false;
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* uid = identifier.uid();
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::HasProperty);

    if (UNLIKELY(!baseValue.isObject())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
            dataLogLnIf(verbose, " ", __LINE__);
            repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ById);
        }
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        return jsUndefined();
    }

    JSObject* baseObject = asObject(baseValue);
    JSObject* object = baseObject;
    bool cacheable = true;
    while (true) {
        if (UNLIKELY(TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()) && object->type() != ArrayType && object->type() != JSFunctionType && object != globalObject->arrayPrototype())) {
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                dataLogLnIf(verbose, " ", __LINE__);
                repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ById);
            }
            RELEASE_AND_RETURN(scope, jsBoolean(object->getNonIndexPropertySlot(globalObject, uid, slot)));
        }

        Structure* structure = object->structure();
        bool hasProperty = object->getOwnNonIndexPropertySlot(vm, structure, uid, slot);
        structure = object->structure(); // Reload it again since static-class-table can cause transition. But this transition only affects on this Structure.
        cacheable &= structure->propertyAccessesAreCacheable();
        if (hasProperty) {
            if (LIKELY(cacheable && slot.isCacheable())) {
                if (slot.slotBase() == baseObject || !baseObject->structure()->isDictionary())
                    vm.megamorphicCache()->initAsHasHit(baseObject->structureID(), uid);
                else {
                    if (UNLIKELY(baseObject->structure()->hasBeenFlattenedBefore())) {
                        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                            dataLogLnIf(verbose, " ", __LINE__);
                            repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ById);
                        }
                    }
                    dataLogLnIf(verbose, " ", __LINE__);
                }
            } else {
                if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                    dataLogLnIf(verbose, " ", __LINE__);
                    repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ById);
                }
            }
            return jsBoolean(true);
        }

        cacheable &= structure->propertyAccessesAreCacheableForAbsence();
        cacheable &= structure->hasMonoProto();

        JSValue prototype = object->getPrototypeDirect();
        if (!prototype.isObject()) {
            if (LIKELY(cacheable)) {
                if (LIKELY(!baseObject->structure()->isDictionary())) {
                    vm.megamorphicCache()->initAsHasMiss(baseObject->structureID(), uid);
                    return jsBoolean(false);
                }
                if (LIKELY(!baseObject->structure()->hasBeenFlattenedBefore()))
                    return jsBoolean(false);
            }
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                dataLogLnIf(verbose, " ", __LINE__);
                repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ById);
            }
            return jsBoolean(false);
        }
        object = asObject(prototype);
    }
}

JSC_DEFINE_JIT_OPERATION(operationInByIdMegamorphic, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(base);
    CacheableIdentifier identifier = stubInfo->identifier();

    OPERATION_RETURN(scope, JSValue::encode(inByIdMegamorphic(globalObject, vm, callFrame, stubInfo, baseValue, identifier)));
}

JSC_DEFINE_JIT_OPERATION(operationInByIdMegamorphicGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(base);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    OPERATION_RETURN(scope, JSValue::encode(inByIdMegamorphic(globalObject, vm, callFrame, nullptr, baseValue, identifier)));
}

JSC_DEFINE_JIT_OPERATION(operationInByValOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedKey, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue key = JSValue::decode(encodedKey);

    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, encodedJSValue());
    }

    JSObject* baseObject = asObject(baseValue);

    uint32_t i;
    if (key.getUInt32(i)) {
        if (profile)
            profile->observeIndexedRead(baseObject, i);

        CodeBlock* codeBlock = callFrame->codeBlock();
        Structure* structure = baseValue.asCell()->structure();
        if (stubInfo->considerRepatchingCacheGeneric(vm, codeBlock, structure)) {
            if (profile)
                profile->computeUpdatedPrediction(codeBlock, structure);
            repatchArrayInByVal(globalObject, codeBlock, baseValue, key, *stubInfo, InByKind::ByVal);
        }

        OPERATION_RETURN(scope, JSValue::encode(jsBoolean(baseObject->hasProperty(globalObject, i))));
    }

    const Identifier propertyName = key.toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    PropertySlot slot(baseObject, PropertySlot::InternalMethodType::HasProperty);
    bool found = baseObject->getPropertySlot(globalObject, propertyName, slot);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (CacheableIdentifier::isCacheableIdentifierCell(key) && (key.isSymbol() || !parseIndex(propertyName))) {
        CodeBlock* codeBlock = callFrame->codeBlock();
        CacheableIdentifier identifier = CacheableIdentifier::createFromCell(key.asCell());
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseObject->structure(), identifier))
            repatchInBy(globalObject, codeBlock, baseObject, identifier, found, slot, *stubInfo, InByKind::ByVal);
    }

    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(found)));
}

JSC_DEFINE_JIT_OPERATION(operationInByValGaveUp, EncodedJSValue, (EncodedJSValue base, EncodedJSValue key, StructureStubInfo* stubInfo, ArrayProfile* arrayProfile))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(CommonSlowPaths::opInByVal(globalObject, JSValue::decode(base), JSValue::decode(key), arrayProfile))));
}

static ALWAYS_INLINE JSValue inByValMegamorphic(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, ArrayProfile* profile, JSValue baseValue, JSValue subscript)
{
    constexpr bool verbose = false;
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!baseValue.isObject() || !(subscript.isString() && CacheableIdentifier::isCacheableIdentifierCell(subscript)))) {
        dataLogLnIf(verbose, " ", __LINE__);
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
            repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ByVal);
        }
        RELEASE_AND_RETURN(scope, jsBoolean(CommonSlowPaths::opInByVal(globalObject, baseValue, subscript, profile)));
    }

    Identifier propertyName = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* baseObject = asObject(baseValue);
    UniquedStringImpl* uid = propertyName.impl();
    if (UNLIKELY(!canUseMegamorphicInById(vm, uid))) {
        dataLogLnIf(verbose, " ", __LINE__);
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
            repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ByVal);
        }
        if (profile)
            profile->observeStructure(baseObject->structure());
        RELEASE_AND_RETURN(scope, jsBoolean(baseObject->hasProperty(globalObject, uid)));
    }

    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::HasProperty);
    JSObject* object = baseObject;
    bool cacheable = true;
    while (true) {
        if (UNLIKELY(TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()) && object->type() != ArrayType && object->type() != JSFunctionType && object != globalObject->arrayPrototype())) {
            dataLogLnIf(verbose, " ", __LINE__);
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ByVal);
            }
            RELEASE_AND_RETURN(scope, jsBoolean(object->getNonIndexPropertySlot(globalObject, uid, slot)));
        }

        Structure* structure = object->structure();
        bool hasProperty = object->getOwnNonIndexPropertySlot(vm, structure, uid, slot);
        structure = object->structure(); // Reload it again since static-class-table can cause transition. But this transition only affects on this Structure.
        cacheable &= structure->propertyAccessesAreCacheable();
        if (hasProperty) {
            if (LIKELY(cacheable && slot.isCacheable())) {
                if (slot.slotBase() == baseObject || !baseObject->structure()->isDictionary())
                    vm.megamorphicCache()->initAsHasHit(baseObject->structureID(), uid);
                else {
                    if (UNLIKELY(baseObject->structure()->hasBeenFlattenedBefore())) {
                        dataLogLnIf(verbose, " ", __LINE__);
                        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                            repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ByVal);
                        }
                    }
                }
            } else {
                dataLogLnIf(verbose, " ", __LINE__);
                if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                    repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ByVal);
                }
            }
            return jsBoolean(true);
        }

        cacheable &= structure->propertyAccessesAreCacheableForAbsence();
        cacheable &= structure->hasMonoProto();

        JSValue prototype = object->getPrototypeDirect();
        if (!prototype.isObject()) {
            if (LIKELY(cacheable)) {
                if (LIKELY(!baseObject->structure()->isDictionary())) {
                    vm.megamorphicCache()->initAsHasMiss(baseObject->structureID(), uid);
                    return jsBoolean(false);
                }
                if (LIKELY(!baseObject->structure()->hasBeenFlattenedBefore()))
                    return jsBoolean(false);
            }

            dataLogLnIf(verbose, " ", __LINE__);
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm)) {
                repatchInBySlowPathCall(callFrame->codeBlock(), *stubInfo, InByKind::ByVal);
            }
            return jsBoolean(false);
        }
        object = asObject(prototype);
    }
}

JSC_DEFINE_JIT_OPERATION(operationInByValMegamorphic, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo, ArrayProfile* arrayProfile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    OPERATION_RETURN(scope, JSValue::encode(inByValMegamorphic(globalObject, vm, callFrame, stubInfo, arrayProfile, baseValue, subscript)));
}

JSC_DEFINE_JIT_OPERATION(operationInByValMegamorphicGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue baseValue = JSValue::decode(encodedBase);
    OPERATION_RETURN(scope, JSValue::encode(inByValMegamorphic(globalObject, vm, callFrame, nullptr, nullptr, baseValue, JSValue::decode(encodedSubscript))));
}

JSC_DEFINE_JIT_OPERATION(operationHasPrivateNameOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedProperty, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, encodedJSValue());
    }
    JSObject* baseObject = asObject(baseValue);

    JSValue propertyValue = JSValue::decode(encodedProperty);
    ASSERT(propertyValue.isSymbol());
    auto property = propertyValue.toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    PropertySlot slot(baseObject, PropertySlot::InternalMethodType::HasProperty);
    bool found = JSObject::getPrivateFieldSlot(baseObject, globalObject, property, slot);

    ASSERT(CacheableIdentifier::isCacheableIdentifierCell(propertyValue));
    CodeBlock* codeBlock = callFrame->codeBlock();
    CacheableIdentifier identifier = CacheableIdentifier::createFromCell(propertyValue.asCell());
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseObject->structure(), identifier))
        repatchInBy(globalObject, codeBlock, baseObject, identifier, found, slot, *stubInfo, InByKind::PrivateName);

    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(found)));
}

JSC_DEFINE_JIT_OPERATION(operationHasPrivateNameGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedProperty, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBase);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, encodedJSValue());
    }

    JSValue propertyValue = JSValue::decode(encodedProperty);
    ASSERT(propertyValue.isSymbol());
    auto property = propertyValue.toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(asObject(baseValue)->hasPrivateField(globalObject, property))));
}

JSC_DEFINE_JIT_OPERATION(operationHasPrivateBrandOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedBrand, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, encodedJSValue());
    }
    JSObject* baseObject = asObject(baseValue);

    JSValue brand = JSValue::decode(encodedBrand);
    bool found = asObject(baseValue)->hasPrivateBrand(globalObject, brand);

    ASSERT(CacheableIdentifier::isCacheableIdentifierCell(brand));
    CodeBlock* codeBlock = callFrame->codeBlock();
    CacheableIdentifier identifier = CacheableIdentifier::createFromCell(brand.asCell());
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseObject->structure(), identifier))
        repatchHasPrivateBrand(globalObject, codeBlock, baseObject, identifier, found, *stubInfo);

    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(found)));
}

JSC_DEFINE_JIT_OPERATION(operationHasPrivateBrandGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedBrand, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBase);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        OPERATION_RETURN(scope, encodedJSValue());
    }

    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(asObject(baseValue)->hasPrivateBrand(globalObject, JSValue::decode(encodedBrand)))));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = stubInfo->identifier();
    PutPropertySlot slot(baseValue, true, callFrame->codeBlock()->putByIdContext());
    baseValue.putInline(globalObject, identifier, JSValue::decode(encodedValue), slot);
    
    LOG_IC((vm, ICEvent::OperationPutByIdStrict, baseValue.classInfoOrNull(), identifier, slot.base() == baseValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSloppyGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = stubInfo->identifier();
    PutPropertySlot slot(baseValue, false, callFrame->codeBlock()->putByIdContext());
    baseValue.putInline(globalObject, identifier, JSValue::decode(encodedValue), slot);

    LOG_IC((vm, ICEvent::OperationPutByIdSloppy, baseValue.classInfoOrNull(), identifier, slot.base() == baseValue));
    OPERATION_RETURN(scope);
}

ALWAYS_INLINE static void putByIdMegamorphic(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, JSValue baseValue, JSValue value, CacheableIdentifier identifier, PutByKind kind)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* uid = identifier.uid();
    bool isStrict = kind == PutByKind::ByIdStrict;
    PutPropertySlot slot(baseValue, isStrict, callFrame->codeBlock()->putByIdContext());

    if (UNLIKELY(!baseValue.isObject())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        scope.release();
        baseValue.put(globalObject, uid, value, slot);
        return;
    }

    JSObject* baseObject = asObject(baseValue);
    Structure* structure = baseObject->structure();

    if (UNLIKELY(structure->typeInfo().overridesPut())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        scope.release();
        baseValue.put(globalObject, uid, value, slot);
        return;
    }

    {
        JSObject* object = baseObject;
        while (true) {
            if (UNLIKELY(structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().overridesGetPrototype() || structure->typeInfo().overridesPut() || structure->hasPolyProto())) {
                if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                    repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
                scope.release();
                baseObject->putInlineSlow(globalObject, uid, value, slot);
                return;
            }
            JSValue prototype = object->getPrototypeDirect();
            if (prototype.isNull())
                break;
            object = asObject(prototype);
            structure = object->structure();
        }
    }

    Structure* oldStructure = baseObject->structure();
    baseObject->putInlineFast(globalObject, uid, value, slot);
    RETURN_IF_EXCEPTION(scope, void());

    if (UNLIKELY(!slot.isCacheablePut() || !oldStructure->propertyAccessesAreCacheable())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        return;
    }

    Structure* newStructure = baseObject->structure();
    if (slot.type() == PutPropertySlot::ExistingProperty) {
        if (LIKELY(oldStructure == newStructure && slot.cachedOffset() <= MegamorphicCache::maxOffset)) {
            oldStructure->didCachePropertyReplacement(vm, slot.cachedOffset()); // Ensure invalidating watchpoint set.
            vm.megamorphicCache()->initAsReplace(StructureID::encode(oldStructure), uid, slot.cachedOffset());
        }
        return;
    }

    ASSERT(slot.type() == PutPropertySlot::NewProperty);
    // This is not worth registering. Dictionary Structure is one-on-one to this object. And NewProperty happens only once.
    // So this cache will be never used again.
    if (oldStructure->isDictionary() || newStructure->isDictionary())
        return;

    if (UNLIKELY(oldStructure->mayBePrototype() || (newStructure->previousID() != oldStructure) || !newStructure->propertyAccessesAreCacheable())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        return;
    }

    bool reallocating = newStructure->outOfLineCapacity() != oldStructure->outOfLineCapacity();
    if (LIKELY(slot.cachedOffset() <= MegamorphicCache::maxOffset))
        vm.megamorphicCache()->initAsTransition(StructureID::encode(oldStructure), StructureID::encode(newStructure), uid, slot.cachedOffset(), reallocating);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdStrictMegamorphic, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue value = JSValue::decode(encodedValue);
    CacheableIdentifier identifier = stubInfo->identifier();

    putByIdMegamorphic(globalObject, vm, callFrame, stubInfo, baseValue, value, identifier, PutByKind::ByIdStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdStrictMegamorphicGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue value = JSValue::decode(encodedValue);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    putByIdMegamorphic(globalObject, vm, callFrame, nullptr, baseValue, value, identifier, PutByKind::ByIdStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSloppyMegamorphic, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue value = JSValue::decode(encodedValue);
    CacheableIdentifier identifier = stubInfo->identifier();

    putByIdMegamorphic(globalObject, vm, callFrame, stubInfo, baseValue, value, identifier, PutByKind::ByIdSloppy);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSloppyMegamorphicGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue value = JSValue::decode(encodedValue);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    putByIdMegamorphic(globalObject, vm, callFrame, nullptr, baseValue, value, identifier, PutByKind::ByIdSloppy);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByMegamorphicReallocating, void, (VM* vmPointer, JSObject* baseObject, EncodedJSValue encodedValue, const MegamorphicCache::StoreEntry* entry))
{
    constexpr bool verbose = false;
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* oldStructure = WTF::opaque(baseObject->structure());
    Structure* newStructure = WTF::opaque(entry->m_newStructureID.decode());
    PropertyOffset offset = entry->m_offset;

    ASSERT(oldStructure == entry->m_oldStructureID.decode());
    Butterfly* newButterfly = baseObject->allocateMoreOutOfLineStorage(vm, oldStructure->outOfLineCapacity(), newStructure->outOfLineCapacity());
    baseObject->nukeStructureAndSetButterfly(vm, StructureID::encode(oldStructure), newButterfly);
    baseObject->putDirectOffset(vm, offset, JSValue::decode(encodedValue));
    baseObject->setStructure(vm, newStructure);
    ASSERT(newStructure == baseObject->structure());
    dataLogLnIf(verbose, JSValue(baseObject), " ", offset);

    ensureStillAliveHere(oldStructure);
    ensureStillAliveHere(newStructure);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = stubInfo->identifier();
    PutPropertySlot slot(baseValue, true, callFrame->codeBlock()->putByIdContext());
    CommonSlowPaths::putDirectWithReify(vm, globalObject, asObject(baseValue), identifier, JSValue::decode(encodedValue), slot);

    LOG_IC((vm, ICEvent::OperationPutByIdDirectStrict, baseValue.classInfoOrNull(), identifier, slot.base() == baseValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectSloppyGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = stubInfo->identifier();
    PutPropertySlot slot(baseValue, false, callFrame->codeBlock()->putByIdContext());
    CommonSlowPaths::putDirectWithReify(vm, globalObject, asObject(baseValue), identifier, JSValue::decode(encodedValue), slot);

    LOG_IC((vm, ICEvent::OperationPutByIdDirectSloppy, baseValue.classInfoOrNull(), identifier, slot.base() == baseValue));
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseValue, true, codeBlock->putByIdContext());

    Structure* structure = CommonSlowPaths::originalStructureBeforePut(baseValue);
    baseValue.putInline(globalObject, identifier, value, slot);

    LOG_IC((vm, ICEvent::OperationPutByIdStrictOptimize, baseValue.classInfoOrNull(), identifier, slot.base() == baseValue));

    OPERATION_RETURN_IF_EXCEPTION(scope);

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        OPERATION_RETURN(scope);
    
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
        repatchPutBy(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, PutByKind::ByIdStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSloppyOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseValue, false, codeBlock->putByIdContext());

    Structure* structure = CommonSlowPaths::originalStructureBeforePut(baseValue);
    baseValue.putInline(globalObject, identifier, value, slot);

    LOG_IC((vm, ICEvent::OperationPutByIdSloppyOptimize, baseValue.classInfoOrNull(), identifier, slot.base() == baseValue));

    OPERATION_RETURN_IF_EXCEPTION(scope);

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        OPERATION_RETURN(scope);
    
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
        repatchPutBy(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, PutByKind::ByIdSloppy);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    CacheableIdentifier identifier = stubInfo->identifier();
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseObject, true, codeBlock->putByIdContext());
    Structure* structure = nullptr;
    CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, identifier, value, slot, &structure);

    LOG_IC((vm, ICEvent::OperationPutByIdDirectStrictOptimize, baseObject->classInfo(), identifier, slot.base() == baseObject));

    OPERATION_RETURN_IF_EXCEPTION(scope);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        OPERATION_RETURN(scope);
    
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
        repatchPutBy(globalObject, codeBlock, baseObject, structure, identifier, slot, *stubInfo, PutByKind::ByIdDirectStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectSloppyOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    CacheableIdentifier identifier = stubInfo->identifier();
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseObject, false, codeBlock->putByIdContext());
    Structure* structure = nullptr;
    CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, identifier, value, slot, &structure);

    LOG_IC((vm, ICEvent::OperationPutByIdDirectSloppyOptimize, baseObject->classInfo(), identifier, slot.base() == baseObject));

    OPERATION_RETURN_IF_EXCEPTION(scope);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        OPERATION_RETURN(scope);
    
    if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
        repatchPutBy(globalObject, codeBlock, baseObject, structure, identifier, slot, *stubInfo, PutByKind::ByIdDirectSloppy);
    OPERATION_RETURN(scope);
}

template<typename PutPrivateFieldCallback>
ALWAYS_INLINE static void setPrivateField(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, CacheableIdentifier identifier, JSValue value, PutPrivateFieldCallback callback)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(identifier.isPrivateName());

    JSObject* baseObject = baseValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    CodeBlock* codeBlock = callFrame->codeBlock();
    Structure* oldStructure = baseObject->structure();

    PutPropertySlot putSlot(baseObject, true, codeBlock->putByIdContext());
    baseObject->setPrivateField(globalObject, identifier, value, putSlot);
    RETURN_IF_EXCEPTION(scope, void());

    callback(vm, codeBlock, oldStructure, putSlot);
}

template<typename PutPrivateFieldCallback>
ALWAYS_INLINE static void definePrivateField(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, CacheableIdentifier identifier, JSValue value, PutPrivateFieldCallback callback)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(identifier.isPrivateName());

    JSObject* baseObject = baseValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    CodeBlock* codeBlock = callFrame->codeBlock();
    Structure* oldStructure = baseObject->structure();

    PutPropertySlot putSlot(baseObject, true, codeBlock->putByIdContext());
    baseObject->definePrivateField(globalObject, identifier, value, putSlot);
    RETURN_IF_EXCEPTION(scope, void());

    callback(vm, codeBlock, oldStructure, putSlot);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDefinePrivateFieldStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);

    definePrivateField(vm, globalObject, callFrame, baseValue, identifier, value, [](VM&, CodeBlock*, Structure*, PutPropertySlot&) { });
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDefinePrivateFieldStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    
    definePrivateField(vm, globalObject, callFrame, baseValue, identifier, value, [&](VM& vm, CodeBlock* codeBlock, Structure* oldStructure, PutPropertySlot& putSlot) {
        JSObject* baseObject = asObject(baseValue);
        LOG_IC((vm, ICEvent::OperationPutByIdDefinePrivateFieldStrictOptimize, baseObject->classInfo(), identifier, putSlot.base() == baseObject));

        ASSERT_UNUSED(accessType, accessType == static_cast<AccessType>(stubInfo->accessType));

        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, oldStructure, identifier))
            repatchPutBy(globalObject, codeBlock, baseObject, oldStructure, identifier, putSlot, *stubInfo, PutByKind::DefinePrivateNameById);
    });
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSetPrivateFieldStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);

    setPrivateField(vm, globalObject, callFrame, baseValue, identifier, value, [](VM&, CodeBlock*, Structure*, PutPropertySlot&) { });
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSetPrivateFieldStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);

    setPrivateField(vm, globalObject, callFrame, baseValue, identifier, value, [&](VM& vm, CodeBlock* codeBlock, Structure* oldStructure, PutPropertySlot& putSlot) {
        JSObject* baseObject = asObject(baseValue);
        LOG_IC((vm, ICEvent::OperationPutByIdSetPrivateFieldStrictOptimize, baseObject->classInfo(), identifier, putSlot.base() == baseObject));

        ASSERT_UNUSED(accessType, accessType == static_cast<AccessType>(stubInfo->accessType));

        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, oldStructure, identifier))
            repatchPutBy(globalObject, codeBlock, baseObject, oldStructure, identifier, putSlot, *stubInfo, PutByKind::SetPrivateNameById);
    });
    OPERATION_RETURN(scope);
}

static void putByVal(JSGlobalObject* globalObject, JSValue baseValue, JSValue subscript, JSValue value, ArrayProfile* arrayProfile, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (std::optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        uint32_t i = *index;
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->trySetIndexQuickly(vm, i, value, arrayProfile))
                return;

            if (arrayProfile)
                arrayProfile->setOutOfBounds();
            scope.release();
            object->methodTable()->putByIndex(object, globalObject, i, value, ecmaMode.isStrict());
            return;
        }

        scope.release();
        baseValue.putByIndex(globalObject, i, value, ecmaMode.isStrict());
        return;
    }

    if (arrayProfile) {
        if (subscript.isNumber()) {
            if (baseValue.isObject())
                arrayProfile->setOutOfBounds();
        }
    }

    auto property = subscript.toPropertyKey(globalObject);
    // Don't put to an object if toString threw an exception.
    RETURN_IF_EXCEPTION(scope, void());

    scope.release();
    PutPropertySlot slot(baseValue, ecmaMode.isStrict());
    baseValue.putInline(globalObject, property, value, slot);
}

static void directPutByVal(JSGlobalObject* globalObject, JSObject* baseObject, JSValue subscript, JSValue value, ArrayProfile* arrayProfile, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (std::optional<uint32_t> maybeIndex = subscript.tryGetAsUint32Index()) {
        uint32_t index = *maybeIndex;

        switch (baseObject->indexingType()) {
        case ALL_INT32_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            if (index < baseObject->butterfly()->vectorLength())
                break;
            FALLTHROUGH;
        default:
            if (arrayProfile)
                arrayProfile->setOutOfBounds();
            break;
        }

        scope.release();
        baseObject->putDirectIndex(globalObject, index, value, 0, ecmaMode.isStrict() ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }

    // Don't put to an object if toString threw an exception.
    auto property = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    if (std::optional<uint32_t> index = parseIndex(property)) {
        scope.release();
        baseObject->putDirectIndex(globalObject, index.value(), value, 0, ecmaMode.isStrict() ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }

    scope.release();
    PutPropertySlot slot(baseObject, ecmaMode.isStrict());
    CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, property, value, slot);
}

enum class OptimizationResult {
    NotOptimized,
    SeenOnce,
    Optimized,
    GiveUp,
};

static ALWAYS_INLINE void putByValOptimize(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue subscript, JSValue value, StructureStubInfo* stubInfo, ArrayProfile* profile, PutByKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool isStrict = kind == PutByKind::ByValStrict;

    if (baseValue.isObject()) {
        JSObject* baseObject = asObject(baseValue);
        if (!isCopyOnWrite(baseObject->indexingMode()) && subscript.isInt32()) {
            Structure* structure = baseObject->structure();
            if (stubInfo->considerRepatchingCacheGeneric(vm, codeBlock, structure)) {
                if (profile)
                    profile->computeUpdatedPrediction(codeBlock, structure);
                repatchArrayPutByVal(globalObject, codeBlock, baseValue, subscript, *stubInfo, kind);
            }
        }

        if (CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
            const Identifier propertyName = subscript.toPropertyKey(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
            if (subscript.isSymbol() || !parseIndex(propertyName)) {
                AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
                PutPropertySlot slot(baseValue, isStrict, codeBlock->putByIdContext());

                Structure* structure = CommonSlowPaths::originalStructureBeforePut(baseValue);
                baseObject->putInline(globalObject, propertyName, value, slot);
                RETURN_IF_EXCEPTION(scope, void());

                if (accessType != static_cast<AccessType>(stubInfo->accessType))
                    return;

                CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
                if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
                    repatchPutBy(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, kind);
                return;
            }
        }
    }

    RELEASE_AND_RETURN(scope, putByVal(globalObject, baseValue, subscript, value, profile, isStrict ? ECMAMode::strict() : ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValStrictOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByValOptimize(globalObject, callFrame->codeBlock(), baseValue, subscript, value, stubInfo, profile, PutByKind::ByValStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSloppyOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByValOptimize(globalObject, callFrame->codeBlock(), baseValue, subscript, value, stubInfo, profile, PutByKind::ByValSloppy);
    OPERATION_RETURN(scope);
}

static ALWAYS_INLINE void directPutByValOptimize(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue subscript, JSValue value, StructureStubInfo* stubInfo, ArrayProfile* profile, PutByKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool isStrict = kind == PutByKind::ByValDirectStrict;

    RELEASE_ASSERT(baseValue.isObject());
    JSObject* baseObject = asObject(baseValue);

    if (!isCopyOnWrite(baseObject->indexingMode()) && subscript.isInt32()) {
        Structure* structure = baseObject->structure();
        if (stubInfo->considerRepatchingCacheGeneric(vm, codeBlock, structure)) {
            if (profile)
                profile->computeUpdatedPrediction(codeBlock, structure);
            repatchArrayPutByVal(globalObject, codeBlock, baseValue, subscript, *stubInfo, kind);
        }
    }

    if (CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
            PutPropertySlot slot(baseValue, isStrict, codeBlock->putByIdContext());

            Structure* structure = CommonSlowPaths::originalStructureBeforePut(baseValue);
            CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, propertyName, value, slot);

            RETURN_IF_EXCEPTION(scope, void());

            if (accessType != static_cast<AccessType>(stubInfo->accessType))
                return;

            CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
            if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
                repatchPutBy(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, kind);
            return;
        }
    }

    RELEASE_AND_RETURN(scope, directPutByVal(globalObject, baseObject, subscript, value, profile, isStrict ? ECMAMode::strict() : ECMAMode::sloppy()));

}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValStrictOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    directPutByValOptimize(globalObject, callFrame->codeBlock(), baseValue, subscript, value, stubInfo, profile, PutByKind::ByValDirectStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValSloppyOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    directPutByValOptimize(globalObject, callFrame->codeBlock(), baseValue, subscript, value, stubInfo, profile, PutByKind::ByValDirectSloppy);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValStrictGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(globalObject, baseValue, subscript, value, profile, ECMAMode::strict());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValStrictGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(globalObject, baseValue, subscript, value, nullptr, ECMAMode::strict());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSloppyGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(globalObject, baseValue, subscript, value, profile, ECMAMode::sloppy());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSloppyGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(globalObject, baseValue, subscript, value, nullptr, ECMAMode::sloppy());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValStrictGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());

    directPutByVal(globalObject, asObject(baseValue), subscript, value, profile, ECMAMode::strict());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValStrictGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());

    directPutByVal(globalObject, asObject(baseValue), subscript, value, nullptr, ECMAMode::strict());
    OPERATION_RETURN(scope);
}

ALWAYS_INLINE static void putByValMegamorphic(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, ArrayProfile* profile, JSValue baseValue, JSValue subscript, JSValue value, PutByKind kind)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool isStrict = kind == PutByKind::ByValStrict;

    if (UNLIKELY(!baseValue.isObject() || !(subscript.isString() && CacheableIdentifier::isCacheableIdentifierCell(subscript)))) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        scope.release();
        putByVal(globalObject, baseValue, subscript, value, profile, isStrict ? ECMAMode::strict() : ECMAMode::sloppy());
        return;
    }

    Identifier propertyName = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* baseObject = asObject(baseValue);
    Structure* structure = baseObject->structure();

    PutPropertySlot slot(baseValue, isStrict);

    UniquedStringImpl* uid = propertyName.impl();
    if (UNLIKELY(!canUseMegamorphicPutById(vm, uid) || structure->typeInfo().overridesPut())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        scope.release();
        baseValue.put(globalObject, uid, value, slot);
        return;
    }

    {
        JSObject* object = baseObject;
        while (true) {
            if (UNLIKELY(structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().overridesGetPrototype() || structure->typeInfo().overridesPut() || structure->hasPolyProto())) {
                if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                    repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
                scope.release();
                baseObject->putInlineSlow(globalObject, uid, value, slot);
                return;
            }
            JSValue prototype = object->getPrototypeDirect();
            if (prototype.isNull())
                break;
            object = asObject(prototype);
            structure = object->structure();
        }
    }

    Structure* oldStructure = baseObject->structure();
    baseObject->putInlineFast(globalObject, uid, value, slot);
    RETURN_IF_EXCEPTION(scope, void());

    if (UNLIKELY(!slot.isCacheablePut() || !oldStructure->propertyAccessesAreCacheable())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        return;
    }

    Structure* newStructure = baseObject->structure();
    if (slot.type() == PutPropertySlot::ExistingProperty) {
        if (LIKELY(oldStructure == newStructure && slot.cachedOffset() <= MegamorphicCache::maxOffset)) {
            oldStructure->didCachePropertyReplacement(vm, slot.cachedOffset()); // Ensure invalidating watchpoint set.
            vm.megamorphicCache()->initAsReplace(StructureID::encode(oldStructure), uid, slot.cachedOffset());
        }
        return;
    }

    ASSERT(slot.type() == PutPropertySlot::NewProperty);
    // This is not worth registering. Dictionary Structure is one-on-one to this object. And NewProperty happens only once.
    // So this cache will be never used again.
    if (oldStructure->isDictionary() || newStructure->isDictionary())
        return;

    if (UNLIKELY(oldStructure->mayBePrototype() || (newStructure->previousID() != oldStructure) || !newStructure->propertyAccessesAreCacheable())) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchPutBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        return;
    }

    bool reallocating = newStructure->outOfLineCapacity() != oldStructure->outOfLineCapacity();
    if (LIKELY(slot.cachedOffset() <= MegamorphicCache::maxOffset))
        vm.megamorphicCache()->initAsTransition(StructureID::encode(oldStructure), StructureID::encode(newStructure), uid, slot.cachedOffset(), reallocating);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValStrictMegamorphic, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByValMegamorphic(globalObject, vm, callFrame, stubInfo, profile, baseValue, subscript, value, PutByKind::ByValStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValStrictMegamorphicGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByValMegamorphic(globalObject, vm, callFrame, nullptr, nullptr, baseValue, subscript, value, PutByKind::ByValStrict);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSloppyMegamorphic, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByValMegamorphic(globalObject, vm, callFrame, stubInfo, profile, baseValue, subscript, value, PutByKind::ByValSloppy);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSloppyMegamorphicGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByValMegamorphic(globalObject, vm, callFrame, nullptr, nullptr, baseValue, subscript, value, PutByKind::ByValSloppy);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValSloppyGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());

    directPutByVal(globalObject, asObject(baseValue), subscript, value, profile, ECMAMode::sloppy());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValSloppyGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());

    directPutByVal(globalObject, asObject(baseValue), subscript, value, nullptr, ECMAMode::sloppy());
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationSetPrivateBrandOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedBrand, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue brand = JSValue::decode(encodedBrand);

    ASSERT(baseValue.isObject());
    ASSERT(brand.isSymbol());

    JSObject* baseObject = asObject(baseValue);
    Structure* oldStructure = baseObject->structure();
    baseObject->setPrivateBrand(globalObject, brand);
    OPERATION_RETURN_IF_EXCEPTION(scope);

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (CacheableIdentifier::isCacheableIdentifierCell(brand)) {
        CacheableIdentifier identifier = CacheableIdentifier::createFromCell(brand.asCell());
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseObject->structure(), identifier))
            repatchSetPrivateBrand(globalObject, codeBlock, baseObject, oldStructure, identifier, *stubInfo);
    }
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationSetPrivateBrandGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedBrand, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue brand = JSValue::decode(encodedBrand);

    ASSERT(baseValue.isObject());
    ASSERT(brand.isSymbol());

    JSObject* baseObject = asObject(baseValue);
    baseObject->setPrivateBrand(globalObject, brand);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationCheckPrivateBrandOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedBrand, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue brand = JSValue::decode(encodedBrand);

    JSObject* baseObject = baseValue.toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);

    ASSERT(brand.isSymbol());

    baseObject->checkPrivateBrand(globalObject, brand);
    OPERATION_RETURN_IF_EXCEPTION(scope);

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (CacheableIdentifier::isCacheableIdentifierCell(brand)) {
        CacheableIdentifier identifier = CacheableIdentifier::createFromCell(brand.asCell());
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseObject->structure(), identifier))
            repatchCheckPrivateBrand(globalObject, codeBlock, baseObject, identifier, *stubInfo);
    }
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationCheckPrivateBrandGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedBrand, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue brand = JSValue::decode(encodedBrand);

    JSObject* baseObject = baseValue.toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope);

    ASSERT(brand.isSymbol());

    baseObject->checkPrivateBrand(globalObject, brand);
    OPERATION_RETURN(scope);
}

template<bool define>
static ALWAYS_INLINE void putPrivateNameOptimize(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue subscript, JSValue value, StructureStubInfo* stubInfo)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto baseObject = baseValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    auto propertyName = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    // Private fields can only be accessed within class lexical scope
    // and class methods are always in strict mode
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
    Structure* structure = CommonSlowPaths::originalStructureBeforePut(baseValue);
    constexpr bool isStrictMode = true;
    PutPropertySlot slot(baseObject, isStrictMode);
    if constexpr (define)
        baseObject->definePrivateField(globalObject, propertyName, value, slot);
    else
        baseObject->setPrivateField(globalObject, propertyName, value, slot);
    RETURN_IF_EXCEPTION(scope, void());

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;

    if (baseValue.isObject() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, structure, identifier))
            repatchPutBy(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, define ? PutByKind::DefinePrivateNameByVal : PutByKind::SetPrivateNameByVal);
    }
}

template<bool define>
static ALWAYS_INLINE void putPrivateName(JSGlobalObject* globalObject, JSValue baseValue, JSValue subscript, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto baseObject = baseValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    auto propertyName = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    scope.release();

    // Private fields can only be accessed within class lexical scope
    // and class methods are always in strict mode
    constexpr bool isStrictMode = true;
    PutPropertySlot slot(baseObject, isStrictMode);
    if constexpr (define)
        baseObject->definePrivateField(globalObject, propertyName, value, slot);
    else
        baseObject->setPrivateField(globalObject, propertyName, value, slot);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDefinePrivateFieldOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile*))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    putPrivateNameOptimize<true>(globalObject, codeBlock, baseValue, subscript, value, stubInfo);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSetPrivateFieldOptimize, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile*))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    putPrivateNameOptimize<false>(globalObject, codeBlock, baseValue, subscript, value, stubInfo);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDefinePrivateFieldGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile*))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putPrivateName<true>(globalObject, baseValue, subscript, value);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValDefinePrivateFieldGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putPrivateName<true>(globalObject, baseValue, subscript, value);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSetPrivateFieldGaveUp, void, (EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, StructureStubInfo* stubInfo, ArrayProfile*))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putPrivateName<false>(globalObject, baseValue, subscript, value);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutByValSetPrivateFieldGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putPrivateName<false>(globalObject, baseValue, subscript, value);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationCallDirectEvalSloppy, EncodedJSValue, (void* frame, JSScope* callerScopeChain, EncodedJSValue encodedThisValue))
{
    CallFrame* calleeFrame = reinterpret_cast<CallFrame*>(frame);
    // We can't trust our callee since it could be garbage but our caller's should be ok.
    VM& vm = calleeFrame->callerFrame()->deprecatedVM();
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);

    OPERATION_RETURN(scope, JSValue::encode(eval(calleeFrame, JSValue::decode(encodedThisValue), callerScopeChain, NoLexicallyScopedFeatures)));
}

JSC_DEFINE_JIT_OPERATION(operationCallDirectEvalStrict, EncodedJSValue, (void* frame, JSScope* callerScopeChain, EncodedJSValue encodedThisValue))
{
    CallFrame* calleeFrame = reinterpret_cast<CallFrame*>(frame);
    // We can't trust our callee since it could be garbage but our caller's should be ok.
    VM& vm = calleeFrame->callerFrame()->deprecatedVM();
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);

    OPERATION_RETURN(scope, JSValue::encode(eval(calleeFrame, JSValue::decode(encodedThisValue), callerScopeChain, StrictModeLexicallyScopedFeature)));
}

JSC_DEFINE_JIT_OPERATION(operationCallDirectEvalSloppyTaintedByWithScope, EncodedJSValue, (void* frame, JSScope* callerScopeChain, EncodedJSValue encodedThisValue))
{
    CallFrame* calleeFrame = reinterpret_cast<CallFrame*>(frame);
    // We can't trust our callee since it could be garbage but our caller's should be ok.
    VM& vm = calleeFrame->callerFrame()->deprecatedVM();
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);

    OPERATION_RETURN(scope, JSValue::encode(eval(calleeFrame, JSValue::decode(encodedThisValue), callerScopeChain, TaintedByWithScopeLexicallyScopedFeature)));
}

JSC_DEFINE_JIT_OPERATION(operationCallDirectEvalStrictTaintedByWithScope, EncodedJSValue, (void* frame, JSScope* callerScopeChain, EncodedJSValue encodedThisValue))
{
    CallFrame* calleeFrame = reinterpret_cast<CallFrame*>(frame);
    // We can't trust our callee since it could be garbage but our caller's should be ok.
    VM& vm = calleeFrame->callerFrame()->deprecatedVM();
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);

    OPERATION_RETURN(scope, JSValue::encode(eval(calleeFrame, JSValue::decode(encodedThisValue), callerScopeChain, StrictModeLexicallyScopedFeature | TaintedByWithScopeLexicallyScopedFeature)));
}

JSC_DEFINE_JIT_OPERATION(operationPolymorphicCall, UCPURegister, (CallFrame* calleeFrame, CallLinkInfo* callLinkInfo))
{
    JSCell* owner = callLinkInfo->ownerForSlowPath(calleeFrame);
    VM& vm = owner->vm();
    NativeCallFrameTracer tracer(vm, calleeFrame);
    sanitizeStackForVM(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);
    JSCell* calleeAsFunctionCell;
    void* callTarget = virtualForWithFunction(vm, owner, calleeFrame, callLinkInfo, calleeAsFunctionCell);
    if (UNLIKELY(scope.exception()))
        OPERATION_RETURN(scope, bitwise_cast<UCPURegister>(vm.getCTIStub(CommonJITThunkID::ThrowExceptionFromCall).template retagged<JSEntryPtrTag>().code().taggedPtr()));
    linkPolymorphicCall(vm, owner, calleeFrame, *callLinkInfo, CallVariant(calleeAsFunctionCell));
    // Keep owner alive explicitly. Now this function can be called from tail-call. This means that CallFrame for that owner already goes away, so we should keep it alive if we would like to use it.
    ensureStillAliveHere(owner);
    if (UNLIKELY(scope.exception()))
        OPERATION_RETURN(scope, bitwise_cast<UCPURegister>(vm.getCTIStub(CommonJITThunkID::ThrowExceptionFromCall).template retagged<JSEntryPtrTag>().code().taggedPtr()));
    OPERATION_RETURN(scope, static_cast<UCPURegister>(bitwise_cast<uintptr_t>(callTarget)));
}

JSC_DEFINE_JIT_OPERATION(operationVirtualCall, UCPURegister, (CallFrame* calleeFrame, CallLinkInfo* callLinkInfo))
{
    JSCell* owner = callLinkInfo->ownerForSlowPath(calleeFrame);
    VM& vm = owner->vm();
    NativeCallFrameTracer tracer(vm, calleeFrame);
    sanitizeStackForVM(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);
    JSCell* calleeAsFunctionCell;
    void* callTarget = virtualForWithFunction(vm, owner, calleeFrame, callLinkInfo, calleeAsFunctionCell);
    // Keep owner alive explicitly. Now this function can be called from tail-call. This means that CallFrame for that owner already goes away, so we should keep it alive if we would like to use it.
    ensureStillAliveHere(owner);
    if (UNLIKELY(scope.exception()))
        OPERATION_RETURN(scope, bitwise_cast<UCPURegister>(vm.getCTIStub(CommonJITThunkID::ThrowExceptionFromCall).template retagged<JSEntryPtrTag>().code().taggedPtr()));
    OPERATION_RETURN(scope, bitwise_cast<UCPURegister>(bitwise_cast<uintptr_t>(callTarget)));
}

JSC_DEFINE_JIT_OPERATION(operationDefaultCall, UCPURegister, (CallFrame* calleeFrame, CallLinkInfo* callLinkInfo))
{
    JSCell* owner = callLinkInfo->ownerForSlowPath(calleeFrame);
    VM& vm = owner->vm();
    NativeCallFrameTracer tracer(vm, calleeFrame);
    sanitizeStackForVM(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    calleeFrame->setCodeBlock(nullptr);
    void* callTarget = linkFor(vm, owner, calleeFrame, callLinkInfo);
    // Keep owner alive explicitly. Now this function can be called from tail-call. This means that CallFrame for that owner already goes away, so we should keep it alive if we would like to use it.
    ensureStillAliveHere(owner);
    if (UNLIKELY(scope.exception()))
        OPERATION_RETURN(scope, bitwise_cast<UCPURegister>(vm.getCTIStub(CommonJITThunkID::ThrowExceptionFromCall).template retagged<JSEntryPtrTag>().code().taggedPtr()));
    OPERATION_RETURN(scope, bitwise_cast<UCPURegister>(bitwise_cast<uintptr_t>(callTarget)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareLess, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    OPERATION_RETURN(scope, jsLess<true>(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareLessEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsLessEq<true>(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareGreater, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsLess<false>(globalObject, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareGreaterEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, jsLessEq<false>(globalObject, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1)));
}

JSC_DEFINE_JIT_OPERATION(operationCompareEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::equalSlowCaseInline(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

#if USE(JSVALUE64)
JSC_DEFINE_JIT_OPERATION(operationCompareStringEq, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* left, JSCell* right))
#else
JSC_DEFINE_JIT_OPERATION(operationCompareStringEq, size_t, (JSGlobalObject* globalObject, JSCell* left, JSCell* right))
#endif
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool result = asString(left)->equalInline(globalObject, asString(right));
#if USE(JSVALUE64)
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(result)));
#else
    OPERATION_RETURN(scope, result);
#endif
}

JSC_DEFINE_JIT_OPERATION(operationCompareStrictEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue src1 = JSValue::decode(encodedOp1);
    JSValue src2 = JSValue::decode(encodedOp2);

    OPERATION_RETURN(scope, JSValue::strictEqual(globalObject, src1, src2));
}

#if USE(BIGINT32)
JSC_DEFINE_JIT_OPERATION(operationCompareEqHeapBigIntToInt32, size_t, (JSGlobalObject* globalObject, JSCell* heapBigInt, int32_t smallInt))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(heapBigInt->isHeapBigInt());

    OPERATION_RETURN(scope, static_cast<JSBigInt*>(heapBigInt)->equalsToInt32(smallInt));
}
#endif

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithProfile, EncodedJSValue, (JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const JSValue* values, int size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(constructArrayNegativeIndexed(globalObject, profile, values, size)));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSizeAndProfile, EncodedJSValue, (JSGlobalObject* globalObject, ArrayAllocationProfile* profile, EncodedJSValue size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue sizeValue = JSValue::decode(size);
    OPERATION_RETURN(scope, JSValue::encode(constructArrayWithSizeQuirk(globalObject, profile, sizeValue)));
}

JSC_DEFINE_JIT_OPERATION(operationCreateLexicalEnvironmentTDZ, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, SymbolTable* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(JSLexicalEnvironment::create(vm, globalObject->activationStructure(), environment, table, jsTDZValue())));
}

JSC_DEFINE_JIT_OPERATION(operationCreateLexicalEnvironmentUndefined, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, SymbolTable* table))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(JSLexicalEnvironment::create(vm, globalObject->activationStructure(), environment, table, jsUndefined())));
}

JSC_DEFINE_JIT_OPERATION(operationCreateDirectArgumentsBaseline, EncodedJSValue, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(DirectArguments::createByCopying(globalObject, callFrame)));
}

JSC_DEFINE_JIT_OPERATION(operationCreateScopedArgumentsBaseline, EncodedJSValue, (JSGlobalObject* globalObject, JSLexicalEnvironment* environment))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ScopedArgumentsTable* table = environment->symbolTable()->arguments();
    OPERATION_RETURN(scope, JSValue::encode(ScopedArguments::createByCopying(globalObject, callFrame, table, environment)));
}

JSC_DEFINE_JIT_OPERATION(operationCreateClonedArgumentsBaseline, EncodedJSValue, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(ClonedArguments::createWithMachineFrame(globalObject, callFrame, ArgumentsMode::Cloned)));
}

template<typename FunctionType, bool isInvalidated>
static EncodedJSValue newFunctionCommon(VM& vm, JSGlobalObject* globalObject, JSScope* scope, JSCell* functionExecutable, Structure* structure)
{
    ASSERT(functionExecutable->inherits<FunctionExecutable>());
    if constexpr (isInvalidated)
        return JSValue::encode(FunctionType::createWithInvalidatedReallocationWatchpoint(vm, globalObject, static_cast<FunctionExecutable*>(functionExecutable), scope, structure));
    else
        return JSValue::encode(FunctionType::create(vm, globalObject, static_cast<FunctionExecutable*>(functionExecutable), scope, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, JSFunction::selectStructureForNewFuncExp(globalObject, jsCast<FunctionExecutable*>(functionExecutable))));
}

JSC_DEFINE_JIT_OPERATION(operationNewFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, JSFunction::selectStructureForNewFuncExp(globalObject, jsCast<FunctionExecutable*>(functionExecutable))));
}

JSC_DEFINE_JIT_OPERATION(operationNewSloppyFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    constexpr bool isBuiltin = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->sloppyFunctionStructure(isBuiltin)));
}

JSC_DEFINE_JIT_OPERATION(operationNewSloppyFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    constexpr bool isBuiltin = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->sloppyFunctionStructure(isBuiltin)));
}

JSC_DEFINE_JIT_OPERATION(operationNewStrictFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    constexpr bool isBuiltin = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->strictFunctionStructure(isBuiltin)));
}

JSC_DEFINE_JIT_OPERATION(operationNewStrictFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    constexpr bool isBuiltin = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->strictFunctionStructure(isBuiltin)));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrowFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    constexpr bool isBuiltin = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->arrowFunctionStructure(isBuiltin)));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrowFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    constexpr bool isBuiltin = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->arrowFunctionStructure(isBuiltin)));
}

JSC_DEFINE_JIT_OPERATION(operationNewGeneratorFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSGeneratorFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->generatorFunctionStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    OPERATION_RETURN(scope, newFunctionCommon<JSGeneratorFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->generatorFunctionStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSAsyncFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->asyncFunctionStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    OPERATION_RETURN(scope, newFunctionCommon<JSAsyncFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->asyncFunctionStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncGeneratorFunction, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = false;
    OPERATION_RETURN(scope, newFunctionCommon<JSAsyncGeneratorFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->asyncGeneratorFunctionStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (JSGlobalObject* globalObject, JSScope* environment, JSCell* functionExecutable))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    constexpr bool isInvalidated = true;
    OPERATION_RETURN(scope, newFunctionCommon<JSAsyncGeneratorFunction, isInvalidated>(vm, globalObject, environment, functionExecutable, globalObject->asyncGeneratorFunctionStructure()));
}

JSC_DEFINE_JIT_OPERATION(operationSetFunctionName, void, (JSGlobalObject* globalObject, JSCell* funcCell, EncodedJSValue encodedName))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* func = jsCast<JSFunction*>(funcCell);
    JSValue name = JSValue::decode(encodedName);
    func->setFunctionName(globalObject, name);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationNewObject, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, constructEmptyObject(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewPromise, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSPromise::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewInternalPromise, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSInternalPromise::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewGenerator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSGenerator::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncGenerator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSAsyncGenerator::create(vm, structure));
}

JSC_DEFINE_JIT_OPERATION(operationNewRegexp, JSCell*, (JSGlobalObject* globalObject, JSCell* regexpPtr))
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    static constexpr bool areLegacyFeaturesEnabled = true;
    OPERATION_RETURN(scope, RegExpObject::create(vm, globalObject->regExpStructure(), regexp, areLegacyFeaturesEnabled));
}

// The only reason for returning an UnusedPtr (instead of void) is so that we can reuse the
// existing DFG slow path generator machinery when creating the slow path for CheckTraps
// in the DFG. If a DFG slow path generator that supports a void return type is added in the
// future, we can switch to using that then.
JSC_DEFINE_JIT_OPERATION(operationHandleTraps, UnusedPtr, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(vm.traps().needHandling(VMTraps::AsyncEvents));
    vm.traps().handleTraps(VMTraps::AsyncEvents);
    OPERATION_RETURN(scope, nullptr);
}

JSC_DEFINE_JIT_OPERATION(operationDebug, void, (VM* vmPointer, int32_t debugHookType))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    vm.interpreter.debug(callFrame, static_cast<DebugHookType>(debugHookType));
    OPERATION_RETURN(scope);
}

#if ENABLE(DFG_JIT)
static void updateAllPredictionsAndOptimizeAfterWarmUp(CodeBlock* codeBlock)
{
    codeBlock->updateAllPredictions();
    codeBlock->optimizeAfterWarmUp();
}

JSC_DEFINE_JIT_OPERATION(operationOptimize, UGPRPair, (VM* vmPointer, uint32_t bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    BytecodeIndex bytecodeIndex = BytecodeIndex::fromBits(bytecodeIndexBits);

    // Defer GC for a while so that it doesn't run between when we enter into this
    // slow path and when we figure out the state of our code block. This prevents
    // a number of awkward reentrancy scenarios, including:
    //
    // - The optimized version of our code block being jettisoned by GC right after
    //   we concluded that we wanted to use it, but have not planted it into the JS
    //   stack yet.
    //
    // - An optimized version of our code block being installed just as we decided
    //   that it wasn't ready yet.
    //
    // Note that jettisoning won't happen if we already initiated OSR, because in
    // that case we would have already planted the optimized code block into the JS
    // stack.
    DeferGCForAWhile deferGC(vm);
    
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (UNLIKELY(codeBlock->jitType() != JITType::BaselineJIT)) {
        dataLog("Unexpected code block in Baseline->DFG tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    DFG::CapabilityLevel level = codeBlock->capabilityLevel();
    if (level == DFG::CannotCompile)
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    
    if (bytecodeIndex) {
        // If we're attempting to OSR from a loop, assume that this should be
        // separately optimized.
        codeBlock->m_shouldAlwaysBeInlined = false;
    }

    if (UNLIKELY(Options::verboseOSR())) {
        dataLog(
            *codeBlock, ": Entered optimize with bytecodeIndex = ", bytecodeIndex,
            ", executeCounter = ", codeBlock->baselineExecuteCounter(),
            ", optimizationDelayCounter = ", codeBlock->reoptimizationRetryCounter(),
            ", exitCounter = ");
        if (codeBlock->hasOptimizedReplacement())
            dataLog(codeBlock->replacement()->osrExitCounter());
        else
            dataLog("N/A");
        dataLog("\n");
    }

    if (!codeBlock->checkIfOptimizationThresholdReached()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("counter = ", codeBlock->baselineExecuteCounter()));
        codeBlock->updateAllPredictions();
        dataLogLnIf(Options::verboseOSR(), "Choosing not to optimize ", *codeBlock, " yet, because the threshold hasn't been reached.");
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    }
    
    if (UNLIKELY(vm.hasTerminationRequest())) {
        // If termination of the current stack of execution is in progress,
        // then we need to hold off on optimized compiles so that termination
        // checks will be called, and we can unwind out of the current stack.
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("Terminating current execution"));
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    }

    Debugger* debugger = codeBlock->globalObject()->debugger();
    if (UNLIKELY(debugger && (debugger->isStepping() || codeBlock->baselineAlternative()->hasDebuggerRequests()))) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("debugger is stepping or has requests"));
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    }

    if (codeBlock->m_shouldAlwaysBeInlined) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("should always be inlined"));
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        dataLogLnIf(Options::verboseOSR(), "Choosing not to optimize ", *codeBlock, " yet, because m_shouldAlwaysBeInlined == true.");
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    }

    // The call to JITWorklist::completeAllReadyPlansForVM() will complete all ready
    // (i.e. compiled) code blocks. But if it completes ours, we also need to know
    // what the result was so that we don't plow ahead and attempt OSR or immediate
    // reoptimization. This will have already also set the appropriate JIT execution
    // count threshold depending on what happened, so if the compilation was anything
    // but successful we just want to return early. See the case for worklistState ==
    // JITWorklist::Compiled, below.

    // Note that we could have alternatively just called Worklist::compilationState()
    // here, and if it returned Compiled, we could have then called
    // completeAndScheduleOSR() below. But that would have meant that it could take
    // longer for code blocks to be completed: they would only complete when *their*
    // execution count trigger fired; but that could take a while since the firing is
    // racy. It could also mean that code blocks that never run again after being
    // compiled would sit on the worklist until next GC. That's fine, but it's
    // probably a waste of memory. Our goal here is to complete code blocks as soon as
    // possible in order to minimize the chances of us executing baseline code after
    // optimized code is already available.
    JITWorklist::State worklistState = JITWorklist::ensureGlobalWorklist().completeAllReadyPlansForVM(
        vm, JITCompilationKey(codeBlock, Options::forceUnlinkedDFG() ? JITCompilationMode::UnlinkedDFG : JITCompilationMode::DFG));

    if (worklistState == JITWorklist::Compiling) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("compiling"));
        // We cannot be in the process of asynchronous compilation and also have an optimized
        // replacement.
        RELEASE_ASSERT(!codeBlock->hasOptimizedReplacement());
        codeBlock->setOptimizationThresholdBasedOnCompilationResult(CompilationDeferred);
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    }

    if (worklistState == JITWorklist::Compiled) {
        // If we don't have an optimized replacement but we did just get compiled, then
        // the compilation failed or was invalidated, in which case the execution count
        // thresholds have already been set appropriately by
        // CodeBlock::setOptimizationThresholdBasedOnCompilationResult() and we have
        // nothing left to do.
        if (!codeBlock->hasOptimizedReplacement()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("compiled and failed"));
            codeBlock->updateAllPredictions();
            dataLogLnIf(Options::verboseOSR(), "Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.");
            OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
        }
    } else if (codeBlock->hasOptimizedReplacement()) {
        CodeBlock* replacement = codeBlock->replacement();
        dataLogLnIf(Options::verboseOSR(), "Considering OSR ", codeBlock, " -> ", replacement, ".");
        // If we have an optimized replacement, then it must be the case that we entered
        // cti_optimize from a loop. That's because if there's an optimized replacement,
        // then all calls to this function will be relinked to the replacement and so
        // the prologue OSR will never fire.
        
        // This is an interesting threshold check. Consider that a function OSR exits
        // in the middle of a loop, while having a relatively low exit count. The exit
        // will reset the execution counter to some target threshold, meaning that this
        // code won't be reached until that loop heats up for >=1000 executions. But then
        // we do a second check here, to see if we should either reoptimize, or just
        // attempt OSR entry. Hence it might even be correct for
        // shouldReoptimizeFromLoopNow() to always return true. But we make it do some
        // additional checking anyway, to reduce the amount of recompilation thrashing.
        if (replacement->shouldReoptimizeFromLoopNow()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("should reoptimize from loop now"));
            dataLogLnIf(Options::verboseOSR(),
                "Triggering reoptimization of ", codeBlock,
                "(", replacement, ") (in loop).");
            replacement->jettison(Profiler::JettisonDueToBaselineLoopReoptimizationTrigger, CountReoptimization);
            OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
        }
    } else {
        if (!codeBlock->shouldOptimizeNowFromBaseline()) {
            dataLogLnIf(Options::verboseOSR(),
                "Delaying optimization for ", *codeBlock,
                " because of insufficient profiling.");
            OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
        }

        dataLogLnIf(Options::verboseOSR(), "Triggering optimized compilation of ", *codeBlock);

        unsigned numVarsWithValues = 0;
        if (bytecodeIndex)
            numVarsWithValues = codeBlock->numCalleeLocals();

        Operands<std::optional<JSValue>> mustHandleValues(codeBlock->numParameters(), numVarsWithValues, 0);
        int localsUsedForCalleeSaves = static_cast<int>(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters());
        for (size_t i = 0; i < mustHandleValues.size(); ++i) {
            Operand operand = mustHandleValues.operandForIndex(i);

            if (operand.isLocal() && operand.toLocal() < localsUsedForCalleeSaves)
                continue;
            mustHandleValues[i] = callFrame->uncheckedR(operand.virtualRegister()).jsValue();
        }

        CodeBlock* replacementCodeBlock = codeBlock->newReplacement();
        CompilationResult result = DFG::compile(
            vm, replacementCodeBlock, nullptr, Options::forceUnlinkedDFG() ? JITCompilationMode::UnlinkedDFG : JITCompilationMode::DFG, bytecodeIndex,
            WTFMove(mustHandleValues), JITToDFGDeferredCompilationCallback::create());
        
        if (result != CompilationSuccessful) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("compilation failed"));
            OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
        }
    }
    
    CodeBlock* optimizedCodeBlock = codeBlock->replacement();
    ASSERT(optimizedCodeBlock && JSC::JITCode::isOptimizingJIT(optimizedCodeBlock->jitType()));
    
    if (void* dataBuffer = DFG::prepareOSREntry(vm, callFrame, optimizedCodeBlock, bytecodeIndex)) {
        CODEBLOCK_LOG_EVENT(optimizedCodeBlock, "osrEntry", ("at bc#", bytecodeIndex));
        dataLogLnIf(Options::verboseOSR(), "Performing OSR ", codeBlock, " -> ", optimizedCodeBlock);

        codeBlock->optimizeSoon();
        codeBlock->unlinkedCodeBlock()->setDidOptimize(TriState::True);
        void* targetPC = untagCodePtr<JITThunkPtrTag>(vm.getCTIStub(DFG::osrEntryThunkGenerator).code().taggedPtr());
        targetPC = tagCodePtrWithStackPointerForJITCall(targetPC, callFrame);
        OPERATION_RETURN(scope, encodeResult(targetPC, dataBuffer));
    }

    dataLogLnIf(Options::verboseOSR(),
        "Optimizing ", codeBlock, " -> ", codeBlock->replacement(),
        " succeeded, OSR failed, after a delay of ",
        codeBlock->optimizationDelayCounter());

    // Count the OSR failure as a speculation failure. If this happens a lot, then
    // reoptimize.
    optimizedCodeBlock->countOSRExit();

    // We are a lot more conservative about triggering reoptimization after OSR failure than
    // before it. If we enter the optimize_from_loop trigger with a bucket full of fail
    // already, then we really would like to reoptimize immediately. But this case covers
    // something else: there weren't many (or any) speculation failures before, but we just
    // failed to enter the speculative code because some variable had the wrong value or
    // because the OSR code decided for any spurious reason that it did not want to OSR
    // right now. So, we only trigger reoptimization only upon the more conservative (non-loop)
    // reoptimization trigger.
    if (optimizedCodeBlock->shouldReoptimizeNow()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("should reoptimize now"));
        dataLogLnIf(Options::verboseOSR(),
            "Triggering reoptimization of ", codeBlock, " -> ",
            codeBlock->replacement(), " (after OSR fail).");
        optimizedCodeBlock->jettison(Profiler::JettisonDueToBaselineLoopReoptimizationTriggerOnOSREntryFail, CountReoptimization);
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
    }

    // OSR failed this time, but it might succeed next time! Let the code run a bit
    // longer and then try again.
    codeBlock->optimizeAfterWarmUp();
    
    CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("OSR failed"));
    OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
}

JSC_DEFINE_JIT_OPERATION(operationTryOSREnterAtCatchAndValueProfile, UGPRPair, (VM* vmPointer, uint32_t bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    BytecodeIndex bytecodeIndex = BytecodeIndex::fromBits(bytecodeIndexBits);

    CodeBlock* codeBlock = callFrame->codeBlock();
    CodeBlock* optimizedReplacement = codeBlock->replacement();
    if (UNLIKELY(!optimizedReplacement))
        OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));

    switch (optimizedReplacement->jitType()) {
    case JITType::DFGJIT:
    case JITType::FTLJIT: {
        CodePtr<ExceptionHandlerPtrTag> entry = DFG::prepareCatchOSREntry(vm, callFrame, codeBlock, optimizedReplacement, bytecodeIndex);
        OPERATION_RETURN(scope, encodeResult(entry.taggedPtr<char*>(), optimizedReplacement));
    }
    default:
        break;
    }

    codeBlock->ensureCatchLivenessIsComputedForBytecodeIndex(bytecodeIndex);
    auto bytecode = codeBlock->instructions().at(bytecodeIndex)->as<OpCatch>();
    auto& metadata = bytecode.metadata(codeBlock);
    metadata.m_buffer->forEach([&] (ValueProfileAndVirtualRegister& profile) {
        profile.m_buckets[0] = JSValue::encode(callFrame->uncheckedR(profile.m_operand).jsValue());
    });

    OPERATION_RETURN(scope, encodeResult(nullptr, nullptr));
}

#endif

enum class AccessorType {
    Getter,
    Setter
};

static void putAccessorByVal(JSGlobalObject* globalObject, JSObject* base, JSValue subscript, int32_t attribute, JSObject* accessor, AccessorType accessorType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto propertyKey = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    scope.release();
    if (accessorType == AccessorType::Getter)
        base->putGetter(globalObject, propertyKey, accessor, attribute);
    else
        base->putSetter(globalObject, propertyKey, accessor, attribute);
}

JSC_DEFINE_JIT_OPERATION(operationPutGetterById, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t options, JSCell* getter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    ASSERT(getter->isObject());
    baseObj->putGetter(globalObject, uid, getter, options);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutSetterById, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t options, JSCell* setter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    ASSERT(setter->isObject());
    baseObj->putSetter(globalObject, uid, setter, options);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutGetterByVal, void, (JSGlobalObject* globalObject, JSCell* base, EncodedJSValue encodedSubscript, int32_t attribute, JSCell* getter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    putAccessorByVal(globalObject, asObject(base), JSValue::decode(encodedSubscript), attribute, asObject(getter), AccessorType::Getter);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationPutSetterByVal, void, (JSGlobalObject* globalObject, JSCell* base, EncodedJSValue encodedSubscript, int32_t attribute, JSCell* setter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    putAccessorByVal(globalObject, asObject(base), JSValue::decode(encodedSubscript), attribute, asObject(setter), AccessorType::Setter);
    OPERATION_RETURN(scope);
}

#if USE(JSVALUE64)
JSC_DEFINE_JIT_OPERATION(operationPutGetterSetter, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t attribute, EncodedJSValue encodedGetterValue, EncodedJSValue encodedSetterValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(object && object->isObject());
    JSObject* baseObject = asObject(object);

    JSValue getter = JSValue::decode(encodedGetterValue);
    JSValue setter = JSValue::decode(encodedSetterValue);
    ASSERT(getter.isObject() || setter.isObject());
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);
    CommonSlowPaths::putDirectAccessorWithReify(vm, globalObject, baseObject, uid, accessor, attribute);
    OPERATION_RETURN(scope);
}

#else
JSC_DEFINE_JIT_OPERATION(operationPutGetterSetter, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t attribute, JSCell* getterCell, JSCell* setterCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(object && object->isObject());
    JSObject* baseObject = asObject(object);

    ASSERT(getterCell || setterCell);
    JSObject* getter = getterCell ? getterCell->getObject() : nullptr;
    JSObject* setter = setterCell ? setterCell->getObject() : nullptr;
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);
    CommonSlowPaths::putDirectAccessorWithReify(vm, globalObject, baseObject, uid, accessor, attribute);
    OPERATION_RETURN(scope);
}
#endif

JSC_DEFINE_JIT_OPERATION(operationInstanceOfCustom, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, JSObject* constructor, EncodedJSValue encodedHasInstance))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    JSValue hasInstanceValue = JSValue::decode(encodedHasInstance);

    if (constructor->hasInstance(globalObject, value, hasInstanceValue))
        OPERATION_RETURN(scope, 1);
    OPERATION_RETURN(scope, 0);
}

#if CPU(ARM64) || CPU(X86_64)

JSC_DEFINE_JIT_OPERATION(operationIteratorNextTryFast, UGPRPair, (JSGlobalObject* globalObject, JSArrayIterator* arrayIterator, JSArray* array, void* metadataPointer))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& metadata = *bitwise_cast<OpIteratorNext::Metadata*>(metadataPointer);
    metadata.m_iterableProfile.observeStructureID(array->structureID());
    metadata.m_iterationMetadata.seenModes = metadata.m_iterationMetadata.seenModes | IterationMode::FastArray;

    auto& indexSlot = arrayIterator->internalField(JSArrayIterator::Field::Index);
    int64_t index = indexSlot.get().asAnyInt();
    ASSERT(0 <= index && index <= maxSafeInteger());

    JSValue value;
    bool done = index == JSArrayIterator::doneIndex || index >= array->length();
    if (!done) {
        // No need for a barrier here because we know this is a primitive.
        indexSlot.setWithoutWriteBarrier(jsNumber(index + 1));
        ASSERT(index == static_cast<unsigned>(index));
        value = array->getIndex(globalObject, static_cast<unsigned>(index));
        OPERATION_RETURN_IF_EXCEPTION(scope, makeUGPRPair(0, 0));
    } else {
        // No need for a barrier here because we know this is a primitive.
        indexSlot.setWithoutWriteBarrier(jsNumber(-1));
    }

    OPERATION_RETURN(scope, makeUGPRPair(JSValue::encode(jsBoolean(done)), JSValue::encode(value)));
}

#endif

ALWAYS_INLINE static JSValue getByVal(JSGlobalObject* globalObject, CallFrame* callFrame, ArrayProfile* arrayProfile, JSValue baseValue, JSValue subscript)
{
    UNUSED_PARAM(callFrame);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure();
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            auto existingAtomString = asString(subscript)->toExistingAtomString(globalObject);
            RETURN_IF_EXCEPTION(scope, JSValue());
            if (!existingAtomString.isNull()) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomString.impl())) {
                    ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
                    return result;
                }
            }
        }
    }

    if (std::optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        uint32_t i = *index;
        if (isJSString(baseValue)) {
            if (asString(baseValue)->canGetIndex(i))
                RELEASE_AND_RETURN(scope, asString(baseValue)->getIndex(globalObject, i));
            if (arrayProfile)
                arrayProfile->setOutOfBounds();
        } else if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (JSValue result = object->tryGetIndexQuickly(i, arrayProfile))
                return result;

            bool skipMarkingOutOfBounds = false;

            if (object->indexingType() == ArrayWithContiguous
                && static_cast<uint32_t>(i) < object->butterfly()->publicLength()) {
                // FIXME: expand this to ArrayStorage, Int32, and maybe Double:
                // https://bugs.webkit.org/show_bug.cgi?id=182940
                auto* globalObject = object->globalObject();
                skipMarkingOutOfBounds = globalObject->isOriginalArrayStructure(object->structure()) && globalObject->arrayPrototypeChainIsSane();
            }

            if (!skipMarkingOutOfBounds && !CommonSlowPaths::canAccessArgumentIndexQuickly(*object, i)) {
                // FIXME: This will make us think that in-bounds typed array accesses are actually
                // out-of-bounds.
                // https://bugs.webkit.org/show_bug.cgi?id=149886
                if (arrayProfile)
                    arrayProfile->setOutOfBounds();
            }
        }

        RELEASE_AND_RETURN(scope, baseValue.get(globalObject, i));
    } else if (subscript.isNumber() && baseValue.isCell() && arrayProfile) {
        arrayProfile->setOutOfBounds();
        if (subscript == jsNumber(-1)) {
            if (auto* array = jsDynamicCast<JSArray*>(baseValue.asCell()); LIKELY(array && array->definitelyNegativeOneMiss()))
                return jsUndefined();
        }
    }

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());
    auto property = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
    RELEASE_AND_RETURN(scope, baseValue.get(globalObject, property));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    OPERATION_RETURN(scope, JSValue::encode(getByVal(globalObject, callFrame, profile, baseValue, subscript)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    CodeBlock* codeBlock = callFrame->codeBlock();

    if (baseValue.isCell() && subscript.isInt32()) {
        Structure* structure = baseValue.asCell()->structure();
        if (stubInfo->considerRepatchingCacheGeneric(vm, codeBlock, structure)) {
            if (profile)
                profile->computeUpdatedPrediction(codeBlock, structure);
            repatchArrayGetByVal(globalObject, codeBlock, baseValue, subscript, *stubInfo, GetByKind::ByVal);
        }
    }

    if (baseValue.isCell() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            scope.release();
            OPERATION_RETURN(scope, JSValue::encode(baseValue.getPropertySlot(globalObject, propertyName, [&] (bool found, PropertySlot& slot) -> JSValue {
                LOG_IC((vm, ICEvent::OperationGetByValOptimize, baseValue.classInfoOrNull(), propertyName, baseValue == slot.slotBase()));
                
                CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
                if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
                    repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::ByVal);
                return found ? slot.getValue(globalObject, propertyName) : jsUndefined();
            })));
        }
    }

    OPERATION_RETURN(scope, JSValue::encode(getByVal(globalObject, callFrame, profile, baseValue, subscript)));
}

ALWAYS_INLINE static JSValue getByValWithThis(JSGlobalObject* globalObject, CallFrame* callFrame, ArrayProfile* arrayProfile, JSValue baseValue, JSValue subscript, JSValue thisValue)
{
    UNUSED_PARAM(callFrame);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure();
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            auto existingAtomString = asString(subscript)->toExistingAtomString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!existingAtomString.isNull()) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomString.impl())) {
                    ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
                    return result;
                }
            }
        }
    }

    PropertySlot slot(thisValue, PropertySlot::PropertySlot::InternalMethodType::Get);
    if (std::optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        uint32_t i = *index;
        if (isJSString(baseValue)) {
            if (asString(baseValue)->canGetIndex(i))
                RELEASE_AND_RETURN(scope, asString(baseValue)->getIndex(globalObject, i));
            if (arrayProfile)
                arrayProfile->setOutOfBounds();
        } else if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (JSValue result = object->tryGetIndexQuickly(i, arrayProfile))
                return result;

            bool skipMarkingOutOfBounds = false;

            if (object->indexingType() == ArrayWithContiguous
                && static_cast<uint32_t>(i) < object->butterfly()->publicLength()) {
                // FIXME: expand this to ArrayStorage, Int32, and maybe Double:
                // https://bugs.webkit.org/show_bug.cgi?id=182940
                auto* globalObject = object->globalObject();
                skipMarkingOutOfBounds = globalObject->isOriginalArrayStructure(object->structure()) && globalObject->arrayPrototypeChainIsSane();
            }

            if (!skipMarkingOutOfBounds && !CommonSlowPaths::canAccessArgumentIndexQuickly(*object, i)) {
                // FIXME: This will make us think that in-bounds typed array accesses are actually
                // out-of-bounds.
                // https://bugs.webkit.org/show_bug.cgi?id=149886
                if (arrayProfile)
                    arrayProfile->setOutOfBounds();
            }
        }

        RELEASE_AND_RETURN(scope, baseValue.get(globalObject, i, slot));
    } else if (subscript.isNumber() && baseValue.isCell() && arrayProfile) {
        arrayProfile->setOutOfBounds();
        if (subscript == jsNumber(-1)) {
            if (auto* array = jsDynamicCast<JSArray*>(baseValue.asCell()); LIKELY(array && array->definitelyNegativeOneMiss()))
                return jsUndefined();
        }
    }

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto property = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, baseValue.get(globalObject, property, slot));
}

static ALWAYS_INLINE JSValue getByValMegamorphic(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, ArrayProfile* profile, JSValue baseValue, JSValue thisValue, JSValue subscript, GetByKind kind)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!baseValue.isObject() || !(subscript.isString() && CacheableIdentifier::isCacheableIdentifierCell(subscript)))) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        if (kind == GetByKind::ByVal)
            RELEASE_AND_RETURN(scope, getByVal(globalObject, callFrame, profile, baseValue, subscript));
        RELEASE_AND_RETURN(scope, getByValWithThis(globalObject, callFrame, profile, baseValue, subscript, thisValue));
    }

    Identifier propertyName = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    UniquedStringImpl* uid = propertyName.impl();
    if (UNLIKELY(!canUseMegamorphicGetById(vm, uid))) {
        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
            repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
        if (kind == GetByKind::ByVal)
            RELEASE_AND_RETURN(scope, getByVal(globalObject, callFrame, profile, baseValue, subscript));
        RELEASE_AND_RETURN(scope, getByValWithThis(globalObject, callFrame, profile, baseValue, subscript, thisValue));
    }

    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);
    JSObject* baseObject = asObject(baseValue);
    JSObject* object = baseObject;
    bool cacheable = true;
    while (true) {
        if (UNLIKELY(TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()) && object->type() != ArrayType && object->type() != JSFunctionType && object != globalObject->arrayPrototype())) {
            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
            bool result = object->getNonIndexPropertySlot(globalObject, uid, slot);
            RETURN_IF_EXCEPTION(scope, { });
            if (result)
                RELEASE_AND_RETURN(scope, slot.getValue(globalObject, uid));
            return jsUndefined();
        }

        Structure* structure = object->structure();
        bool hasProperty = object->getOwnNonIndexPropertySlot(vm, structure, uid, slot);
        structure = object->structure(); // Reload it again since static-class-table can cause transition. But this transition only affects on this Structure.
        cacheable &= structure->propertyAccessesAreCacheable();
        if (hasProperty) {
            if (LIKELY(cacheable && slot.isCacheableValue() && slot.cachedOffset() <= MegamorphicCache::maxOffset)) {
                if (slot.slotBase() == baseObject || !baseObject->structure()->isDictionary())
                    vm.megamorphicCache()->initAsHit(baseObject->structureID(), uid, slot.slotBase(), slot.cachedOffset(), slot.slotBase() == baseObject);
                else {
                    if (UNLIKELY(baseObject->structure()->hasBeenFlattenedBefore())) {
                        if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                            repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
                    }
                }
            } else {
                if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                    repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
            }
            RELEASE_AND_RETURN(scope, slot.getValue(globalObject, uid));
        }

        cacheable &= structure->propertyAccessesAreCacheableForAbsence();
        cacheable &= structure->hasMonoProto();

        JSValue prototype = object->getPrototypeDirect();
        if (!prototype.isObject()) {
            if (LIKELY(cacheable)) {
                if (LIKELY(!baseObject->structure()->isDictionary())) {
                    vm.megamorphicCache()->initAsMiss(baseObject->structureID(), uid);
                    return jsUndefined();
                }
                if (LIKELY(!baseObject->structure()->hasBeenFlattenedBefore()))
                    return jsUndefined();
            }

            if (stubInfo && stubInfo->considerRepatchingCacheMegamorphic(vm))
                repatchGetBySlowPathCall(callFrame->codeBlock(), *stubInfo, kind);
            return jsUndefined();
        }
        object = asObject(prototype);
    }
}

JSC_DEFINE_JIT_OPERATION(operationGetByValMegamorphic, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    OPERATION_RETURN(scope, JSValue::encode(getByValMegamorphic(globalObject, vm, callFrame, stubInfo, profile, baseValue, baseValue, JSValue::decode(encodedSubscript), GetByKind::ByVal)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValMegamorphicGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue baseValue = JSValue::decode(encodedBase);
    OPERATION_RETURN(scope, JSValue::encode(getByValMegamorphic(globalObject, vm, callFrame, nullptr, nullptr, baseValue, baseValue, JSValue::decode(encodedSubscript), GetByKind::ByVal)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);

    if (LIKELY(baseValue.isCell())) {
        JSCell* base = baseValue.asCell();

        if (std::optional<uint32_t> index = property.tryGetAsUint32Index())
            OPERATION_RETURN(scope, getByValWithIndex(globalObject, base, *index));

        if (property.isString()) {
            Structure& structure = *base->structure();
            if (JSCell::canUseFastGetOwnProperty(structure)) {
                auto existingAtomString = asString(property)->toExistingAtomString(globalObject);
                OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
                if (!existingAtomString.isNull()) {
                    if (JSValue result = base->fastGetOwnProperty(vm, structure, existingAtomString.impl()))
                        OPERATION_RETURN(scope, JSValue::encode(result));
                }
            }
        }
    }

    baseValue.requireObjectCoercible(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto propertyName = property.toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(baseValue.get(globalObject, propertyName)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValWithThisGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue thisValue = JSValue::decode(encodedThis);

    OPERATION_RETURN(scope, JSValue::encode(getByValWithThis(globalObject, callFrame, profile, baseValue, subscript, thisValue)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValWithThisOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue thisValue = JSValue::decode(encodedThis);

    CodeBlock* codeBlock = callFrame->codeBlock();

    if (baseValue.isCell() && subscript.isInt32()) {
        Structure* structure = baseValue.asCell()->structure();
        if (stubInfo->considerRepatchingCacheGeneric(vm, codeBlock, structure)) {
            if (profile)
                profile->computeUpdatedPrediction(codeBlock, structure);
            repatchArrayGetByVal(globalObject, codeBlock, baseValue, subscript, *stubInfo, GetByKind::ByValWithThis);
        }
    }

    PropertySlot slot(thisValue, PropertySlot::PropertySlot::InternalMethodType::Get);
    if (baseValue.isCell() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            scope.release();
            OPERATION_RETURN(scope, JSValue::encode(baseValue.getPropertySlot(globalObject, propertyName, slot, [&] (bool found, PropertySlot& slot) -> JSValue {
                LOG_IC((vm, ICEvent::OperationGetByValWithThisOptimize, baseValue.classInfoOrNull(), propertyName, baseValue == slot.slotBase()));

                CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
                if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
                    repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::ByValWithThis);
                return found ? slot.getValue(globalObject, propertyName) : jsUndefined();
            })));
        }
    }

    OPERATION_RETURN(scope, JSValue::encode(getByValWithThis(globalObject, callFrame, profile, baseValue, subscript, thisValue)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValWithThisGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedThis))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);
    JSValue thisValue = JSValue::decode(encodedThis);

    if (LIKELY(baseValue.isCell())) {
        JSCell* base = baseValue.asCell();

        if (std::optional<uint32_t> index = property.tryGetAsUint32Index())
            OPERATION_RETURN(scope, getByValWithIndexAndThis(globalObject, base, *index, thisValue));

        if (property.isString()) {
            Structure& structure = *base->structure();
            if (JSCell::canUseFastGetOwnProperty(structure)) {
                auto existingAtomString = asString(property)->toExistingAtomString(globalObject);
                OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
                if (!existingAtomString.isNull()) {
                    if (JSValue result = base->fastGetOwnProperty(vm, structure, existingAtomString.impl()))
                        OPERATION_RETURN(scope, JSValue::encode(result));
                }
            }
        }
    }

    baseValue.requireObjectCoercible(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto propertyName = property.toPropertyKey(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    PropertySlot slot(thisValue, PropertySlot::PropertySlot::InternalMethodType::Get);
    OPERATION_RETURN(scope, JSValue::encode(baseValue.get(globalObject, propertyName, slot)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValWithThisMegamorphic, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis, StructureStubInfo* stubInfo, ArrayProfile* profile))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, JSValue::encode(getByValMegamorphic(globalObject, vm, callFrame, stubInfo, profile, JSValue::decode(encodedBase), JSValue::decode(encodedThis), JSValue::decode(encodedSubscript), GetByKind::ByValWithThis)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValWithThisMegamorphicGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(getByValMegamorphic(globalObject, vm, callFrame, nullptr, nullptr, JSValue::decode(encodedBase), JSValue::decode(encodedThis), JSValue::decode(encodedSubscript), GetByKind::ByValWithThis)));
}

ALWAYS_INLINE static JSValue getPrivateName(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, JSValue fieldNameValue)
{
    UNUSED_PARAM(callFrame);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* base = baseValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());
    auto fieldName = fieldNameValue.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    PropertySlot slot(base, PropertySlot::InternalMethodType::GetOwnProperty);
    base->getPrivateField(globalObject, fieldName, slot);
    RETURN_IF_EXCEPTION(scope, JSValue());

    return slot.getValue(globalObject, fieldName);
}

ALWAYS_INLINE static JSValue getPrivateName(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, PropertyName fieldName)
{
    ASSERT(fieldName.isPrivateName());
    UNUSED_PARAM(callFrame);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    JSObject* base = baseValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    PropertySlot slot(base, PropertySlot::InternalMethodType::GetOwnProperty);
    base->getPrivateField(globalObject, fieldName, slot);
    RETURN_IF_EXCEPTION(scope, JSValue());

    return slot.getValue(globalObject, fieldName);
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedFieldName, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue fieldNameValue = JSValue::decode(encodedFieldName);
    ASSERT(CacheableIdentifier::isCacheableIdentifierCell(fieldNameValue));

    CodeBlock* codeBlock = callFrame->codeBlock();

    if (baseValue.isObject()) {
        const Identifier fieldName = fieldNameValue.toPropertyKey(globalObject);
        EXCEPTION_ASSERT(!scope.exception() || vm.hasPendingTerminationException());
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        ASSERT(fieldName.isSymbol());

        JSObject* base = jsCast<JSObject*>(baseValue.asCell());

        PropertySlot slot(base, PropertySlot::InternalMethodType::GetOwnProperty);
        base->getPrivateField(globalObject, fieldName, slot);
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

        LOG_IC((vm, ICEvent::OperationGetPrivateNameOptimize, baseValue.classInfoOrNull(), fieldName, true));

        CacheableIdentifier identifier = CacheableIdentifier::createFromCell(fieldNameValue.asCell());
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::PrivateName);
        OPERATION_RETURN(scope, JSValue::encode(slot.getValue(globalObject, fieldName)));
    }

    OPERATION_RETURN(scope, JSValue::encode(getPrivateName(globalObject, callFrame, baseValue, fieldNameValue)));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedFieldName, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue fieldNameValue = JSValue::decode(encodedFieldName);

    OPERATION_RETURN(scope, JSValue::encode(getPrivateName(globalObject, callFrame, baseValue, fieldNameValue)));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedFieldName))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue fieldNameValue = JSValue::decode(encodedFieldName);

    OPERATION_RETURN(scope, JSValue::encode(getPrivateName(globalObject, callFrame, baseValue, fieldNameValue)));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameByIdGaveUp, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(base);
    CacheableIdentifier identifier = stubInfo->identifier();

    JSValue result = getPrivateName(globalObject, callFrame, baseValue, identifier);

    LOG_IC((vm, ICEvent::OperationGetPrivateNameById, baseValue.classInfoOrNull(), identifier, true));

    OPERATION_RETURN(scope, JSValue::encode(result));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameByIdOptimize, EncodedJSValue, (EncodedJSValue base, StructureStubInfo* stubInfo))
{
    SuperSamplerScope superSamplerScope(false);

    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = stubInfo->identifier();
    JSValue baseValue = JSValue::decode(base);

    if (baseValue.isObject()) {
        JSObject* base = jsCast<JSObject*>(baseValue.asCell());

        PropertySlot slot(base, PropertySlot::InternalMethodType::GetOwnProperty);
        base->getPrivateField(globalObject, identifier, slot);
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

        LOG_IC((vm, ICEvent::OperationGetPrivateNameByIdOptimize, baseValue.classInfoOrNull(), identifier, true));

        CodeBlock* codeBlock = callFrame->codeBlock();
        if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::PrivateNameById);
        OPERATION_RETURN(scope, JSValue::encode(slot.getValue(globalObject, identifier)));
    }

    OPERATION_RETURN(scope, JSValue::encode(getPrivateName(globalObject, callFrame, baseValue, identifier)));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameByIdGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(base);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);

    JSValue result = getPrivateName(globalObject, callFrame, baseValue, identifier);

    LOG_IC((vm, ICEvent::OperationGetPrivateNameByIdGeneric, baseValue.classInfoOrNull(), identifier, true));

    OPERATION_RETURN(scope, JSValue::encode(result));
}

static bool deleteById(JSGlobalObject* globalObject, VM& vm, DeletePropertySlot& slot, JSValue base, PropertyName propertyName, ECMAMode ecmaMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* baseObj = base.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (!baseObj)
        return false;
    bool couldDelete = baseObj->methodTable()->deleteProperty(baseObj, globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, false);
    if (!couldDelete && ecmaMode.isStrict())
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
    return couldDelete;
}

static ALWAYS_INLINE size_t deleteByIdOptimize(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, StructureStubInfo* stubInfo, JSValue baseValue, CacheableIdentifier identifier, ECMAMode ecmaMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* oldStructure = baseValue.structureOrNull();

    DeletePropertySlot slot;
    bool result = deleteById(globalObject, vm, slot, baseValue, identifier, ecmaMode);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (baseValue.isObject()) {
        if (!parseIndex(identifier)) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
                repatchDeleteBy(globalObject, codeBlock, slot, baseValue, oldStructure, identifier, *stubInfo, ecmaMode.isStrict() ? DelByKind::ByIdStrict : DelByKind::ByIdSloppy, ecmaMode);
        }
    }

    return result;
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdSloppyOptimize, size_t, (EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = stubInfo->identifier();

    OPERATION_RETURN(scope, deleteByIdOptimize(globalObject, vm, callFrame, stubInfo, baseValue, identifier, ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdStrictOptimize, size_t, (EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = stubInfo->identifier();

    OPERATION_RETURN(scope, deleteByIdOptimize(globalObject, vm, callFrame, stubInfo, baseValue, identifier, ECMAMode::strict()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdSloppyGaveUp, size_t, (EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();
    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteById(globalObject, vm, slot, JSValue::decode(encodedBase), identifier, ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdStrictGaveUp, size_t, (EncodedJSValue encodedBase, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = stubInfo->identifier();
    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteById(globalObject, vm, slot, JSValue::decode(encodedBase), identifier, ECMAMode::strict()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdSloppyGeneric, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteById(globalObject, vm, slot, JSValue::decode(encodedBase), identifier, ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdStrictGeneric, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteById(globalObject, vm, slot, JSValue::decode(encodedBase), identifier, ECMAMode::strict()));
}

static bool deleteByVal(JSGlobalObject* globalObject, VM& vm, DeletePropertySlot& slot, JSValue base, JSValue key, ECMAMode ecmaMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* baseObj = base.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (!baseObj)
        return false;

    bool couldDelete;
    uint32_t index;
    if (key.getUInt32(index))
        couldDelete = baseObj->methodTable()->deletePropertyByIndex(baseObj, globalObject, index);
    else {
        Identifier property = key.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        couldDelete = baseObj->methodTable()->deleteProperty(baseObj, globalObject, property, slot);
    }
    RETURN_IF_EXCEPTION(scope, false);
    if (!couldDelete && ecmaMode.isStrict())
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
    return couldDelete;
}

static ALWAYS_INLINE size_t deleteByValOptimize(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame, JSValue baseValue, JSValue subscript, StructureStubInfo* stubInfo, ECMAMode ecmaMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    DeletePropertySlot slot;
    Structure* oldStructure = baseValue.structureOrNull();

    bool result = deleteByVal(globalObject, vm, slot, baseValue, subscript, ecmaMode);
    RETURN_IF_EXCEPTION(scope, { });

    if (baseValue.isObject() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
            if (stubInfo->considerRepatchingCacheBy(vm, codeBlock, baseValue.structureOrNull(), identifier))
                repatchDeleteBy(globalObject, codeBlock, slot, baseValue, oldStructure, identifier, *stubInfo, ecmaMode.isStrict() ? DelByKind::ByValStrict : DelByKind::ByValSloppy, ecmaMode);
        }
    }

    return result;
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValSloppyOptimize, size_t, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    OPERATION_RETURN(scope, deleteByValOptimize(globalObject, vm, callFrame, baseValue, subscript, stubInfo, ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValStrictOptimize, size_t, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    OPERATION_RETURN(scope, deleteByValOptimize(globalObject, vm, callFrame, baseValue, subscript, stubInfo, ECMAMode::strict()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValSloppyGaveUp, size_t, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteByVal(globalObject, vm, slot, JSValue::decode(encodedBase), JSValue::decode(encodedSubscript), ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValStrictGaveUp, size_t, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteByVal(globalObject, vm, slot, JSValue::decode(encodedBase), JSValue::decode(encodedSubscript), ECMAMode::strict()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValSloppyGeneric, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteByVal(globalObject, vm, slot, JSValue::decode(encodedBase), JSValue::decode(encodedSubscript), ECMAMode::sloppy()));
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValStrictGeneric, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    DeletePropertySlot slot;
    OPERATION_RETURN(scope, deleteByVal(globalObject, vm, slot, JSValue::decode(encodedBase), JSValue::decode(encodedSubscript), ECMAMode::strict()));
}

JSC_DEFINE_JIT_OPERATION(operationPushWithScope, JSCell*, (JSGlobalObject* globalObject, JSCell* currentScopeCell, EncodedJSValue objectValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(objectValue).toObject(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

    JSScope* currentScope = jsCast<JSScope*>(currentScopeCell);

    OPERATION_RETURN(scope, JSWithScope::create(vm, globalObject, currentScope, object));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationPushWithScopeObject, JSCell*, (JSGlobalObject* globalObject, JSCell* currentScopeCell, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSScope* currentScope = jsCast<JSScope*>(currentScopeCell);
    OPERATION_RETURN(scope, JSWithScope::create(vm, globalObject, currentScope, object));
}

JSC_DEFINE_JIT_OPERATION(operationInstanceOfGaveUp, EncodedJSValue, (EncodedJSValue encodedValue, EncodedJSValue encodedProto, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    JSValue value = JSValue::decode(encodedValue);
    JSValue proto = JSValue::decode(encodedProto);

    bool result = JSObject::defaultHasInstance(globalObject, value, proto);
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(result)));
}

JSC_DEFINE_JIT_OPERATION(operationInstanceOfGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, EncodedJSValue encodedProto))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    JSValue proto = JSValue::decode(encodedProto);

    bool result = JSObject::defaultHasInstance(globalObject, value, proto);
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(result)));
}

JSC_DEFINE_JIT_OPERATION(operationInstanceOfOptimize, EncodedJSValue, (EncodedJSValue encodedValue, EncodedJSValue encodedProto, StructureStubInfo* stubInfo))
{
    JSGlobalObject* globalObject = stubInfo->globalObject();
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    ICSlowPathCallFrameTracer tracer(vm, callFrame, stubInfo);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = JSValue::decode(encodedValue);
    JSValue proto = JSValue::decode(encodedProto);
    
    bool result = JSObject::defaultHasInstance(globalObject, value, proto);
    OPERATION_RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));
    
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerRepatchingCacheGeneric(vm, codeBlock, value.structureOrNull()))
        repatchInstanceOf(globalObject, codeBlock, value, proto, *stubInfo, result);
    
    OPERATION_RETURN(scope, JSValue::encode(jsBoolean(result)));
}

JSC_DEFINE_JIT_OPERATION(operationSizeFrameForForwardArguments, size_t, (JSGlobalObject* globalObject, EncodedJSValue, int32_t numUsedStackSlots, int32_t))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, sizeFrameForForwardArguments(globalObject, callFrame, vm, numUsedStackSlots));
}

JSC_DEFINE_JIT_OPERATION(operationSizeFrameForVarargs, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedArguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue arguments = JSValue::decode(encodedArguments);
    OPERATION_RETURN(scope, sizeFrameForVarargs(globalObject, callFrame, vm, arguments, numUsedStackSlots, firstVarArgOffset));
}

JSC_DEFINE_JIT_OPERATION(operationSetupForwardArgumentsFrame, CallFrame*, (JSGlobalObject* globalObject, CallFrame* newCallFrame, EncodedJSValue, int32_t, int32_t length))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    setupForwardArgumentsFrame(globalObject, callFrame, newCallFrame, length);
    OPERATION_RETURN(scope, newCallFrame);
}

JSC_DEFINE_JIT_OPERATION(operationSetupVarargsFrame, CallFrame*, (JSGlobalObject* globalObject, CallFrame* newCallFrame, EncodedJSValue encodedArguments, int32_t firstVarArgOffset, int32_t length))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue arguments = JSValue::decode(encodedArguments);
    setupVarargsFrame(globalObject, callFrame, newCallFrame, arguments, firstVarArgOffset, length);
    OPERATION_RETURN(scope, newCallFrame);
}

JSC_DEFINE_JIT_OPERATION(operationSwitchCharWithUnknownKeyType, char*, (JSGlobalObject* globalObject, EncodedJSValue encodedKey, size_t tableIndex, int32_t min))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = callFrame->codeBlock();

    const SimpleJumpTable& linkedTable = codeBlock->baselineSwitchJumpTable(tableIndex);
    ASSERT(codeBlock->unlinkedSwitchJumpTable(tableIndex).m_min == min);
    void* result = linkedTable.m_ctiDefault.taggedPtr();

    if (key.isString()) {
        JSString* string = asString(key);
        if (string->length() == 1) {
            auto value = string->value(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);
            result = linkedTable.ctiForValue(min, value[0]).taggedPtr();
        }
    }

    assertIsTaggedWith<JSSwitchPtrTag>(result);
    OPERATION_RETURN(scope, reinterpret_cast<char*>(result));
}

JSC_DEFINE_JIT_OPERATION(operationSwitchImmWithUnknownKeyType, char*, (VM* vmPointer, EncodedJSValue encodedKey, size_t tableIndex, int32_t min))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = callFrame->codeBlock();

    const SimpleJumpTable& linkedTable = codeBlock->baselineSwitchJumpTable(tableIndex);
    ASSERT(codeBlock->unlinkedSwitchJumpTable(tableIndex).m_min == min);
    void* result;
    if (key.isInt32())
        result = linkedTable.ctiForValue(min, key.asInt32()).taggedPtr();
    else if (key.isDouble() && key.asDouble() == static_cast<int32_t>(key.asDouble()))
        result = linkedTable.ctiForValue(min, static_cast<int32_t>(key.asDouble())).taggedPtr();
    else
        result = linkedTable.m_ctiDefault.taggedPtr();
    assertIsTaggedWith<JSSwitchPtrTag>(result);
    OPERATION_RETURN(scope, reinterpret_cast<char*>(result));
}

JSC_DEFINE_JIT_OPERATION(operationSwitchStringWithUnknownKeyType, char*, (JSGlobalObject* globalObject, EncodedJSValue encodedKey, size_t tableIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result;
    const StringJumpTable& linkedTable = codeBlock->baselineStringSwitchJumpTable(tableIndex);

    if (key.isString()) {
        const UnlinkedStringJumpTable& unlinkedTable = codeBlock->unlinkedStringSwitchJumpTable(tableIndex);
        auto* string = asString(key);

        unsigned length = string->length();
        if (length < unlinkedTable.minLength() || length > unlinkedTable.maxLength())
            result = linkedTable.ctiDefault().taggedPtr();
        else {
            auto value = string->value(globalObject);
            OPERATION_RETURN_IF_EXCEPTION(scope, nullptr);

            result = linkedTable.ctiForValue(unlinkedTable, value.data.impl()).taggedPtr();
        }
    } else
        result = linkedTable.ctiDefault().taggedPtr();

    assertIsTaggedWith<JSSwitchPtrTag>(result);
    OPERATION_RETURN(scope, reinterpret_cast<char*>(result));
}

JSC_DEFINE_JIT_OPERATION(operationResolveScopeForBaseline, EncodedJSValue, (JSGlobalObject* globalObject, const JSInstruction* pc))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();

    auto bytecode = pc->as<OpResolveScope>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSScope* environment = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    JSObject* resolvedScope = JSScope::resolve(globalObject, environment, ident);
    // Proxy can throw an error here, e.g. Proxy in with statement's @unscopables.
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto& metadata = bytecode.metadata(codeBlock);
    ResolveType resolveType = metadata.m_resolveType;

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(resolveType != ModuleVar);

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks:
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        if (resolvedScope->isGlobalObject()) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(resolvedScope);
            bool hasProperty = globalObject->hasProperty(globalObject, ident);
            OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (hasProperty) {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalPropertyWithVarInjectionChecks : GlobalProperty;
                metadata.m_globalObject.set(vm, codeBlock, globalObject);
                metadata.m_globalLexicalBindingEpoch = globalObject->globalLexicalBindingEpoch();
            }
        } else if (resolvedScope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(resolvedScope);
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar;
            metadata.m_globalLexicalEnvironment.set(vm, codeBlock, globalLexicalEnvironment);
        }
        break;
    }
    default:
        break;
    }

    OPERATION_RETURN(scope, JSValue::encode(resolvedScope));
}

JSC_DEFINE_JIT_OPERATION(operationGetFromScope, EncodedJSValue, (JSGlobalObject* globalObject, const JSInstruction* pc))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();

    auto bytecode = pc->as<OpGetFromScope>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* environment = jsCast<JSObject*>(callFrame->uncheckedR(bytecode.m_scope).jsValue());
    GetPutInfo& getPutInfo = bytecode.metadata(codeBlock).m_getPutInfo;

    // ModuleVar is always converted to ClosureVar for get_from_scope.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    OPERATION_RETURN(scope, JSValue::encode(environment->getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            if (getPutInfo.resolveMode() == ThrowIfNotFound)
                throwException(globalObject, scope, createUndefinedVariableError(globalObject, ident));
            return jsUndefined();
        }

        JSValue result = JSValue();
        if (environment->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            result = slot.getValue(globalObject, ident);
            if (result == jsTDZValue()) {
                throwException(globalObject, scope, createTDZError(globalObject));
                return jsUndefined();
            }
        }

        CommonSlowPaths::tryCacheGetFromScopeGlobal(globalObject, codeBlock, vm, bytecode, environment, slot, ident);

        if (!result)
            return slot.getValue(globalObject, ident);
        return result;
    })));
}

JSC_DEFINE_JIT_OPERATION(operationPutToScope, void, (JSGlobalObject* globalObject, const JSInstruction* pc))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();
    auto bytecode = pc->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(codeBlock);

    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* jsScope = jsCast<JSObject*>(callFrame->uncheckedR(bytecode.m_scope).jsValue());
    JSValue value = callFrame->r(bytecode.m_value).jsValue();
    GetPutInfo& getPutInfo = metadata.m_getPutInfo;

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    if (getPutInfo.resolveType() == ResolvedClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(jsScope);
        environment->variableAt(ScopeOffset(metadata.m_operand)).set(vm, environment, value);
        if (WatchpointSet* set = metadata.m_watchpointSet)
            set->touch(vm, "Executed op_put_scope<ResolvedClosureVar>");
        OPERATION_RETURN(scope);
    }

    bool hasProperty = jsScope->hasProperty(globalObject, ident);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    if (hasProperty
        && jsScope->isGlobalLexicalEnvironment()
        && !isInitialization(getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(jsScope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(jsScope, globalObject, ident, slot);
        if (slot.getValue(globalObject, ident) == jsTDZValue()) {
            throwException(globalObject, scope, createTDZError(globalObject));
            OPERATION_RETURN(scope);
        }
    }

    if (getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty) {
        throwException(globalObject, scope, createUndefinedVariableError(globalObject, ident));
        OPERATION_RETURN(scope);
    }

    PutPropertySlot slot(jsScope, getPutInfo.ecmaMode().isStrict(), PutPropertySlot::UnknownContext, isInitialization(getPutInfo.initializationMode()));
    jsScope->methodTable()->put(jsScope, globalObject, ident, value, slot);
    
    OPERATION_RETURN_IF_EXCEPTION(scope);

    CommonSlowPaths::tryCachePutToScopeGlobal(globalObject, codeBlock, bytecode, jsScope, slot, ident);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationThrow, void, (JSGlobalObject* globalObject, EncodedJSValue encodedExceptionValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue exceptionValue = JSValue::decode(encodedExceptionValue);
    throwException(globalObject, scope, exceptionValue);

    // Results stored out-of-band in vm.targetMachinePCForThrow & vm.callFrameForCatch
    genericUnwind(vm, callFrame);
    OPERATION_RETURN(scope);
}

JSC_DEFINE_JIT_OPERATION(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity, char*, (VM* vmPointer, JSObject* object))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!object->structure()->outOfLineCapacity());
    Butterfly* result = object->allocateMoreOutOfLineStorage(vm, 0, initialOutOfLineCapacity);
    object->nukeStructureAndSetButterfly(vm, object->structureID(), result);
    OPERATION_RETURN(scope, reinterpret_cast<char*>(result));
}

JSC_DEFINE_JIT_OPERATION(operationReallocateButterflyToGrowPropertyStorage, char*, (VM* vmPointer, JSObject* object, size_t newSize))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Butterfly* result = object->allocateMoreOutOfLineStorage(vm, object->structure()->outOfLineCapacity(), newSize);
    object->nukeStructureAndSetButterfly(vm, object->structureID(), result);
    OPERATION_RETURN(scope, reinterpret_cast<char*>(result));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationReallocateButterflyAndTransition, void, (VM* vmPointer, JSObject* baseObject, const InlineCacheHandler* handler, EncodedJSValue encodedValue))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    size_t newSize = handler->newSize() / sizeof(JSValue);
    size_t oldSize = handler->oldSize() / sizeof(JSValue);
    PropertyOffset offset = handler->offset();
    Structure* oldStructure = WTF::opaque(handler->structureID().decode());
    Structure* newStructure = WTF::opaque(handler->newStructureID().decode());

    ASSERT(oldStructure == baseObject->structure());
    Butterfly* newButterfly = baseObject->allocateMoreOutOfLineStorage(vm, oldSize, newSize);
    baseObject->nukeStructureAndSetButterfly(vm, StructureID::encode(oldStructure), newButterfly);
    baseObject->putDirectOffset(vm, offset, JSValue::decode(encodedValue));
    baseObject->setStructure(vm, newStructure);

    ensureStillAliveHere(oldStructure);
    ensureStillAliveHere(newStructure);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationOSRWriteBarrier, void, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    vm.writeBarrier(cell);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationWriteBarrierSlowPath, void, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    vm.writeBarrierSlowPath(cell);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationLookupExceptionHandler, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
    ASSERT(vm.targetMachinePCForThrow);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationLookupExceptionHandlerFromCallerFrame, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ASSERT(callFrame->isPartiallyInitializedFrame());
    ASSERT(jsCast<ErrorInstance*>(vm.exceptionForInspection()->value().asCell())->isStackOverflowError());
    genericUnwind(vm, callFrame);
    ASSERT(vm.targetMachinePCForThrow);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationVMHandleException, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
}

// This function "should" just take the JSGlobalObject*, but doing so would make it more difficult
// to call from exception check sites. So, unlike all of our other functions, we allow
// ourselves to play some gnarly ABI tricks just to simplify the calling convention. This is
// particularly safe here since this is never called on the critical path - it's only for
// testing.
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationExceptionFuzz, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);
    void* returnPC = __builtin_return_address(0);
    // FIXME (see rdar://72897291): Work around a Clang bug where __builtin_return_address()
    // sometimes gives us a signed pointer, and sometimes does not.
    returnPC = removeCodePtrTag(returnPC);
    doExceptionFuzzing(globalObject, scope, "JITOperations", returnPC);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationExceptionFuzzWithCallFrame, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);
    void* returnPC = __builtin_return_address(0);
    // FIXME (see rdar://72897291): Work around a Clang bug where __builtin_return_address()
    // sometimes gives us a signed pointer, and sometimes does not.
    returnPC = removeCodePtrTag(returnPC);
    doExceptionFuzzing(callFrame->lexicalGlobalObject(vm), scope, "JITOperations", returnPC);
}

JSC_DEFINE_JIT_OPERATION(operationValueAdd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(jsAdd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2))));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile* arithProfile))
{
    ASSERT(arithProfile);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, JSValue::encode(profiledAdd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2), *arithProfile)));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC* addIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    BinaryArithProfile* arithProfile = addIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeLHSAndRHS(op1, op2);
    auto nonOptimizeVariant = operationValueAddProfiledNoOptimize;
    addIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif
    
    JSValue result = jsAdd(globalObject, op1, op2);
    arithProfile->observeResult(result);

    OPERATION_RETURN(scope, JSValue::encode(result));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddProfiledNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC* addIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    BinaryArithProfile* arithProfile = addIC->arithProfile();
    ASSERT(arithProfile);
    OPERATION_RETURN(scope, JSValue::encode(profiledAdd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2), *arithProfile)));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC* addIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    auto nonOptimizeVariant = operationValueAddNoOptimize;
    if (BinaryArithProfile* arithProfile = addIC->arithProfile())
        arithProfile->observeLHSAndRHS(op1, op2);
    addIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    OPERATION_RETURN(scope, JSValue::encode(jsAdd(globalObject, op1, op2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    JSValue result = jsAdd(globalObject, op1, op2);

    OPERATION_RETURN(scope, JSValue::encode(result));
}

ALWAYS_INLINE static EncodedJSValue unprofiledMul(JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    return JSValue::encode(jsMul(globalObject, op1, op2));
}

ALWAYS_INLINE static EncodedJSValue profiledMul(JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile& arithProfile, bool shouldObserveLHSAndRHSTypes = true)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    if (shouldObserveLHSAndRHSTypes)
        arithProfile.observeLHSAndRHS(op1, op2);

    JSValue result = jsMul(globalObject, op1, op2);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    arithProfile.observeResult(result);
    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationValueMul, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, unprofiledMul(globalObject, encodedOp1, encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationValueMulNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, unprofiledMul(globalObject, encodedOp1, encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationValueMulOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC* mulIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto nonOptimizeVariant = operationValueMulNoOptimize;
    if (BinaryArithProfile* arithProfile = mulIC->arithProfile())
        arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    mulIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    OPERATION_RETURN(scope, unprofiledMul(globalObject, encodedOp1, encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationValueMulProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile* arithProfile))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(arithProfile);
    OPERATION_RETURN(scope, profiledMul(globalObject, encodedOp1, encodedOp2, *arithProfile));
}

JSC_DEFINE_JIT_OPERATION(operationValueMulProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC* mulIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    BinaryArithProfile* arithProfile = mulIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    auto nonOptimizeVariant = operationValueMulProfiledNoOptimize;
    mulIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    OPERATION_RETURN(scope, profiledMul(globalObject, encodedOp1, encodedOp2, *arithProfile, false));
}

JSC_DEFINE_JIT_OPERATION(operationValueMulProfiledNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC* mulIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    BinaryArithProfile* arithProfile = mulIC->arithProfile();
    ASSERT(arithProfile);
    OPERATION_RETURN(scope, profiledMul(globalObject, encodedOp1, encodedOp2, *arithProfile));
}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegate, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue operand = JSValue::decode(encodedOperand);

    JSValue primValue = operand.toPrimitive(globalObject, PreferNumber);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    if (primValue.isBigInt32())
        OPERATION_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32())));
#endif
    if (primValue.isHeapBigInt())
        OPERATION_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt())));

    double number = primValue.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(-number)));

}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegateProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand, UnaryArithProfile* arithProfile))
{
    ASSERT(arithProfile);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue operand = JSValue::decode(encodedOperand);
    arithProfile->observeArg(operand);

    JSValue primValue = operand.toPrimitive(globalObject, PreferNumber);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    if (primValue.isBigInt32()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32());
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        arithProfile->observeResult(result);
        OPERATION_RETURN(scope, JSValue::encode(result));
    }
#endif
    if (primValue.isHeapBigInt()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt());
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        arithProfile->observeResult(result);
        OPERATION_RETURN(scope, JSValue::encode(result));
    }

    double number = primValue.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSValue result = jsNumber(-number);
    arithProfile->observeResult(result);
    OPERATION_RETURN(scope, JSValue::encode(result));
}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegateProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand, JITNegIC* negIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue operand = JSValue::decode(encodedOperand);

    UnaryArithProfile* arithProfile = negIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeArg(operand);
    negIC->generateOutOfLine(callFrame->codeBlock(), operationArithNegateProfiled);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif
    
    JSValue primValue = operand.toPrimitive(globalObject, PreferNumber);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    if (primValue.isBigInt32()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32());
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        arithProfile->observeResult(result);
        OPERATION_RETURN(scope, JSValue::encode(result));
    }
#endif
    if (primValue.isHeapBigInt()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt());
        OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
        arithProfile->observeResult(result);
        OPERATION_RETURN(scope, JSValue::encode(result));
    }

    double number = primValue.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSValue result = jsNumber(-number);
    arithProfile->observeResult(result);
    OPERATION_RETURN(scope, JSValue::encode(result));
}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegateOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand, JITNegIC* negIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue operand = JSValue::decode(encodedOperand);

    if (UnaryArithProfile* arithProfile = negIC->arithProfile())
        arithProfile->observeArg(operand);
    negIC->generateOutOfLine(callFrame->codeBlock(), operationArithNegate);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    JSValue primValue = operand.toPrimitive(globalObject, PreferNumber);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    // FIXME: why does this function profile the argument but not the result?
    if (primValue.isBigInt32())
        OPERATION_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32())));
#endif
    if (primValue.isHeapBigInt())
        OPERATION_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt())));

    double number = primValue.toNumber(globalObject);
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
    OPERATION_RETURN(scope, JSValue::encode(jsNumber(-number)));
}

ALWAYS_INLINE static EncodedJSValue unprofiledSub(JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    return JSValue::encode(jsSub(globalObject, op1, op2));
}

ALWAYS_INLINE static EncodedJSValue profiledSub(VM& vm, JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile& arithProfile, bool shouldObserveLHSAndRHSTypes = true)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    if (shouldObserveLHSAndRHSTypes)
        arithProfile.observeLHSAndRHS(op1, op2);

    JSValue result = jsSub(globalObject, op1, op2);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    arithProfile.observeResult(result);
    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationValueSub, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    OPERATION_RETURN(scope, unprofiledSub(globalObject, encodedOp1, encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationValueSubProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile* arithProfile))
{
    ASSERT(arithProfile);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, profiledSub(vm, globalObject, encodedOp1, encodedOp2, *arithProfile));
}

JSC_DEFINE_JIT_OPERATION(operationValueSubOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC* subIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto nonOptimizeVariant = operationValueSubNoOptimize;
    if (BinaryArithProfile* arithProfile = subIC->arithProfile())
        arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    subIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    OPERATION_RETURN(scope, unprofiledSub(globalObject, encodedOp1, encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationValueSubNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    OPERATION_RETURN(scope, unprofiledSub(globalObject, encodedOp1, encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationValueSubProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC* subIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    BinaryArithProfile* arithProfile = subIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    auto nonOptimizeVariant = operationValueSubProfiledNoOptimize;
    subIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    OPERATION_RETURN(scope, profiledSub(vm, globalObject, encodedOp1, encodedOp2, *arithProfile, false));
}

JSC_DEFINE_JIT_OPERATION(operationValueSubProfiledNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC* subIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    BinaryArithProfile* arithProfile = subIC->arithProfile();
    ASSERT(arithProfile);
    OPERATION_RETURN(scope, profiledSub(vm, globalObject, encodedOp1, encodedOp2, *arithProfile));
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationDebuggerWillCallNativeExecutable, void, (CallFrame* callFrame))
{
    ASSERT(!callFrame->isNativeCalleeFrame());

    auto* globalObject = callFrame->jsCallee()->globalObject();
    if (!globalObject)
        return;

    auto* debugger = globalObject->debugger();
    if (!debugger)
        return;

    debugger->willCallNativeExecutable(callFrame);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationProcessTypeProfilerLog, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    vm.typeProfilerLog()->processLogEntries(vm, "Log Full, called from inside baseline JIT"_s);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationProcessShadowChickenLog, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    RELEASE_ASSERT(vm.shadowChicken());
    vm.shadowChicken()->update(vm, callFrame);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationRetrieveAndClearExceptionIfCatchable, JSCell*, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!!scope.exception());

    Exception* exception = scope.exception();
    if (UNLIKELY(vm.isTerminationException(exception))) {
        genericUnwind(vm, callFrame);
        return nullptr;
    }

    // We want to clear the exception here rather than in the catch prologue
    // JIT code because clearing it also entails clearing a bit in an Atomic
    // bit field in VMTraps.
    scope.clearException();
    return exception;
}

} // namespace JSC

IGNORE_WARNINGS_END

#endif // ENABLE(JIT)
