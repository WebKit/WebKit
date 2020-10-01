/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "CommonSlowPathsInlines.h"
#include "DFGCompilationMode.h"
#include "DFGDriver.h"
#include "DFGOSREntry.h"
#include "DFGThunks.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "GetterSetter.h"
#include "ICStats.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JITToDFGDeferredCompilationCallback.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGenerator.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSCInlines.h"
#include "JSCPtrTag.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSLexicalEnvironment.h"
#include "JSWithScope.h"
#include "LLIntEntrypoint.h"
#include "ObjectConstructor.h"
#include "PropertyName.h"
#include "RegExpObject.h"
#include "Repatch.h"
#include "ShadowChicken.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "TypeProfilerLog.h"
#include "VMInlines.h"

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC {

ALWAYS_INLINE JSValue profiledAdd(JSGlobalObject* globalObject, JSValue op1, JSValue op2, BinaryArithProfile& arithProfile)
{
    arithProfile.observeLHSAndRHS(op1, op2);
    JSValue result = jsAdd(globalObject, op1, op2);
    arithProfile.observeResult(result);
    return result;
}

extern "C" {

#if COMPILER(MSVC)
void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

#define OUR_RETURN_ADDRESS _ReturnAddress()
#else
#define OUR_RETURN_ADDRESS __builtin_return_address(0)
#endif

#if ENABLE(OPCODE_SAMPLING)
#define CTI_SAMPLER vm.interpreter->sampler()
#else
#define CTI_SAMPLER 0
#endif


JSC_DEFINE_JIT_OPERATION(operationThrowStackOverflowError, void, (CodeBlock* codeBlock))
{
    // We pass in our own code block, because the callframe hasn't been populated.
    VM& vm = codeBlock->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    callFrame->convertToStackOverflowFrame(vm, codeBlock);
    throwStackOverflowError(codeBlock->globalObject(), scope);
}

JSC_DEFINE_JIT_OPERATION(operationThrowStackOverflowErrorFromThunk, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwStackOverflowError(globalObject, scope);
    genericUnwind(vm, callFrame);
    ASSERT(vm.targetMachinePCForThrow);
}

JSC_DEFINE_JIT_OPERATION(operationThrowIteratorResultIsNotObject, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
}

JSC_DEFINE_JIT_OPERATION(operationCallArityCheck, int32_t, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(vm, callFrame, CodeForCall);
    if (UNLIKELY(missingArgCount < 0)) {
        CodeBlock* codeBlock = CommonSlowPaths::codeBlockFromCallFrameCallee(callFrame, CodeForCall);
        callFrame->convertToStackOverflowFrame(vm, codeBlock);
        throwStackOverflowError(globalObject, scope);
    }

    return missingArgCount;
}

JSC_DEFINE_JIT_OPERATION(operationConstructArityCheck, int32_t, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(vm, callFrame, CodeForConstruct);
    if (UNLIKELY(missingArgCount < 0)) {
        CodeBlock* codeBlock = CommonSlowPaths::codeBlockFromCallFrameCallee(callFrame, CodeForConstruct);
        callFrame->convertToStackOverflowFrame(vm, codeBlock);
        throwStackOverflowError(globalObject, scope);
    }

    return missingArgCount;
}

JSC_DEFINE_JIT_OPERATION(operationTryGetById, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);
    baseValue.getPropertySlot(globalObject, ident, slot);

    return JSValue::encode(slot.getPureResult());
}


JSC_DEFINE_JIT_OPERATION(operationTryGetByIdGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);
    baseValue.getPropertySlot(globalObject, ident, slot);

    return JSValue::encode(slot.getPureResult());
}

JSC_DEFINE_JIT_OPERATION(operationTryGetByIdOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);

    baseValue.getPropertySlot(globalObject, ident, slot);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier) && !slot.isTaintedByOpaqueObject() && (slot.isCacheableValue() || slot.isCacheableGetter() || slot.isUnset()))
        repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::Try);

    return JSValue::encode(slot.getPureResult());
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdDirect, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, ident, slot);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(found ? slot.getValue(globalObject, ident) : jsUndefined()));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdDirectGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, ident, slot);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(found ? slot.getValue(globalObject, ident) : jsUndefined()));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdDirectOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, ident, slot);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
        repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::Direct);

    RELEASE_AND_RETURN(scope, JSValue::encode(found ? slot.getValue(globalObject, ident) : jsUndefined()));
}

JSC_DEFINE_JIT_OPERATION(operationGetById, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    JSValue result = baseValue.get(globalObject, ident, slot);

    LOG_IC((ICEvent::OperationGetById, baseValue.classInfoOrNull(vm), ident, baseValue == slot.slotBase()));

    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    JSValue result = baseValue.get(globalObject, ident, slot);
    
    LOG_IC((ICEvent::OperationGetByIdGeneric, baseValue.classInfoOrNull(vm), ident, baseValue == slot.slotBase()));
    
    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);

    return JSValue::encode(baseValue.getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        
        LOG_IC((ICEvent::OperationGetByIdOptimize, baseValue.classInfoOrNull(vm), ident, baseValue == slot.slotBase()));
        
        CodeBlock* codeBlock = callFrame->codeBlock();
        if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::Normal);
        return found ? slot.getValue(globalObject, ident) : jsUndefined();
    }));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThis, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, EncodedJSValue thisEncoded, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    stubInfo->tookSlowPath = true;

    JSValue baseValue = JSValue::decode(base);
    JSValue thisValue = JSValue::decode(thisEncoded);
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);

    return JSValue::encode(baseValue.get(globalObject, ident, slot));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue base, EncodedJSValue thisEncoded, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    JSValue thisValue = JSValue::decode(thisEncoded);
    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);

    return JSValue::encode(baseValue.get(globalObject, ident, slot));
}

JSC_DEFINE_JIT_OPERATION(operationGetByIdWithThisOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, EncodedJSValue thisEncoded, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    JSValue thisValue = JSValue::decode(thisEncoded);

    PropertySlot slot(thisValue, PropertySlot::InternalMethodType::Get);
    return JSValue::encode(baseValue.getPropertySlot(globalObject, ident, slot, [&] (bool found, PropertySlot& slot) -> JSValue {
        LOG_IC((ICEvent::OperationGetByIdWithThisOptimize, baseValue.classInfoOrNull(vm), ident, baseValue == slot.slotBase()));
        
        CodeBlock* codeBlock = callFrame->codeBlock();
        if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::WithThis);
        return found ? slot.getValue(globalObject, ident) : jsUndefined();
    }));
}

JSC_DEFINE_JIT_OPERATION(operationInById, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    stubInfo->tookSlowPath = true;

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        return JSValue::encode(jsUndefined());
    }
    JSObject* baseObject = asObject(baseValue);

    LOG_IC((ICEvent::OperationInById, baseObject->classInfo(vm), ident));

    scope.release();
    PropertySlot slot(baseObject, PropertySlot::InternalMethodType::HasProperty);
    return JSValue::encode(jsBoolean(baseObject->getPropertySlot(globalObject, ident, slot)));
}

JSC_DEFINE_JIT_OPERATION(operationInByIdOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue base, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    JSValue baseValue = JSValue::decode(base);
    if (!baseValue.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseValue));
        return JSValue::encode(jsUndefined());
    }
    JSObject* baseObject = asObject(baseValue);

    LOG_IC((ICEvent::OperationInByIdOptimize, baseObject->classInfo(vm), ident));

    scope.release();
    PropertySlot slot(baseObject, PropertySlot::InternalMethodType::HasProperty);
    bool found = baseObject->getPropertySlot(globalObject, ident, slot);
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerCachingBy(vm, codeBlock, baseObject->structure(vm), identifier))
        repatchInByID(globalObject, codeBlock, baseObject, identifier, found, slot, *stubInfo);
    return JSValue::encode(jsBoolean(found));
}

JSC_DEFINE_JIT_OPERATION(operationInByVal, EncodedJSValue, (JSGlobalObject* globalObject, JSCell* base, EncodedJSValue key))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::encode(jsBoolean(CommonSlowPaths::opInByVal(globalObject, base, JSValue::decode(key))));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdStrict, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    PutPropertySlot slot(baseValue, true, callFrame->codeBlock()->putByIdContext());
    baseValue.putInline(globalObject, ident, JSValue::decode(encodedValue), slot);
    
    LOG_IC((ICEvent::OperationPutByIdStrict, baseValue.classInfoOrNull(vm), ident, slot.base() == baseValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdNonStrict, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    PutPropertySlot slot(baseValue, false, callFrame->codeBlock()->putByIdContext());
    baseValue.putInline(globalObject, ident, JSValue::decode(encodedValue), slot);

    LOG_IC((ICEvent::OperationPutByIdNonStrict, baseValue.classInfoOrNull(vm), ident, slot.base() == baseValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectStrict, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    PutPropertySlot slot(baseValue, true, callFrame->codeBlock()->putByIdContext());
    CommonSlowPaths::putDirectWithReify(vm, globalObject, asObject(baseValue), ident, JSValue::decode(encodedValue), slot);

    LOG_IC((ICEvent::OperationPutByIdDirectStrict, baseValue.classInfoOrNull(vm), ident, slot.base() == baseValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectNonStrict, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(encodedBase);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    PutPropertySlot slot(baseValue, false, callFrame->codeBlock()->putByIdContext());
    CommonSlowPaths::putDirectWithReify(vm, globalObject, asObject(baseValue), ident, JSValue::decode(encodedValue), slot);

    LOG_IC((ICEvent::OperationPutByIdDirectNonStrict, baseValue.classInfoOrNull(vm), ident, slot.base() == baseValue));
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdStrictOptimize, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseValue, true, codeBlock->putByIdContext());

    Structure* structure = CommonSlowPaths::originalStructureBeforePut(vm, baseValue);
    baseValue.putInline(globalObject, ident, value, slot);

    LOG_IC((ICEvent::OperationPutByIdStrictOptimize, baseValue.classInfoOrNull(vm), ident, slot.base() == baseValue));

    RETURN_IF_EXCEPTION(scope, void());

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCachingBy(vm, codeBlock, structure, identifier))
        repatchPutByID(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, PutKind::NotDirect);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdNonStrictOptimize, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseValue, false, codeBlock->putByIdContext());

    Structure* structure = CommonSlowPaths::originalStructureBeforePut(vm, baseValue);
    baseValue.putInline(globalObject, ident, value, slot);

    LOG_IC((ICEvent::OperationPutByIdNonStrictOptimize, baseValue.classInfoOrNull(vm), ident, slot.base() == baseValue));

    RETURN_IF_EXCEPTION(scope, void());

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCachingBy(vm, codeBlock, structure, identifier))
        repatchPutByID(globalObject, codeBlock, baseValue, structure, identifier, slot, *stubInfo, PutKind::NotDirect);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectStrictOptimize, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseObject, true, codeBlock->putByIdContext());
    Structure* structure = nullptr;
    CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, ident, value, slot, &structure);

    LOG_IC((ICEvent::OperationPutByIdDirectStrictOptimize, baseObject->classInfo(vm), ident, slot.base() == baseObject));

    RETURN_IF_EXCEPTION(scope, void());
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCachingBy(vm, codeBlock, structure, identifier))
        repatchPutByID(globalObject, codeBlock, baseObject, structure, identifier, slot, *stubInfo, PutKind::Direct);
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDirectNonStrictOptimize, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    CodeBlock* codeBlock = callFrame->codeBlock();
    PutPropertySlot slot(baseObject, false, codeBlock->putByIdContext());
    Structure* structure = nullptr;
    CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, ident, value, slot, &structure);

    LOG_IC((ICEvent::OperationPutByIdDirectNonStrictOptimize, baseObject->classInfo(vm), ident, slot.base() == baseObject));

    RETURN_IF_EXCEPTION(scope, void());
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCachingBy(vm, codeBlock, structure, identifier))
        repatchPutByID(globalObject, codeBlock, baseObject, structure, identifier, slot, *stubInfo, PutKind::Direct);
}

} // extern "C"

template<typename PutPrivateFieldCallback>
ALWAYS_INLINE static void setPrivateField(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, CacheableIdentifier identifier, JSValue value, PutPrivateFieldCallback callback)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    ASSERT(ident.isPrivateName());

    JSObject* baseObject = asObject(baseValue);
    CodeBlock* codeBlock = callFrame->codeBlock();
    Structure* oldStructure = baseObject->structure(vm);

    PutPropertySlot putSlot(baseObject, true, codeBlock->putByIdContext());
    baseObject->setPrivateField(globalObject, ident, value, putSlot);
    RETURN_IF_EXCEPTION(scope, void());

    callback(vm, codeBlock, oldStructure, putSlot, ident);
}

template<typename PutPrivateFieldCallback>
ALWAYS_INLINE static void definePrivateField(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, CacheableIdentifier identifier, JSValue value, PutPrivateFieldCallback callback)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    ASSERT(ident.isPrivateName());

    JSObject* baseObject = asObject(baseValue);
    CodeBlock* codeBlock = callFrame->codeBlock();
    Structure* oldStructure = baseObject->structure(vm);

    PutPropertySlot putSlot(baseObject, true, codeBlock->putByIdContext());
    baseObject->definePrivateField(globalObject, ident, value, putSlot);
    RETURN_IF_EXCEPTION(scope, void());

    callback(vm, codeBlock, oldStructure, putSlot, ident);
}

extern "C" {

JSC_DEFINE_JIT_OPERATION(operationPutByIdDefinePrivateFieldStrict, void, (JSGlobalObject* globalObject, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);

    definePrivateField(vm, globalObject, callFrame, baseValue, identifier, value, [](VM&, CodeBlock*, Structure*, PutPropertySlot&, const Identifier&) { });
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdDefinePrivateFieldStrictOptimize, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));

    definePrivateField(vm, globalObject, callFrame, baseObject, identifier, value, [=](VM& vm, CodeBlock* codeBlock, Structure* oldStructure, PutPropertySlot& putSlot, const Identifier& ident) {
        LOG_IC((ICEvent::OperationPutByIdDefinePrivateFieldFieldStrictOptimize, baseObject->classInfo(vm), ident, putSlot.base() == baseObject));

        ASSERT_UNUSED(accessType, accessType == static_cast<AccessType>(stubInfo->accessType));

        if (stubInfo->considerCachingBy(vm, codeBlock, oldStructure, identifier))
            repatchPutByID(globalObject, codeBlock, baseObject, oldStructure, identifier, putSlot, *stubInfo, PutKind::DirectPrivateFieldDefine);
    });
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSetPrivateFieldStrict, void, (JSGlobalObject* globalObject, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);

    setPrivateField(vm, globalObject, callFrame, baseValue, identifier, value, [](VM&, CodeBlock*, Structure*, PutPropertySlot&, const Identifier&) { });
}

JSC_DEFINE_JIT_OPERATION(operationPutByIdSetPrivateFieldStrictOptimize, void, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier))
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    NativeCallFrameTracer tracer(vm, callFrame);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));

    setPrivateField(vm, globalObject, callFrame, baseObject, identifier, value, [&](VM& vm, CodeBlock* codeBlock, Structure* oldStructure, PutPropertySlot& putSlot, const Identifier& ident) {
        LOG_IC((ICEvent::OperationPutByIdPutPrivateFieldFieldStrictOptimize, baseObject->classInfo(vm), ident, putSlot.base() == baseObject));

        ASSERT_UNUSED(accessType, accessType == static_cast<AccessType>(stubInfo->accessType));

        if (stubInfo->considerCachingBy(vm, codeBlock, oldStructure, identifier))
            repatchPutByID(globalObject, codeBlock, baseObject, oldStructure, identifier, putSlot, *stubInfo, PutKind::DirectPrivateFieldSet);
    });
}

static void putByVal(JSGlobalObject* globalObject, JSValue baseValue, JSValue subscript, JSValue value, ByValInfo* byValInfo, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (Optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        byValInfo->tookSlowPath = true;
        uint32_t i = *index;
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canSetIndexQuickly(i, value)) {
                object->setIndexQuickly(vm, i, value);
                return;
            }

            byValInfo->arrayProfile->setOutOfBounds();
            scope.release();
            object->methodTable(vm)->putByIndex(object, globalObject, i, value, ecmaMode.isStrict());
            return;
        }

        scope.release();
        baseValue.putByIndex(globalObject, i, value, ecmaMode.isStrict());
        return;
    } 
    if (subscript.isNumber()) {
        byValInfo->tookSlowPath = true;
        if (baseValue.isObject())
            byValInfo->arrayProfile->setOutOfBounds();
    }

    auto property = subscript.toPropertyKey(globalObject);
    // Don't put to an object if toString threw an exception.
    RETURN_IF_EXCEPTION(scope, void());

    if (byValInfo->stubInfo && (!CacheableIdentifier::isCacheableIdentifierCell(subscript) || byValInfo->cachedId.uid() != property))
        byValInfo->tookSlowPath = true;

    scope.release();
    PutPropertySlot slot(baseValue, ecmaMode.isStrict());
    baseValue.putInline(globalObject, property, value, slot);
}

static void directPutByVal(JSGlobalObject* globalObject, JSObject* baseObject, JSValue subscript, JSValue value, ByValInfo* byValInfo, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (Optional<uint32_t> maybeIndex = subscript.tryGetAsUint32Index()) {
        byValInfo->tookSlowPath = true;
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
            byValInfo->arrayProfile->setOutOfBounds();
            break;
        }

        scope.release();
        baseObject->putDirectIndex(globalObject, index, value, 0, ecmaMode.isStrict() ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }

    // Don't put to an object if toString threw an exception.
    auto property = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    if (Optional<uint32_t> index = parseIndex(property)) {
        byValInfo->tookSlowPath = true;
        scope.release();
        baseObject->putDirectIndex(globalObject, index.value(), value, 0, ecmaMode.isStrict() ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }

    if (byValInfo->stubInfo && (!CacheableIdentifier::isCacheableIdentifierCell(subscript) || byValInfo->cachedId.uid() != property))
        byValInfo->tookSlowPath = true;

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

static OptimizationResult tryPutByValOptimize(JSGlobalObject* globalObject, CallFrame* callFrame, CodeBlock* codeBlock, JSValue baseValue, JSValue subscript, ByValInfo* byValInfo, ReturnAddressPtr returnAddress)
{
    UNUSED_PARAM(callFrame);

    // See if it's worth optimizing at all.
    OptimizationResult optimizationResult = OptimizationResult::NotOptimized;

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (baseValue.isObject() && isCopyOnWrite(baseValue.getObject()->indexingMode()))
        return OptimizationResult::GiveUp;

    if (baseValue.isObject() && subscript.isInt32()) {
        JSObject* object = asObject(baseValue);

        ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
        ASSERT(!byValInfo->stubRoutine);

        Structure* structure = object->structure(vm);
        if (hasOptimizableIndexing(structure)) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(structure);
            if (jitArrayModePermitsPut(arrayMode) && arrayMode != byValInfo->arrayMode) {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                byValInfo->arrayProfile->computeUpdatedPrediction(locker, codeBlock, structure);
                JIT::compilePutByVal(locker, vm, codeBlock, byValInfo, returnAddress, arrayMode);
                optimizationResult = OptimizationResult::Optimized;
            }
        }

        // If we failed to patch and we have some object that intercepts indexed get, then don't even wait until 10 times.
        if (optimizationResult != OptimizationResult::Optimized && object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
            optimizationResult = OptimizationResult::GiveUp;
    }

    if (baseValue.isObject() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, OptimizationResult::GiveUp);
        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
            ASSERT(!byValInfo->stubRoutine);
            if (byValInfo->seen) {
                if (byValInfo->cachedId.uid() == propertyName) {
                    JIT::compilePutByValWithCachedId<OpPutByVal>(vm, codeBlock, byValInfo, returnAddress, PutKind::NotDirect, byValInfo->cachedId);
                    optimizationResult = OptimizationResult::Optimized;
                } else {
                    // Seem like a generic property access site.
                    optimizationResult = OptimizationResult::GiveUp;
                }
            } else {
                {
                    ConcurrentJSLocker locker(codeBlock->m_lock);
                    byValInfo->seen = true;
                    byValInfo->cachedId = CacheableIdentifier::createFromCell(subscript.asCell());
                    optimizationResult = OptimizationResult::SeenOnce;
                }
                vm.heap.writeBarrier(codeBlock, subscript.asCell());
            }
        }
    }

    if (optimizationResult != OptimizationResult::Optimized && optimizationResult != OptimizationResult::SeenOnce) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. For cases where we see non-index-intercepting
        // objects, this gives 10 iterations worth of opportunity for us to observe
        // that the put_by_val may be polymorphic. We count up slowPathCount even if
        // the result is GiveUp.
        if (++byValInfo->slowPathCount >= 10)
            optimizationResult = OptimizationResult::GiveUp;
    }

    return optimizationResult;
}

JSC_DEFINE_JIT_OPERATION(operationPutByValOptimize, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    CodeBlock* codeBlock = callFrame->codeBlock();
    OptimizationResult result = tryPutByValOptimize(globalObject, callFrame, codeBlock, baseValue, subscript, byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS));
    RETURN_IF_EXCEPTION(scope, void());
    if (result == OptimizationResult::GiveUp) {
        // Don't ever try to optimize.
        byValInfo->tookSlowPath = true;
        ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), operationPutByValGeneric);
    }
    RELEASE_AND_RETURN(scope, putByVal(globalObject, baseValue, subscript, value, byValInfo, ecmaMode));
}

static OptimizationResult tryDirectPutByValOptimize(JSGlobalObject* globalObject, CallFrame* callFrame, CodeBlock* codeBlock, JSObject* object, JSValue subscript, ByValInfo* byValInfo, ReturnAddressPtr returnAddress)
{
    UNUSED_PARAM(callFrame);

    // See if it's worth optimizing at all.
    OptimizationResult optimizationResult = OptimizationResult::NotOptimized;

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (subscript.isInt32()) {
        ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
        ASSERT(!byValInfo->stubRoutine);

        Structure* structure = object->structure(vm);
        if (hasOptimizableIndexing(structure)) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(structure);
            if (jitArrayModePermitsPutDirect(arrayMode) && arrayMode != byValInfo->arrayMode) {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                byValInfo->arrayProfile->computeUpdatedPrediction(locker, codeBlock, structure);

                JIT::compileDirectPutByVal(locker, vm, codeBlock, byValInfo, returnAddress, arrayMode);
                optimizationResult = OptimizationResult::Optimized;
            }
        }

        // If we failed to patch and we have some object that intercepts indexed get, then don't even wait until 10 times.
        if (optimizationResult != OptimizationResult::Optimized && object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
            optimizationResult = OptimizationResult::GiveUp;
    } else if (CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, OptimizationResult::GiveUp);
        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
            ASSERT(!byValInfo->stubRoutine);
            if (byValInfo->seen) {
                if (byValInfo->cachedId.uid() == propertyName) {
                    JIT::compilePutByValWithCachedId<OpPutByValDirect>(vm, codeBlock, byValInfo, returnAddress, PutKind::Direct, byValInfo->cachedId);
                    optimizationResult = OptimizationResult::Optimized;
                } else {
                    // Seem like a generic property access site.
                    optimizationResult = OptimizationResult::GiveUp;
                }
            } else {
                {
                    ConcurrentJSLocker locker(codeBlock->m_lock);
                    byValInfo->seen = true;
                    byValInfo->cachedId = CacheableIdentifier::createFromCell(subscript.asCell());
                    optimizationResult = OptimizationResult::SeenOnce;
                }
                vm.heap.writeBarrier(codeBlock, subscript.asCell());
            }
        }
    }

    if (optimizationResult != OptimizationResult::Optimized && optimizationResult != OptimizationResult::SeenOnce) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. For cases where we see non-index-intercepting
        // objects, this gives 10 iterations worth of opportunity for us to observe
        // that the get_by_val may be polymorphic. We count up slowPathCount even if
        // the result is GiveUp.
        if (++byValInfo->slowPathCount >= 10)
            optimizationResult = OptimizationResult::GiveUp;
    }

    return optimizationResult;
}

JSC_DEFINE_JIT_OPERATION(operationDirectPutByValOptimize, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());
    JSObject* object = asObject(baseValue);
    CodeBlock* codeBlock = callFrame->codeBlock();
    OptimizationResult result = tryDirectPutByValOptimize(globalObject, callFrame, codeBlock, object, subscript, byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS));
    RETURN_IF_EXCEPTION(scope, void());
    if (result == OptimizationResult::GiveUp) {
        // Don't ever try to optimize.
        byValInfo->tookSlowPath = true;
        ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), operationDirectPutByValGeneric);
    }

    RELEASE_AND_RETURN(scope, directPutByVal(globalObject, object, subscript, value, byValInfo, ecmaMode));
}

JSC_DEFINE_JIT_OPERATION(operationPutByValGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(globalObject, baseValue, subscript, value, byValInfo, ecmaMode);
}


JSC_DEFINE_JIT_OPERATION(operationDirectPutByValGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());
    directPutByVal(globalObject, asObject(baseValue), subscript, value, byValInfo, ecmaMode);
}

JSC_DEFINE_JIT_OPERATION(operationPutPrivateNameOptimize, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo, PrivateFieldPutKind putKind))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    auto propertyName = subscript.toPropertyKey(globalObject);
    EXCEPTION_ASSERT(!scope.exception());

    OptimizationResult optimizationResult = OptimizationResult::NotOptimized;

    if (baseValue.isObject() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        ASSERT(subscript.isSymbol());
        ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
        ASSERT(!byValInfo->stubRoutine);
        if (byValInfo->seen) {
            if (byValInfo->cachedId.uid() == propertyName) {
                JIT::compilePutPrivateNameWithCachedId(vm, codeBlock, byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), byValInfo->cachedId);
                optimizationResult = OptimizationResult::Optimized;
            } else {
                // Seem like a generic property access site.
                optimizationResult = OptimizationResult::GiveUp;
            }
        } else {
            {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                byValInfo->seen = true;
                byValInfo->cachedId = CacheableIdentifier::createFromCell(subscript.asCell());
                optimizationResult = OptimizationResult::SeenOnce;
            }
            vm.heap.writeBarrier(codeBlock, subscript.asCell());
        }
    }

    if (optimizationResult != OptimizationResult::Optimized && optimizationResult != OptimizationResult::SeenOnce) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. This gives 10 iterations worth of opportunity
        // for us to observe that the put_private_name may be polymorphic.
        // We count up slowPathCount even if the result is GiveUp.
        if (++byValInfo->slowPathCount >= 10)
            optimizationResult = OptimizationResult::GiveUp;
    }

    if (optimizationResult == OptimizationResult::GiveUp) {
        // Don't ever try to optimize.
        byValInfo->tookSlowPath = true;
        ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), operationPutPrivateNameGeneric);
    }

    scope.release();
    
    // Private fields can only be accessed within class lexical scope
    // and class methods are always in strict mode
    const bool isStrictMode = true;
    auto baseObject = asObject(baseValue);
    PutPropertySlot slot(baseObject, isStrictMode);
    if (putKind.isDefine())
        baseObject->definePrivateField(globalObject, propertyName, value, slot);
    else
        baseObject->setPrivateField(globalObject, propertyName, value, slot);
}

// We need to match the signature of operationPutPrivateNameOptimize
JSC_DEFINE_JIT_OPERATION(operationPutPrivateNameGeneric, void, (JSGlobalObject* globalObject, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo, PrivateFieldPutKind privateFieldPutKind))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    auto propertyName = subscript.toPropertyKey(globalObject);
    EXCEPTION_ASSERT(!scope.exception());

    scope.release();

    // Private fields can only be accessed within class lexical scope
    // and class methods are always in strict mode
    const bool isStrictMode = true;
    auto baseObject = asObject(baseValue);
    PutPropertySlot slot(baseObject, isStrictMode);
    if (privateFieldPutKind.isDefine())
        baseObject->definePrivateField(globalObject, propertyName, value, slot);
    else
        baseObject->setPrivateField(globalObject, propertyName, value, slot);

    if (byValInfo)
        byValInfo->tookSlowPath = true;
}

JSC_DEFINE_JIT_OPERATION(operationCallEval, EncodedJSValue, (JSGlobalObject* globalObject, CallFrame* calleeFrame, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calleeFrame->setCodeBlock(nullptr);
    
    if (!isHostFunction(calleeFrame->guaranteedJSValueCallee(), globalFuncEval))
        return JSValue::encode(JSValue());

    JSValue result = eval(globalObject, calleeFrame, ecmaMode);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    return JSValue::encode(result);
}

static SlowPathReturnType handleHostCall(JSGlobalObject* globalObject, CallFrame* calleeFrame, JSValue callee, CallLinkInfo* callLinkInfo)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    calleeFrame->setCodeBlock(nullptr);

    if (callLinkInfo->specializationKind() == CodeForCall) {
        auto callData = getCallData(vm, callee);
        ASSERT(callData.type != CallData::Type::JS);

        if (callData.type == CallData::Type::Native) {
            NativeCallFrameTracer tracer(vm, calleeFrame);
            calleeFrame->setCallee(asObject(callee));
            vm.encodedHostCallReturnValue = callData.native.function(asObject(callee)->globalObject(vm), calleeFrame);
            DisallowGC disallowGC;
            if (UNLIKELY(scope.exception())) {
                return encodeResult(
                    vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress(),
                    reinterpret_cast<void*>(KeepTheFrame));
            }

            return encodeResult(
                LLInt::getHostCallReturnValueEntrypoint().code().executableAddress(),
                reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
        }
    
        ASSERT(callData.type == CallData::Type::None);
        throwException(globalObject, scope, createNotAFunctionError(globalObject, callee));
        return encodeResult(
            vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress(),
            reinterpret_cast<void*>(KeepTheFrame));
    }

    ASSERT(callLinkInfo->specializationKind() == CodeForConstruct);

    auto constructData = getConstructData(vm, callee);
    ASSERT(constructData.type != CallData::Type::JS);

    if (constructData.type == CallData::Type::Native) {
        NativeCallFrameTracer tracer(vm, calleeFrame);
        calleeFrame->setCallee(asObject(callee));
        vm.encodedHostCallReturnValue = constructData.native.function(asObject(callee)->globalObject(vm), calleeFrame);
        DisallowGC disallowGC;
        if (UNLIKELY(scope.exception())) {
            return encodeResult(
                vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }

        return encodeResult(LLInt::getHostCallReturnValueEntrypoint().code().executableAddress(), reinterpret_cast<void*>(KeepTheFrame));
    }

    ASSERT(constructData.type == CallData::Type::None);
    throwException(globalObject, scope, createNotAConstructorError(globalObject, callee));
    return encodeResult(
        vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress(),
        reinterpret_cast<void*>(KeepTheFrame));
}

JSC_DEFINE_JIT_OPERATION(operationLinkCall, SlowPathReturnType, (CallFrame* calleeFrame, JSGlobalObject* globalObject, CallLinkInfo* callLinkInfo))
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
        if (auto* internalFunction = jsDynamicCast<InternalFunction*>(vm, calleeAsValue)) {
            MacroAssemblerCodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            RELEASE_ASSERT(!!codePtr);

            if (!callLinkInfo->seenOnce())
                callLinkInfo->setSeen();
            else
                linkFor(vm, calleeFrame, *callLinkInfo, nullptr, internalFunction, codePtr);

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
    if (executable->isHostFunction()) {
        codePtr = jsToWasmICCodePtr(vm, kind, callee);
        if (!codePtr)
            codePtr = executable->entrypointFor(kind, MustCheckArity);
    } else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        auto handleThrowException = [&] () {
            void* throwTarget = vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress();
            return encodeResult(throwTarget, reinterpret_cast<void*>(KeepTheFrame));
        };

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            throwException(globalObject, throwScope, createNotAConstructorError(globalObject, callee));
            return handleThrowException();
        }

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        Exception* error = functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, scope, kind, *codeBlockSlot);
        EXCEPTION_ASSERT(throwScope.exception() == error);
        if (UNLIKELY(error))
            return handleThrowException();
        codeBlock = *codeBlockSlot;
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
        linkFor(vm, calleeFrame, *callLinkInfo, codeBlock, callee, codePtr);

    return encodeResult(codePtr.executableAddress(), reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
}

inline SlowPathReturnType virtualForWithFunction(JSGlobalObject* globalObject, CallFrame* calleeFrame, CallLinkInfo* callLinkInfo, JSCell*& calleeAsFunctionCell)
{
    CallFrame* callFrame = calleeFrame->callerFrame();
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeSpecializationKind kind = callLinkInfo->specializationKind();
    NativeCallFrameTracer tracer(vm, callFrame);

    JSValue calleeAsValue = calleeFrame->guaranteedJSValueCallee();
    calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell)) {
        if (jsDynamicCast<InternalFunction*>(vm, calleeAsValue)) {
            MacroAssemblerCodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            ASSERT(!!codePtr);
            return encodeResult(codePtr.executableAddress(), reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(globalObject, calleeFrame, calleeAsValue, callLinkInfo));
    }
    
    JSFunction* function = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = function->scopeUnchecked();
    ExecutableBase* executable = function->executable();
    if (UNLIKELY(!executable->hasJITCodeFor(kind))) {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            throwException(globalObject, throwScope, createNotAConstructorError(globalObject, function));
            return encodeResult(
                vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        Exception* error = functionExecutable->prepareForExecution<FunctionExecutable>(vm, function, scope, kind, *codeBlockSlot);
        EXCEPTION_ASSERT(throwScope.exception() == error);
        if (UNLIKELY(error)) {
            return encodeResult(
                vm.getCTIStub(throwExceptionFromCallSlowPathGenerator).retaggedCode<JSEntryPtrTag>().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }
    }
    return encodeResult(executable->entrypointFor(
        kind, MustCheckArity).executableAddress(),
        reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
}

JSC_DEFINE_JIT_OPERATION(operationLinkPolymorphicCall, SlowPathReturnType, (CallFrame* calleeFrame, JSGlobalObject* globalObject, CallLinkInfo* callLinkInfo))
{
    ASSERT(callLinkInfo->specializationKind() == CodeForCall);
    JSCell* calleeAsFunctionCell;
    SlowPathReturnType result = virtualForWithFunction(globalObject, calleeFrame, callLinkInfo, calleeAsFunctionCell);

    linkPolymorphicCall(globalObject, calleeFrame, *callLinkInfo, CallVariant(calleeAsFunctionCell));
    
    return result;
}

JSC_DEFINE_JIT_OPERATION(operationVirtualCall, SlowPathReturnType, (CallFrame* calleeFrame, JSGlobalObject* globalObject, CallLinkInfo* callLinkInfo))
{
    JSCell* calleeAsFunctionCellIgnored;
    return virtualForWithFunction(globalObject, calleeFrame, callLinkInfo, calleeAsFunctionCellIgnored);
}

JSC_DEFINE_JIT_OPERATION(operationCompareLess, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return jsLess<true>(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationCompareLessEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return jsLessEq<true>(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

JSC_DEFINE_JIT_OPERATION(operationCompareGreater, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return jsLess<false>(globalObject, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

JSC_DEFINE_JIT_OPERATION(operationCompareGreaterEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return jsLessEq<false>(globalObject, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

JSC_DEFINE_JIT_OPERATION(operationCompareEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSValue::equalSlowCaseInline(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
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

    bool result = asString(left)->equal(globalObject, asString(right));
#if USE(JSVALUE64)
    return JSValue::encode(jsBoolean(result));
#else
    return result;
#endif
}

JSC_DEFINE_JIT_OPERATION(operationCompareStrictEq, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue src1 = JSValue::decode(encodedOp1);
    JSValue src2 = JSValue::decode(encodedOp2);

    return JSValue::strictEqual(globalObject, src1, src2);
}

#if USE(BIGINT32)
JSC_DEFINE_JIT_OPERATION(operationCompareEqHeapBigIntToInt32, size_t, (JSGlobalObject* globalObject, JSCell* heapBigInt, int32_t smallInt))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(heapBigInt->isHeapBigInt());

    return static_cast<JSBigInt*>(heapBigInt)->equalsToInt32(smallInt);
}
#endif

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithProfile, EncodedJSValue, (JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const JSValue* values, int size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(constructArrayNegativeIndexed(globalObject, profile, values, size));
}

JSC_DEFINE_JIT_OPERATION(operationNewArrayWithSizeAndProfile, EncodedJSValue, (JSGlobalObject* globalObject, ArrayAllocationProfile* profile, EncodedJSValue size))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue sizeValue = JSValue::decode(size);
    return JSValue::encode(constructArrayWithSizeQuirk(globalObject, profile, sizeValue));
}

}

template<typename FunctionType>
static EncodedJSValue newFunctionCommon(VM& vm, JSScope* scope, JSCell* functionExecutable, bool isInvalidated)
{
    ASSERT(functionExecutable->inherits<FunctionExecutable>(vm));
    if (isInvalidated)
        return JSValue::encode(FunctionType::createWithInvalidatedReallocationWatchpoint(vm, static_cast<FunctionExecutable*>(functionExecutable), scope));
    return JSValue::encode(FunctionType::create(vm, static_cast<FunctionExecutable*>(functionExecutable), scope));
}

extern "C" {

JSC_DEFINE_JIT_OPERATION(operationNewFunction, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSFunction>(vm, scope, functionExecutable, false);
}

JSC_DEFINE_JIT_OPERATION(operationNewFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSFunction>(vm, scope, functionExecutable, true);
}

JSC_DEFINE_JIT_OPERATION(operationNewGeneratorFunction, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSGeneratorFunction>(vm, scope, functionExecutable, false);
}

JSC_DEFINE_JIT_OPERATION(operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSGeneratorFunction>(vm, scope, functionExecutable, true);
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncFunction, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSAsyncFunction>(vm, scope, functionExecutable, false);
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSAsyncFunction>(vm, scope, functionExecutable, true);
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncGeneratorFunction, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSAsyncGeneratorFunction>(vm, scope, functionExecutable, false);
}
    
JSC_DEFINE_JIT_OPERATION(operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM* vmPointer, JSScope* scope, JSCell* functionExecutable))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return newFunctionCommon<JSAsyncGeneratorFunction>(vm, scope, functionExecutable, true);
}
    
JSC_DEFINE_JIT_OPERATION(operationSetFunctionName, void, (JSGlobalObject* globalObject, JSCell* funcCell, EncodedJSValue encodedName))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSFunction* func = jsCast<JSFunction*>(funcCell);
    JSValue name = JSValue::decode(encodedName);
    func->setFunctionName(globalObject, name);
}

JSC_DEFINE_JIT_OPERATION(operationNewObject, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return constructEmptyObject(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewPromise, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSPromise::create(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewInternalPromise, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSInternalPromise::create(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewGenerator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSGenerator::create(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewAsyncGenerator, JSCell*, (VM* vmPointer, Structure* structure))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return JSAsyncGenerator::create(vm, structure);
}

JSC_DEFINE_JIT_OPERATION(operationNewRegexp, JSCell*, (JSGlobalObject* globalObject, JSCell* regexpPtr))
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    ASSERT(regexp->isValid());
    return RegExpObject::create(vm, globalObject->regExpStructure(), regexp);
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
    ASSERT(vm.needTrapHandling());
    vm.handleTraps(globalObject, callFrame);
    return nullptr;
}

JSC_DEFINE_JIT_OPERATION(operationDebug, void, (VM* vmPointer, int32_t debugHookType))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    vm.interpreter->debug(callFrame, static_cast<DebugHookType>(debugHookType));
}

#if ENABLE(DFG_JIT)
static void updateAllPredictionsAndOptimizeAfterWarmUp(CodeBlock* codeBlock)
{
    codeBlock->updateAllPredictions();
    codeBlock->optimizeAfterWarmUp();
}

JSC_DEFINE_JIT_OPERATION(operationOptimize, SlowPathReturnType, (VM* vmPointer, uint32_t bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
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
    DeferGCForAWhile deferGC(vm.heap);
    
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (UNLIKELY(codeBlock->jitType() != JITType::BaselineJIT)) {
        dataLog("Unexpected code block in Baseline->DFG tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (bytecodeIndex) {
        // If we're attempting to OSR from a loop, assume that this should be
        // separately optimized.
        codeBlock->m_shouldAlwaysBeInlined = false;
    }

    if (UNLIKELY(Options::verboseOSR())) {
        dataLog(
            *codeBlock, ": Entered optimize with bytecodeIndex = ", bytecodeIndex,
            ", executeCounter = ", codeBlock->jitExecuteCounter(),
            ", optimizationDelayCounter = ", codeBlock->reoptimizationRetryCounter(),
            ", exitCounter = ");
        if (codeBlock->hasOptimizedReplacement())
            dataLog(codeBlock->replacement()->osrExitCounter());
        else
            dataLog("N/A");
        dataLog("\n");
    }

    if (!codeBlock->checkIfOptimizationThresholdReached()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("counter = ", codeBlock->jitExecuteCounter()));
        codeBlock->updateAllPredictions();
        dataLogLnIf(Options::verboseOSR(), "Choosing not to optimize ", *codeBlock, " yet, because the threshold hasn't been reached.");
        return encodeResult(nullptr, nullptr);
    }
    
    Debugger* debugger = codeBlock->globalObject()->debugger();
    if (UNLIKELY(debugger && (debugger->isStepping() || codeBlock->baselineAlternative()->hasDebuggerRequests()))) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("debugger is stepping or has requests"));
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        return encodeResult(nullptr, nullptr);
    }

    if (codeBlock->m_shouldAlwaysBeInlined) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("should always be inlined"));
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        dataLogLnIf(Options::verboseOSR(), "Choosing not to optimize ", *codeBlock, " yet, because m_shouldAlwaysBeInlined == true.");
        return encodeResult(nullptr, nullptr);
    }

    // We cannot be in the process of asynchronous compilation and also have an optimized
    // replacement.
    DFG::Worklist* worklist = DFG::existingGlobalDFGWorklistOrNull();
    ASSERT(
        !worklist
        || !(worklist->compilationState(DFG::CompilationKey(codeBlock, DFG::DFGMode)) != DFG::Worklist::NotKnown
        && codeBlock->hasOptimizedReplacement()));

    DFG::Worklist::State worklistState;
    if (worklist) {
        // The call to DFG::Worklist::completeAllReadyPlansForVM() will complete all ready
        // (i.e. compiled) code blocks. But if it completes ours, we also need to know
        // what the result was so that we don't plow ahead and attempt OSR or immediate
        // reoptimization. This will have already also set the appropriate JIT execution
        // count threshold depending on what happened, so if the compilation was anything
        // but successful we just want to return early. See the case for worklistState ==
        // DFG::Worklist::Compiled, below.
        
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
        worklistState = worklist->completeAllReadyPlansForVM(
            vm, DFG::CompilationKey(codeBlock, DFG::DFGMode));
    } else
        worklistState = DFG::Worklist::NotKnown;

    if (worklistState == DFG::Worklist::Compiling) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("compiling"));
        // We cannot be in the process of asynchronous compilation and also have an optimized
        // replacement.
        RELEASE_ASSERT(!codeBlock->hasOptimizedReplacement());
        codeBlock->setOptimizationThresholdBasedOnCompilationResult(CompilationDeferred);
        return encodeResult(nullptr, nullptr);
    }

    if (worklistState == DFG::Worklist::Compiled) {
        // If we don't have an optimized replacement but we did just get compiled, then
        // the compilation failed or was invalidated, in which case the execution count
        // thresholds have already been set appropriately by
        // CodeBlock::setOptimizationThresholdBasedOnCompilationResult() and we have
        // nothing left to do.
        if (!codeBlock->hasOptimizedReplacement()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("compiled and failed"));
            codeBlock->updateAllPredictions();
            dataLogLnIf(Options::verboseOSR(), "Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.");
            return encodeResult(nullptr, nullptr);
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
            return encodeResult(nullptr, nullptr);
        }
    } else {
        if (!codeBlock->shouldOptimizeNow()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("insufficient profiling"));
            dataLogLnIf(Options::verboseOSR(),
                "Delaying optimization for ", *codeBlock,
                " because of insufficient profiling.");
            return encodeResult(nullptr, nullptr);
        }

        dataLogLnIf(Options::verboseOSR(), "Triggering optimized compilation of ", *codeBlock);

        unsigned numVarsWithValues = 0;
        if (bytecodeIndex)
            numVarsWithValues = codeBlock->numCalleeLocals();

        Operands<Optional<JSValue>> mustHandleValues(codeBlock->numParameters(), numVarsWithValues, 0);
        int localsUsedForCalleeSaves = static_cast<int>(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters());
        for (size_t i = 0; i < mustHandleValues.size(); ++i) {
            Operand operand = mustHandleValues.operandForIndex(i);

            if (operand.isLocal() && operand.toLocal() < localsUsedForCalleeSaves)
                continue;
            mustHandleValues[i] = callFrame->uncheckedR(operand.virtualRegister()).jsValue();
        }

        CodeBlock* replacementCodeBlock = codeBlock->newReplacement();
        CompilationResult result = DFG::compile(
            vm, replacementCodeBlock, nullptr, DFG::DFGMode, bytecodeIndex,
            mustHandleValues, JITToDFGDeferredCompilationCallback::create());
        
        if (result != CompilationSuccessful) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("compilation failed"));
            return encodeResult(nullptr, nullptr);
        }
    }
    
    CodeBlock* optimizedCodeBlock = codeBlock->replacement();
    ASSERT(optimizedCodeBlock && JITCode::isOptimizingJIT(optimizedCodeBlock->jitType()));
    
    if (void* dataBuffer = DFG::prepareOSREntry(vm, callFrame, optimizedCodeBlock, bytecodeIndex)) {
        CODEBLOCK_LOG_EVENT(optimizedCodeBlock, "osrEntry", ("at bc#", bytecodeIndex));
        dataLogLnIf(Options::verboseOSR(), "Performing OSR ", codeBlock, " -> ", optimizedCodeBlock);

        codeBlock->optimizeSoon();
        codeBlock->unlinkedCodeBlock()->setDidOptimize(TriState::True);
        void* targetPC = vm.getCTIStub(DFG::osrEntryThunkGenerator).code().executableAddress();
        targetPC = retagCodePtr(targetPC, JITThunkPtrTag, bitwise_cast<PtrTag>(callFrame));
        return encodeResult(targetPC, dataBuffer);
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
        return encodeResult(nullptr, nullptr);
    }

    // OSR failed this time, but it might succeed next time! Let the code run a bit
    // longer and then try again.
    codeBlock->optimizeAfterWarmUp();
    
    CODEBLOCK_LOG_EVENT(codeBlock, "delayOptimizeToDFG", ("OSR failed"));
    return encodeResult(nullptr, nullptr);
}

JSC_DEFINE_JIT_OPERATION(operationTryOSREnterAtCatch, char*, (VM* vmPointer, uint32_t bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    BytecodeIndex bytecodeIndex = BytecodeIndex::fromBits(bytecodeIndexBits);

    CodeBlock* codeBlock = callFrame->codeBlock();
    CodeBlock* optimizedReplacement = codeBlock->replacement();
    if (UNLIKELY(!optimizedReplacement))
        return nullptr;

    switch (optimizedReplacement->jitType()) {
    case JITType::DFGJIT:
    case JITType::FTLJIT: {
        MacroAssemblerCodePtr<ExceptionHandlerPtrTag> entry = DFG::prepareCatchOSREntry(vm, callFrame, codeBlock, optimizedReplacement, bytecodeIndex);
        return entry.executableAddress<char*>();
    }
    default:
        break;
    }
    return nullptr;
}

JSC_DEFINE_JIT_OPERATION(operationTryOSREnterAtCatchAndValueProfile, char*, (VM* vmPointer, uint32_t bytecodeIndexBits))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    BytecodeIndex bytecodeIndex = BytecodeIndex::fromBits(bytecodeIndexBits);

    CodeBlock* codeBlock = callFrame->codeBlock();
    CodeBlock* optimizedReplacement = codeBlock->replacement();
    if (UNLIKELY(!optimizedReplacement))
        return nullptr;

    switch (optimizedReplacement->jitType()) {
    case JITType::DFGJIT:
    case JITType::FTLJIT: {
        MacroAssemblerCodePtr<ExceptionHandlerPtrTag> entry = DFG::prepareCatchOSREntry(vm, callFrame, codeBlock, optimizedReplacement, bytecodeIndex);
        return entry.executableAddress<char*>();
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

    return nullptr;
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

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    ASSERT(getter->isObject());
    baseObj->putGetter(globalObject, uid, getter, options);
}

JSC_DEFINE_JIT_OPERATION(operationPutSetterById, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t options, JSCell* setter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    ASSERT(setter->isObject());
    baseObj->putSetter(globalObject, uid, setter, options);
}

JSC_DEFINE_JIT_OPERATION(operationPutGetterByVal, void, (JSGlobalObject* globalObject, JSCell* base, EncodedJSValue encodedSubscript, int32_t attribute, JSCell* getter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    putAccessorByVal(globalObject, asObject(base), JSValue::decode(encodedSubscript), attribute, asObject(getter), AccessorType::Getter);
}

JSC_DEFINE_JIT_OPERATION(operationPutSetterByVal, void, (JSGlobalObject* globalObject, JSCell* base, EncodedJSValue encodedSubscript, int32_t attribute, JSCell* setter))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    putAccessorByVal(globalObject, asObject(base), JSValue::decode(encodedSubscript), attribute, asObject(setter), AccessorType::Setter);
}

#if USE(JSVALUE64)
JSC_DEFINE_JIT_OPERATION(operationPutGetterSetter, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t attribute, EncodedJSValue encodedGetterValue, EncodedJSValue encodedSetterValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(object && object->isObject());
    JSObject* baseObject = asObject(object);

    JSValue getter = JSValue::decode(encodedGetterValue);
    JSValue setter = JSValue::decode(encodedSetterValue);
    ASSERT(getter.isObject() || setter.isObject());
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);
    CommonSlowPaths::putDirectAccessorWithReify(vm, globalObject, baseObject, uid, accessor, attribute);
}

#else
JSC_DEFINE_JIT_OPERATION(operationPutGetterSetter, void, (JSGlobalObject* globalObject, JSCell* object, UniquedStringImpl* uid, int32_t attribute, JSCell* getterCell, JSCell* setterCell))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(object && object->isObject());
    JSObject* baseObject = asObject(object);

    ASSERT(getterCell || setterCell);
    JSObject* getter = getterCell ? getterCell->getObject() : nullptr;
    JSObject* setter = setterCell ? setterCell->getObject() : nullptr;
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);
    CommonSlowPaths::putDirectAccessorWithReify(vm, globalObject, baseObject, uid, accessor, attribute);
}
#endif

JSC_DEFINE_JIT_OPERATION(operationInstanceOfCustom, size_t, (JSGlobalObject* globalObject, EncodedJSValue encodedValue, JSObject* constructor, EncodedJSValue encodedHasInstance))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue value = JSValue::decode(encodedValue);
    JSValue hasInstanceValue = JSValue::decode(encodedHasInstance);

    if (constructor->hasInstance(globalObject, value, hasInstanceValue))
        return 1;
    return 0;
}

}

ALWAYS_INLINE static JSValue getByVal(JSGlobalObject* globalObject, CallFrame* callFrame, ArrayProfile* arrayProfile, JSValue baseValue, JSValue subscript)
{
    UNUSED_PARAM(callFrame);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            RefPtr<AtomStringImpl> existingAtomString = asString(subscript)->toExistingAtomString(globalObject);
            RETURN_IF_EXCEPTION(scope, JSValue());
            if (existingAtomString) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomString.get())) {
                    ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
                    return result;
                }
            }
        }
    }

    if (Optional<int32_t> index = subscript.tryGetAsInt32()) {
        int32_t i = *index;
        if (isJSString(baseValue)) {
            if (i >= 0 && asString(baseValue)->canGetIndex(i))
                RELEASE_AND_RETURN(scope, asString(baseValue)->getIndex(globalObject, i));
            if (arrayProfile)
                arrayProfile->setOutOfBounds();
        } else if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canGetIndexQuickly(static_cast<uint32_t>(i)))
                return object->getIndexQuickly(i);

            bool skipMarkingOutOfBounds = false;

            if (object->indexingType() == ArrayWithContiguous && i >= 0 && static_cast<uint32_t>(i) < object->butterfly()->publicLength()) {
                // FIXME: expand this to ArrayStorage, Int32, and maybe Double:
                // https://bugs.webkit.org/show_bug.cgi?id=182940
                auto* globalObject = object->globalObject(vm);
                skipMarkingOutOfBounds = globalObject->isOriginalArrayStructure(object->structure(vm)) && globalObject->arrayPrototypeChainIsSane();
            }

            if (!skipMarkingOutOfBounds && !CommonSlowPaths::canAccessArgumentIndexQuickly(*object, i)) {
                // FIXME: This will make us think that in-bounds typed array accesses are actually
                // out-of-bounds.
                // https://bugs.webkit.org/show_bug.cgi?id=149886
                if (arrayProfile)
                    arrayProfile->setOutOfBounds();
            }
        }

        if (i >= 0)
            RELEASE_AND_RETURN(scope, baseValue.get(globalObject, static_cast<uint32_t>(i)));
    } else if (subscript.isNumber() && baseValue.isCell() && arrayProfile)
        arrayProfile->setOutOfBounds();

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());
    auto property = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
    RELEASE_AND_RETURN(scope, baseValue.get(globalObject, property));
}

extern "C" {

JSC_DEFINE_JIT_OPERATION(operationGetByValGeneric, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, ArrayProfile* profile, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    stubInfo->tookSlowPath = true;

    return JSValue::encode(getByVal(globalObject, callFrame, profile, baseValue, subscript));
}

JSC_DEFINE_JIT_OPERATION(operationGetByValOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, ArrayProfile* profile, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    CodeBlock* codeBlock = callFrame->codeBlock();

    if (baseValue.isCell() && subscript.isInt32()) {
        Structure* structure = baseValue.asCell()->structure(vm);
        if (stubInfo->considerCachingGeneric(vm, codeBlock, structure)) {
            if (profile) {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                profile->computeUpdatedPrediction(locker, codeBlock, structure);
            }
            repatchArrayGetByVal(globalObject, codeBlock, baseValue, subscript, *stubInfo);
        }
    }

    if (baseValue.isCell() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            scope.release();
            return JSValue::encode(baseValue.getPropertySlot(globalObject, propertyName, [&] (bool found, PropertySlot& slot) -> JSValue {
                LOG_IC((ICEvent::OperationGetByValOptimize, baseValue.classInfoOrNull(vm), propertyName, baseValue == slot.slotBase())); 
                
                CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
                if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
                    repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::NormalByVal);
                return found ? slot.getValue(globalObject, propertyName) : jsUndefined();
            }));
        }
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(getByVal(globalObject, callFrame, profile, baseValue, subscript)));
}

JSC_DEFINE_JIT_OPERATION(operationGetByVal, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedProperty))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);

    if (LIKELY(baseValue.isCell())) {
        JSCell* base = baseValue.asCell();

        if (Optional<uint32_t> index = property.tryGetAsUint32Index())
            RELEASE_AND_RETURN(scope, getByValWithIndex(globalObject, base, *index));

        if (property.isString()) {
            Structure& structure = *base->structure(vm);
            if (JSCell::canUseFastGetOwnProperty(structure)) {
                RefPtr<AtomStringImpl> existingAtomString = asString(property)->toExistingAtomString(globalObject);
                RETURN_IF_EXCEPTION(scope, encodedJSValue());
                if (existingAtomString) {
                    if (JSValue result = base->fastGetOwnProperty(vm, structure, existingAtomString.get()))
                        return JSValue::encode(result);
                }
            }
        }
    }

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto propertyName = property.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(baseValue.get(globalObject, propertyName)));
}

} // extern "C"

ALWAYS_INLINE static JSValue getPrivateName(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue baseValue, JSValue fieldNameValue)
{
    UNUSED_PARAM(callFrame);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());
    auto fieldName = fieldNameValue.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    JSObject* base = baseValue.toObject(globalObject);
    PropertySlot slot(base, PropertySlot::InternalMethodType::GetOwnProperty);
    base->getPrivateField(globalObject, fieldName, slot);
    RETURN_IF_EXCEPTION(scope, JSValue());

    return slot.getValue(globalObject, fieldName);
}

extern "C" {

JSC_DEFINE_JIT_OPERATION(operationGetPrivateNameOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedBase, EncodedJSValue encodedFieldName))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue fieldNameValue = JSValue::decode(encodedFieldName);
    ASSERT(CacheableIdentifier::isCacheableIdentifierCell(fieldNameValue));

    CodeBlock* codeBlock = callFrame->codeBlock();

    if (baseValue.isObject()) {
        const Identifier fieldName = fieldNameValue.toPropertyKey(globalObject);
        EXCEPTION_ASSERT(!scope.exception());
        ASSERT(fieldName.isSymbol());

        JSObject* base = jsCast<JSObject*>(baseValue.asCell());
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        PropertySlot slot(base, PropertySlot::InternalMethodType::GetOwnProperty);
        base->getPrivateField(globalObject, fieldName, slot);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        LOG_IC((ICEvent::OperationGetPrivateNameOptimize, baseValue.classInfoOrNull(vm), fieldName, true));

        CacheableIdentifier identifier = CacheableIdentifier::createFromCell(fieldNameValue.asCell());
        if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
            repatchGetBy(globalObject, codeBlock, baseValue, identifier, slot, *stubInfo, GetByKind::PrivateName);
        return JSValue::encode(slot.getValue(globalObject, fieldName));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(getPrivateName(globalObject, callFrame, baseValue, fieldNameValue)));
}

JSC_DEFINE_JIT_OPERATION(operationGetPrivateName, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedBase, EncodedJSValue encodedFieldName))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue fieldNameValue = JSValue::decode(encodedFieldName);

    stubInfo->tookSlowPath = true;

    return JSValue::encode(getPrivateName(globalObject, callFrame, baseValue, fieldNameValue));
}

JSC_DEFINE_JIT_OPERATION(operationHasIndexedPropertyDefault, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    ASSERT(baseValue.isObject());
    ASSERT(subscript.isUInt32AsAnyInt());

    JSObject* object = asObject(baseValue);
    bool didOptimize = false;

    ASSERT(callFrame->bytecodeIndex() != BytecodeIndex(0));
    ASSERT(!byValInfo->stubRoutine);
    
    if (hasOptimizableIndexing(object->structure(vm))) {
        // Attempt to optimize.
        JITArrayMode arrayMode = jitArrayModeForStructure(object->structure(vm));
        if (arrayMode != byValInfo->arrayMode) {
            JIT::compileHasIndexedProperty(vm, callFrame->codeBlock(), byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), arrayMode);
            didOptimize = true;
        }
    }
    
    if (!didOptimize) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. Or, if we failed to patch and we have some object
        // that intercepts indexed get, then don't even wait until 10 times. For cases
        // where we see non-index-intercepting objects, this gives 10 iterations worth of
        // opportunity for us to observe that the get_by_val may be polymorphic.
        if (++byValInfo->slowPathCount >= 10
            || object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
            // Don't ever try to optimize.
            ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), operationHasIndexedPropertyGeneric);
        }
    }

    uint32_t index = subscript.asUInt32AsAnyInt();
    if (object->canGetIndexQuickly(index))
        return JSValue::encode(JSValue(JSValue::JSTrue));

    if (!CommonSlowPaths::canAccessArgumentIndexQuickly(*object, index))
        byValInfo->arrayProfile->setOutOfBounds();
    return JSValue::encode(jsBoolean(object->hasPropertyGeneric(globalObject, index, PropertySlot::InternalMethodType::GetOwnProperty)));
}
    
JSC_DEFINE_JIT_OPERATION(operationHasIndexedPropertyGeneric, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    ASSERT(baseValue.isObject());
    ASSERT(subscript.isUInt32AsAnyInt());

    JSObject* object = asObject(baseValue);
    uint32_t index = subscript.asUInt32AsAnyInt();
    if (object->canGetIndexQuickly(index))
        return JSValue::encode(JSValue(JSValue::JSTrue));

    if (!CommonSlowPaths::canAccessArgumentIndexQuickly(*object, index))
        byValInfo->arrayProfile->setOutOfBounds();
    return JSValue::encode(jsBoolean(object->hasPropertyGeneric(globalObject, index, PropertySlot::InternalMethodType::GetOwnProperty)));
}
    
static bool deleteById(JSGlobalObject* globalObject, VM& vm, DeletePropertySlot& slot, JSValue base, const Identifier& ident, ECMAMode ecmaMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* baseObj = base.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (!baseObj)
        return false;
    bool couldDelete = baseObj->methodTable(vm)->deleteProperty(baseObj, globalObject, ident, slot);
    RETURN_IF_EXCEPTION(scope, false);
    if (!couldDelete && ecmaMode.isStrict())
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
    return couldDelete;
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdOptimize, size_t, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue baseValue = JSValue::decode(encodedBase);

    DeletePropertySlot slot;
    Structure* oldStructure = baseValue.structureOrNull(vm);

    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());

    bool result = deleteById(globalObject, vm, slot, baseValue, ident, ecmaMode);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (baseValue.isObject()) {
        if (!parseIndex(ident)) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
                repatchDeleteBy(globalObject, codeBlock, slot, baseValue, oldStructure, identifier, *stubInfo, DelByKind::Normal, ecmaMode);
        }
    }

    return result;
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByIdGeneric, size_t, (JSGlobalObject* globalObject, StructureStubInfo*, EncodedJSValue encodedBase, uintptr_t rawCacheableIdentifier, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    CacheableIdentifier identifier = CacheableIdentifier::createFromRawBits(rawCacheableIdentifier);
    Identifier ident = Identifier::fromUid(vm, identifier.uid());
    DeletePropertySlot slot;
    return deleteById(globalObject, vm, slot, JSValue::decode(encodedBase), ident, ecmaMode);
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
        couldDelete = baseObj->methodTable(vm)->deletePropertyByIndex(baseObj, globalObject, index);
    else {
        Identifier property = key.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        couldDelete = baseObj->methodTable(vm)->deleteProperty(baseObj, globalObject, property, slot);
    }
    RETURN_IF_EXCEPTION(scope, false);
    if (!couldDelete && ecmaMode.isStrict())
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
    return couldDelete;
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValOptimize, size_t, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    DeletePropertySlot slot;
    Structure* oldStructure = baseValue.structureOrNull(vm);

    bool result = deleteByVal(globalObject, vm, slot, baseValue, subscript, ecmaMode);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (baseValue.isObject() && CacheableIdentifier::isCacheableIdentifierCell(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        if (subscript.isSymbol() || !parseIndex(propertyName)) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            CacheableIdentifier identifier = CacheableIdentifier::createFromCell(subscript.asCell());
            if (stubInfo->considerCachingBy(vm, codeBlock, baseValue.structureOrNull(vm), identifier))
                repatchDeleteBy(globalObject, codeBlock, slot, baseValue, oldStructure, identifier, *stubInfo, DelByKind::NormalByVal, ecmaMode);
        }
    }

    return result;
}

JSC_DEFINE_JIT_OPERATION(operationDeleteByValGeneric, size_t, (JSGlobalObject* globalObject, StructureStubInfo*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ECMAMode ecmaMode))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    DeletePropertySlot slot;
    return deleteByVal(globalObject, vm, slot, JSValue::decode(encodedBase), JSValue::decode(encodedSubscript), ecmaMode);
}

JSC_DEFINE_JIT_OPERATION(operationPushWithScope, JSCell*, (JSGlobalObject* globalObject, JSCell* currentScopeCell, EncodedJSValue objectValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = JSValue::decode(objectValue).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSScope* currentScope = jsCast<JSScope*>(currentScopeCell);

    return JSWithScope::create(vm, globalObject, currentScope, object);
}

JSC_DEFINE_JIT_OPERATION(operationPushWithScopeObject, JSCell*, (JSGlobalObject* globalObject, JSCell* currentScopeCell, JSObject* object))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSScope* currentScope = jsCast<JSScope*>(currentScopeCell);
    return JSWithScope::create(vm, globalObject, currentScope, object);
}

JSC_DEFINE_JIT_OPERATION(operationInstanceOfGeneric, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedProto))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue value = JSValue::decode(encodedValue);
    JSValue proto = JSValue::decode(encodedProto);
    
    stubInfo->tookSlowPath = true;
    
    bool result = JSObject::defaultHasInstance(globalObject, value, proto);
    return JSValue::encode(jsBoolean(result));
}

JSC_DEFINE_JIT_OPERATION(operationInstanceOfOptimize, EncodedJSValue, (JSGlobalObject* globalObject, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedProto))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue value = JSValue::decode(encodedValue);
    JSValue proto = JSValue::decode(encodedProto);
    
    bool result = JSObject::defaultHasInstance(globalObject, value, proto);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));
    
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (stubInfo->considerCachingGeneric(vm, codeBlock, value.structureOrNull(vm)))
        repatchInstanceOf(globalObject, codeBlock, value, proto, *stubInfo, result);
    
    return JSValue::encode(jsBoolean(result));
}

JSC_DEFINE_JIT_OPERATION(operationSizeFrameForForwardArguments, int32_t, (JSGlobalObject* globalObject, EncodedJSValue, int32_t numUsedStackSlots, int32_t))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return sizeFrameForForwardArguments(globalObject, callFrame, vm, numUsedStackSlots);
}

JSC_DEFINE_JIT_OPERATION(operationSizeFrameForVarargs, int32_t, (JSGlobalObject* globalObject, EncodedJSValue encodedArguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue arguments = JSValue::decode(encodedArguments);
    return sizeFrameForVarargs(globalObject, callFrame, vm, arguments, numUsedStackSlots, firstVarArgOffset);
}

JSC_DEFINE_JIT_OPERATION(operationSetupForwardArgumentsFrame, CallFrame*, (JSGlobalObject* globalObject, CallFrame* newCallFrame, EncodedJSValue, int32_t, int32_t length))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    setupForwardArgumentsFrame(globalObject, callFrame, newCallFrame, length);
    return newCallFrame;
}

JSC_DEFINE_JIT_OPERATION(operationSetupVarargsFrame, CallFrame*, (JSGlobalObject* globalObject, CallFrame* newCallFrame, EncodedJSValue encodedArguments, int32_t firstVarArgOffset, int32_t length))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue arguments = JSValue::decode(encodedArguments);
    setupVarargsFrame(globalObject, callFrame, newCallFrame, arguments, firstVarArgOffset, length);
    return newCallFrame;
}

JSC_DEFINE_JIT_OPERATION(operationSwitchCharWithUnknownKeyType, char*, (JSGlobalObject* globalObject, EncodedJSValue encodedKey, size_t tableIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = callFrame->codeBlock();

    SimpleJumpTable& jumpTable = codeBlock->switchJumpTable(tableIndex);
    void* result = jumpTable.ctiDefault.executableAddress();

    if (key.isString()) {
        JSString* string = asString(key);
        if (string->length() == 1) {
            String value = string->value(globalObject);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
            result = jumpTable.ctiForValue(value[0]).executableAddress();
        }
    }

    assertIsTaggedWith(result, JSSwitchPtrTag);
    return reinterpret_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationSwitchImmWithUnknownKeyType, char*, (VM* vmPointer, EncodedJSValue encodedKey, size_t tableIndex))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = callFrame->codeBlock();

    SimpleJumpTable& jumpTable = codeBlock->switchJumpTable(tableIndex);
    void* result;
    if (key.isInt32())
        result = jumpTable.ctiForValue(key.asInt32()).executableAddress();
    else if (key.isDouble() && key.asDouble() == static_cast<int32_t>(key.asDouble()))
        result = jumpTable.ctiForValue(static_cast<int32_t>(key.asDouble())).executableAddress();
    else
        result = jumpTable.ctiDefault.executableAddress();
    assertIsTaggedWith(result, JSSwitchPtrTag);
    return reinterpret_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationSwitchStringWithUnknownKeyType, char*, (JSGlobalObject* globalObject, EncodedJSValue encodedKey, size_t tableIndex))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = callFrame->codeBlock();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    void* result;
    StringJumpTable& jumpTable = codeBlock->stringSwitchJumpTable(tableIndex);

    if (key.isString()) {
        StringImpl* value = asString(key)->value(globalObject).impl();

        RETURN_IF_EXCEPTION(throwScope, nullptr);

        result = jumpTable.ctiForValue(value).executableAddress();
    } else
        result = jumpTable.ctiDefault.executableAddress();

    assertIsTaggedWith(result, JSSwitchPtrTag);
    return reinterpret_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationGetFromScope, EncodedJSValue, (JSGlobalObject* globalObject, const Instruction* pc))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();

    auto bytecode = pc->as<OpGetFromScope>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* scope = jsCast<JSObject*>(callFrame->uncheckedR(bytecode.m_scope).jsValue());
    GetPutInfo& getPutInfo = bytecode.metadata(codeBlock).m_getPutInfo;

    // ModuleVar is always converted to ClosureVar for get_from_scope.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    RELEASE_AND_RETURN(throwScope, JSValue::encode(scope->getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            if (getPutInfo.resolveMode() == ThrowIfNotFound)
                throwException(globalObject, throwScope, createUndefinedVariableError(globalObject, ident));
            return jsUndefined();
        }

        JSValue result = JSValue();
        if (scope->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            result = slot.getValue(globalObject, ident);
            if (result == jsTDZValue()) {
                throwException(globalObject, throwScope, createTDZError(globalObject));
                return jsUndefined();
            }
        }

        CommonSlowPaths::tryCacheGetFromScopeGlobal(globalObject, codeBlock, vm, bytecode, scope, slot, ident);

        if (!result)
            return slot.getValue(globalObject, ident);
        return result;
    })));
}

JSC_DEFINE_JIT_OPERATION(operationPutToScope, void, (JSGlobalObject* globalObject, const Instruction* pc))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    CodeBlock* codeBlock = callFrame->codeBlock();
    auto bytecode = pc->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(codeBlock);

    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* scope = jsCast<JSObject*>(callFrame->uncheckedR(bytecode.m_scope).jsValue());
    JSValue value = callFrame->r(bytecode.m_value).jsValue();
    GetPutInfo& getPutInfo = metadata.m_getPutInfo;

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    if (getPutInfo.resolveType() == LocalClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(scope);
        environment->variableAt(ScopeOffset(metadata.m_operand)).set(vm, environment, value);
        if (WatchpointSet* set = metadata.m_watchpointSet)
            set->touch(vm, "Executed op_put_scope<LocalClosureVar>");
        return;
    }

    bool hasProperty = scope->hasProperty(globalObject, ident);
    RETURN_IF_EXCEPTION(throwScope, void());
    if (hasProperty
        && scope->isGlobalLexicalEnvironment()
        && !isInitialization(getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, globalObject, ident, slot);
        if (slot.getValue(globalObject, ident) == jsTDZValue()) {
            throwException(globalObject, throwScope, createTDZError(globalObject));
            return;
        }
    }

    if (getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty) {
        throwException(globalObject, throwScope, createUndefinedVariableError(globalObject, ident));
        return;
    }

    PutPropertySlot slot(scope, getPutInfo.ecmaMode().isStrict(), PutPropertySlot::UnknownContext, isInitialization(getPutInfo.initializationMode()));
    scope->methodTable(vm)->put(scope, globalObject, ident, value, slot);
    
    RETURN_IF_EXCEPTION(throwScope, void());

    CommonSlowPaths::tryCachePutToScopeGlobal(globalObject, codeBlock, bytecode, scope, slot, ident);
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
}

JSC_DEFINE_JIT_OPERATION(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity, char*, (VM* vmPointer, JSObject* object))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(!object->structure(vm)->outOfLineCapacity());
    Butterfly* result = object->allocateMoreOutOfLineStorage(vm, 0, initialOutOfLineCapacity);
    object->nukeStructureAndSetButterfly(vm, object->structureID(), result);
    return reinterpret_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationReallocateButterflyToGrowPropertyStorage, char*, (VM* vmPointer, JSObject* object, size_t newSize))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    Butterfly* result = object->allocateMoreOutOfLineStorage(vm, object->structure(vm)->outOfLineCapacity(), newSize);
    object->nukeStructureAndSetButterfly(vm, object->structureID(), result);
    return reinterpret_cast<char*>(result);
}

JSC_DEFINE_JIT_OPERATION(operationOSRWriteBarrier, void, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    vm.heap.writeBarrier(cell);
}

JSC_DEFINE_JIT_OPERATION(operationWriteBarrierSlowPath, void, (VM* vmPointer, JSCell* cell))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    vm.heap.writeBarrierSlowPath(cell);
}

JSC_DEFINE_JIT_OPERATION(operationLookupExceptionHandler, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
    ASSERT(vm.targetMachinePCForThrow);
}

JSC_DEFINE_JIT_OPERATION(operationLookupExceptionHandlerFromCallerFrame, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    ASSERT(callFrame->isStackOverflowFrame());
    ASSERT(jsCast<ErrorInstance*>(vm.exceptionForInspection()->value().asCell())->isStackOverflowError());
    genericUnwind(vm, callFrame);
    ASSERT(vm.targetMachinePCForThrow);
}

JSC_DEFINE_JIT_OPERATION(operationVMHandleException, void, (VM* vmPointer))
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
JSC_DEFINE_JIT_OPERATION(operationExceptionFuzz, void, (JSGlobalObject* globalObject))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);
#if COMPILER(GCC_COMPATIBLE)
    void* returnPC = __builtin_return_address(0);
    doExceptionFuzzing(globalObject, scope, "JITOperations", returnPC);
#endif // COMPILER(GCC_COMPATIBLE)
}

JSC_DEFINE_JIT_OPERATION(operationExceptionFuzzWithCallFrame, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);
#if COMPILER(GCC_COMPATIBLE)
    void* returnPC = __builtin_return_address(0);
    doExceptionFuzzing(callFrame->lexicalGlobalObject(vm), scope, "JITOperations", returnPC);
#endif // COMPILER(GCC_COMPATIBLE)
}

JSC_DEFINE_JIT_OPERATION(operationValueAdd, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(jsAdd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2)));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile* arithProfile))
{
    ASSERT(arithProfile);
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(profiledAdd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2), *arithProfile));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC* addIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
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

    return JSValue::encode(result);
}

JSC_DEFINE_JIT_OPERATION(operationValueAddProfiledNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC* addIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    BinaryArithProfile* arithProfile = addIC->arithProfile();
    ASSERT(arithProfile);
    return JSValue::encode(profiledAdd(globalObject, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2), *arithProfile));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC* addIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    auto nonOptimizeVariant = operationValueAddNoOptimize;
    if (BinaryArithProfile* arithProfile = addIC->arithProfile())
        arithProfile->observeLHSAndRHS(op1, op2);
    addIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    return JSValue::encode(jsAdd(globalObject, op1, op2));
}

JSC_DEFINE_JIT_OPERATION(operationValueAddNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    JSValue result = jsAdd(globalObject, op1, op2);

    return JSValue::encode(result);
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

    return unprofiledMul(globalObject, encodedOp1, encodedOp2);
}

JSC_DEFINE_JIT_OPERATION(operationValueMulNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return unprofiledMul(globalObject, encodedOp1, encodedOp2);
}

JSC_DEFINE_JIT_OPERATION(operationValueMulOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC* mulIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto nonOptimizeVariant = operationValueMulNoOptimize;
    if (BinaryArithProfile* arithProfile = mulIC->arithProfile())
        arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    mulIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    return unprofiledMul(globalObject, encodedOp1, encodedOp2);
}

JSC_DEFINE_JIT_OPERATION(operationValueMulProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile* arithProfile))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    ASSERT(arithProfile);
    return profiledMul(globalObject, encodedOp1, encodedOp2, *arithProfile);
}

JSC_DEFINE_JIT_OPERATION(operationValueMulProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC* mulIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    BinaryArithProfile* arithProfile = mulIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    auto nonOptimizeVariant = operationValueMulProfiledNoOptimize;
    mulIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    return profiledMul(globalObject, encodedOp1, encodedOp2, *arithProfile, false);
}

JSC_DEFINE_JIT_OPERATION(operationValueMulProfiledNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC* mulIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    BinaryArithProfile* arithProfile = mulIC->arithProfile();
    ASSERT(arithProfile);
    return profiledMul(globalObject, encodedOp1, encodedOp2, *arithProfile);
}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegate, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue operand = JSValue::decode(encodedOperand);

    JSValue primValue = operand.toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    if (primValue.isBigInt32())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32())));
#endif
    if (primValue.isHeapBigInt())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt())));

    double number = primValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(-number));

}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegateProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand, UnaryArithProfile* arithProfile))
{
    ASSERT(arithProfile);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue operand = JSValue::decode(encodedOperand);
    arithProfile->observeArg(operand);

    JSValue primValue = operand.toPrimitive(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    if (primValue.isBigInt32()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32());
        RETURN_IF_EXCEPTION(scope, { });
        arithProfile->observeResult(result);
        return JSValue::encode(result);
    }
#endif
    if (primValue.isHeapBigInt()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt());
        RETURN_IF_EXCEPTION(scope, { });
        arithProfile->observeResult(result);
        return JSValue::encode(result);
    }

    double number = primValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSValue result = jsNumber(-number);
    arithProfile->observeResult(result);
    return JSValue::encode(result);
}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegateProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand, JITNegIC* negIC))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    JSValue operand = JSValue::decode(encodedOperand);

    UnaryArithProfile* arithProfile = negIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeArg(operand);
    negIC->generateOutOfLine(callFrame->codeBlock(), operationArithNegateProfiled);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif
    
    JSValue primValue = operand.toPrimitive(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    if (primValue.isBigInt32()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32());
        RETURN_IF_EXCEPTION(scope, { });
        arithProfile->observeResult(result);
        return JSValue::encode(result);
    }
#endif
    if (primValue.isHeapBigInt()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt());
        RETURN_IF_EXCEPTION(scope, { });
        arithProfile->observeResult(result);
        return JSValue::encode(result);
    }

    double number = primValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSValue result = jsNumber(-number);
    arithProfile->observeResult(result);
    return JSValue::encode(result);
}

// FIXME: it would be better to call those operationValueNegate, since the operand can be a BigInt
JSC_DEFINE_JIT_OPERATION(operationArithNegateOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOperand, JITNegIC* negIC))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    JSValue operand = JSValue::decode(encodedOperand);

    if (UnaryArithProfile* arithProfile = negIC->arithProfile())
        arithProfile->observeArg(operand);
    negIC->generateOutOfLine(callFrame->codeBlock(), operationArithNegate);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    JSValue primValue = operand.toPrimitive(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if USE(BIGINT32)
    // FIXME: why does this function profile the argument but not the result?
    if (primValue.isBigInt32())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32())));
#endif
    if (primValue.isHeapBigInt())
        RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt())));

    double number = primValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(-number));
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
    return unprofiledSub(globalObject, encodedOp1, encodedOp2);
}

JSC_DEFINE_JIT_OPERATION(operationValueSubProfiled, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile* arithProfile))
{
    ASSERT(arithProfile);

    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return profiledSub(vm, globalObject, encodedOp1, encodedOp2, *arithProfile);
}

JSC_DEFINE_JIT_OPERATION(operationValueSubOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC* subIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    auto nonOptimizeVariant = operationValueSubNoOptimize;
    if (BinaryArithProfile* arithProfile = subIC->arithProfile())
        arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    subIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    return unprofiledSub(globalObject, encodedOp1, encodedOp2);
}

JSC_DEFINE_JIT_OPERATION(operationValueSubNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    return unprofiledSub(globalObject, encodedOp1, encodedOp2);
}

JSC_DEFINE_JIT_OPERATION(operationValueSubProfiledOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC* subIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    BinaryArithProfile* arithProfile = subIC->arithProfile();
    ASSERT(arithProfile);
    arithProfile->observeLHSAndRHS(JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
    auto nonOptimizeVariant = operationValueSubProfiledNoOptimize;
    subIC->generateOutOfLine(callFrame->codeBlock(), nonOptimizeVariant);

#if ENABLE(MATH_IC_STATS)
    callFrame->codeBlock()->dumpMathICStats();
#endif

    return profiledSub(vm, globalObject, encodedOp1, encodedOp2, *arithProfile, false);
}

JSC_DEFINE_JIT_OPERATION(operationValueSubProfiledNoOptimize, EncodedJSValue, (JSGlobalObject* globalObject, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC* subIC))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    BinaryArithProfile* arithProfile = subIC->arithProfile();
    ASSERT(arithProfile);
    return profiledSub(vm, globalObject, encodedOp1, encodedOp2, *arithProfile);
}

JSC_DEFINE_JIT_OPERATION(operationProcessTypeProfilerLog, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    vm.typeProfilerLog()->processLogEntries(vm, "Log Full, called from inside baseline JIT"_s);
}

JSC_DEFINE_JIT_OPERATION(operationProcessShadowChickenLog, void, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    RELEASE_ASSERT(vm.shadowChicken());
    vm.shadowChicken()->update(vm, callFrame);
}

JSC_DEFINE_JIT_OPERATION(operationCheckIfExceptionIsUncatchableAndNotifyProfiler, int32_t, (VM* vmPointer))
{
    VM& vm = *vmPointer;
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(!!scope.exception());

    if (isTerminatedExecutionException(vm, scope.exception())) {
        genericUnwind(vm, callFrame);
        return 1;
    }
    return 0;
}

} // extern "C"

} // namespace JSC

IGNORE_WARNINGS_END

#endif // ENABLE(JIT)
